#ifndef SIL_MAIN_SDL_IOS_H
#define SIL_MAIN_SDL_IOS_H

#if defined(SIL_IOS)

#include <stdbool.h>
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the underlying UIWindow's safe area insets in window units (points).
 *
 * iOS pads horizontal safe area insets symmetrically in landscape on notched
 * devices, leaving an unused band opposite the actual sensor housing. This
 * helper reads the raw insets and, when it detects symmetric padding in
 * landscape, zeroes the side that has no real hardware obstruction (using the
 * current interface orientation to identify the notch side). On portrait,
 * iPad, or already-asymmetric layouts, the values pass through unchanged.
 *
 * Returns false if the window's UIWindow could not be resolved. */
bool sdl_ios_get_safe_area_insets(SDL_Window* window,
    int* out_left, int* out_right, int* out_top, int* out_bottom);

/* Installs a one-time observer for device orientation changes. On rotation,
 * pushes an SDL_EVENT_WINDOW_SAFE_AREA_CHANGED event so the main loop will
 * re-query the safe area and re-resolve the asymmetric override above.
 *
 * Necessary because iOS reports symmetric horizontal insets in landscape, so
 * rotating between LandscapeLeft and LandscapeRight does not change the inset
 * values, hence safeAreaInsetsDidChange does not fire and SDL never re-asks. */
void sdl_ios_install_orientation_observer(SDL_Window* window);

#ifdef __cplusplus
}
#endif

#endif /* SIL_IOS */

#endif /* SIL_MAIN_SDL_IOS_H */
