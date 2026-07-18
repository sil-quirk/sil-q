#include "main-sdl-ios.h"

#if defined(SIL_IOS)

#import <UIKit/UIKit.h>
#include <math.h>

bool sdl_ios_get_safe_area_insets(SDL_Window* window,
    int* out_left, int* out_right, int* out_top, int* out_bottom)
{
    if (!window || !out_left || !out_right || !out_top || !out_bottom)
        return false;

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (!props)
        return false;

    UIWindow* uiwin = (__bridge UIWindow*)SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (!uiwin)
        return false;

    /* Read the root view's insets — they reflect the rotated coordinate space
     * the game actually draws into (the UIWindow's insets can lag behind). */
    UIView* root_view = uiwin.rootViewController ? uiwin.rootViewController.view : nil;
    UIEdgeInsets insets = root_view ? root_view.safeAreaInsets : uiwin.safeAreaInsets;

    UIInterfaceOrientation orientation = UIInterfaceOrientationUnknown;
    UIWindowScene* scene = uiwin.windowScene;
    if (scene)
        orientation = scene.interfaceOrientation;

    int left = (int)ceil(insets.left);
    int right = (int)ceil(insets.right);
    int top = (int)ceil(insets.top);
    int bottom = (int)ceil(insets.bottom);

    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;

    /* In landscape on notched iPhones, iOS reports symmetric horizontal insets
     * even though the sensor housing is only on one edge. When we see equal
     * (or opposite-side larger) insets, drop the one that doesn't correspond
     * to the actual notch side.
     *
     * UIInterfaceOrientationLandscape{Left,Right} describes the rotation FROM
     * portrait — the notch ends up on the OPPOSITE side from the suffix:
     *   LandscapeLeft  (raw 4) -> home button on right -> notch on RIGHT
     *   LandscapeRight (raw 3) -> home button on left  -> notch on LEFT */
    if (orientation == UIInterfaceOrientationLandscapeRight) {
        /* Notch on the LEFT edge; strip the symmetric padding on the right. */
        if (right > 0 && right >= left)
            right = 0;
    } else if (orientation == UIInterfaceOrientationLandscapeLeft) {
        /* Notch on the RIGHT edge; strip the symmetric padding on the left. */
        if (left > 0 && left >= right)
            left = 0;
    }

    *out_left = left;
    *out_right = right;
    *out_top = top;
    *out_bottom = bottom;
    return true;
}

static bool g_observer_installed = false;
static SDL_Window* g_observer_window = NULL;

void sdl_ios_install_orientation_observer(SDL_Window* window)
{
    g_observer_window = window;

    if (g_observer_installed)
        return;
    g_observer_installed = true;

    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIDeviceOrientationDidChangeNotification
        object:nil
        queue:[NSOperationQueue mainQueue]
        usingBlock:^(NSNotification* note) {
            (void)note;
            UIDeviceOrientation o = [UIDevice currentDevice].orientation;
            if (!UIDeviceOrientationIsPortrait(o)
                && !UIDeviceOrientationIsLandscape(o))
                return;

            /* Defer to the next main-runloop tick so windowScene's
             * interfaceOrientation has caught up before we re-query. */
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!g_observer_window)
                    return;
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = SDL_EVENT_WINDOW_SAFE_AREA_CHANGED;
                ev.window.windowID = SDL_GetWindowID(g_observer_window);
                SDL_PushEvent(&ev);
            });
        }];
}

void sdl_ios_request_orientation(bool portrait)
{
    UIInterfaceOrientationMask mask = portrait
        ? UIInterfaceOrientationMaskPortrait
        : UIInterfaceOrientationMaskLandscape;

    dispatch_async(dispatch_get_main_queue(), ^{
        UIWindow* uiwin = nil;
        UIViewController* root = nil;
        UIWindowScene* scene = nil;

        if (g_observer_window) {
            SDL_PropertiesID props = SDL_GetWindowProperties(g_observer_window);

            if (props) {
                uiwin = (__bridge UIWindow*)SDL_GetPointerProperty(props,
                    SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
            }
        }
        if (uiwin) {
            root = uiwin.rootViewController;
            scene = uiwin.windowScene;
        }

        if (@available(iOS 16.0, *)) {
            if (scene) {
                UIWindowSceneGeometryPreferencesIOS* preferences =
                    [[UIWindowSceneGeometryPreferencesIOS alloc]
                        initWithInterfaceOrientations:mask];
                [scene requestGeometryUpdateWithPreferences:preferences
                    errorHandler:^(NSError* error) {
                        NSLog(@"Sil-More orientation request failed: %@", error);
                    }];
                return;
            }
        }

        /* The older public API cannot force an orientation.  It does cause
         * UIKit to re-check SDL's hint-derived supported-orientation mask. */
        if (root) {
            if ([root respondsToSelector:
                    @selector(setNeedsUpdateOfSupportedInterfaceOrientations)])
            {
                [root setNeedsUpdateOfSupportedInterfaceOrientations];
            }
            [UIViewController attemptRotationToDeviceOrientation];
        }
    });
}

#endif /* SIL_IOS */
