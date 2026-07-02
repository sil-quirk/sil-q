/* File: cave-visuals.c */

#include "cave-internal.h"

/*
 * Multi-hued monsters shimmer according to their base colour.
 */
static byte multi_hued_attr(monster_race* r_ptr)
{
    /* Monsters with an attr other than 'w' choose colors according to attr */
    if (r_ptr->d_attr != TERM_WHITE)
    {
        if ((r_ptr->d_attr == TERM_RED) || (r_ptr->d_attr == TERM_L_RED))
            return ((one_in_(2)) ? TERM_RED : TERM_L_RED);
        if ((r_ptr->d_attr == TERM_BLUE) || (r_ptr->d_attr == TERM_L_BLUE))
            return ((one_in_(2)) ? TERM_BLUE : TERM_L_BLUE);
        if ((r_ptr->d_attr == TERM_WHITE) || (r_ptr->d_attr == TERM_L_WHITE))
            return ((one_in_(2)) ? TERM_WHITE : TERM_L_WHITE);
        if ((r_ptr->d_attr == TERM_GREEN) || (r_ptr->d_attr == TERM_L_GREEN))
            return ((one_in_(2)) ? TERM_GREEN : TERM_L_GREEN);
        if ((r_ptr->d_attr == TERM_UMBER) || (r_ptr->d_attr == TERM_L_UMBER))
            return ((one_in_(2)) ? TERM_UMBER : TERM_L_UMBER);
        if ((r_ptr->d_attr == TERM_ORANGE) || (r_ptr->d_attr == TERM_YELLOW))
            return ((one_in_(2)) ? TERM_ORANGE : TERM_YELLOW);
        if ((r_ptr->d_attr == TERM_L_DARK) || (r_ptr->d_attr == TERM_SLATE))
            return ((one_in_(2)) ? TERM_L_DARK : TERM_SLATE);
        if ((r_ptr->d_attr == TERM_VIOLET)
            || (r_ptr->d_attr == TERM_VIOLET + TERM_SHADE))
            return ((one_in_(2)) ? TERM_VIOLET : TERM_VIOLET + TERM_SHADE);
    }

    /* Otherwise can be any color */
    return (dieroll(15));
}

/*
 * Hack -- Legal monster codes
 */
static const char image_monster_hack[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/*
 * Hack -- Hallucinatory monster
 */
static u16b image_monster(void)
{
    byte a;
    char c;

    /* Random symbol from set above (not including final nul) */
    c = image_monster_hack[rand_int(sizeof(image_monster_hack) - 1)];

    /* Random color */
    a = dieroll(15);

    /* Encode */
    return (PICT(a, c));
}

/*
 * Hack -- Legal object codes
 */
static const char image_object_hack[] = "?/|\\\"!$()_-=[]{},~"; /* " */

/*
 * Hack -- Hallucinatory object
 */
static u16b image_object(void)
{
    byte a;
    char c;

    /* Random symbol from set above (not including final nul) */
    c = image_object_hack[rand_int(sizeof(image_object_hack) - 1)];

    /* Random color */
    a = dieroll(15);

    /* Encode */
    return (PICT(a, c));
}

/*
 * Hack -- Random hallucination
 */
static u16b image_random(void)
{
    /* Normally, assume monsters */
    if (percent_chance(75))
    {
        return (image_monster());
    }

    /* Otherwise, assume objects */
    else
    {
        return (image_object());
    }
}

/*
 * The 16x16 tile of the terrain supports lighting
 */
bool feat_supports_lighting(int feat)
{
    /* Pseudo graphics don't support lighting */
    if (use_graphics == GRAPHICS_PSEUDO)
        return false;

    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
    {
        return true;
    }

    switch (feat)
    {
    case FEAT_FLOOR:
    case FEAT_SECRET:
    case FEAT_QUARTZ:
    case FEAT_WALL_EXTRA:
    case FEAT_WALL_INNER:
    case FEAT_WALL_OUTER:
    case FEAT_WALL_SOLID:
    case FEAT_WALL_PERM:
        return true;
    default:
        return false;
    }
}

static byte darken(byte a, byte c)
{
    // don't darken the symbols for traps
    if (c == '^')
        return (a);

    // or chasms or shafts
    if (((c == '%') || (c == '>') || (c == '<')) && a == TERM_L_DARK)
        return (a);

    if (a == TERM_WHITE)
        return (TERM_SLATE);
    if (a == TERM_L_WHITE)
        return (TERM_L_DARK);
    if (a == TERM_SLATE)
        return (TERM_L_DARK);
    if (a == TERM_L_DARK)
        return (TERM_DARK + TERM_SHADE);
    if (a == TERM_L_UMBER)
        return (TERM_UMBER);
    if (a == TERM_L_BLUE)
        return (TERM_BLUE);
    if (a == TERM_L_GREEN)
        return (TERM_GREEN);
    if (a == TERM_L_WHITE + TERM_SHADE)
        return (TERM_L_DARK + TERM_SHADE);

    return (a);
}

void cave_feature_visual(const feature_type* f_ptr, byte* a, char* c)
{
    if (graphics_are_ascii())
    {
        *a = f_ptr->d_attr;
        *c = f_ptr->d_char;
    }
    else
    {
        *a = f_ptr->x_attr;
        *c = f_ptr->x_char;
    }
}

void cave_monster_visual(const monster_race* r_ptr, byte* a, char* c)
{
    if (graphics_are_ascii())
    {
        *a = r_ptr->d_attr;
        *c = r_ptr->d_char;
    }
    else
    {
        *a = r_ptr->x_attr;
        *c = r_ptr->x_char;
    }
}

static void special_lighting_floor(byte* a, char* c, int info, int light)
{
    /* A currently seen floor always uses its normal visible appearance.
     * CAVE_SEEN already incorporates line of sight and illumination. */
    if (info & CAVE_SEEN)
        return;

    /* Determine if this grid should appear dark because of blindness or lack of light. */
    bool is_dark = false;

    if (p_ptr->blind)
    {
        is_dark = true;
    }
    else
    {
        /* Treat as dark when there's no positive light and the grid isn't sunlit. */
        if (light <= 0 && !(info & (CAVE_GLOW)))
            is_dark = true;
    }

    switch (use_graphics)
    {
    case GRAPHICS_NONE:
    case GRAPHICS_PSEUDO:
        /* In ASCII modes, darken for truly dark grids; also dim unseen grids. */
        if (is_dark)
        {
            *a = TERM_DARK + TERM_SHADE;
        }
        else if (!(info & (CAVE_SEEN)))
        {
            *a = darken(*a, *c);
        }
        break;

    case GRAPHICS_MICROCHASM:
        /* In tiles, use the darker variant for dark or unseen grids. */
        if (is_dark || !(info & (CAVE_SEEN)))
        {
            *c += 1;
        }
        break;
    }
}

static void special_lighting_wall(byte* a, char* c, int feat, int info, int light)
{
    /* Determine brightness based on blindness and dynamic light (torch/lantern) */
    bool is_dark = false;

    if (p_ptr->blind)
    {
        is_dark = true;
    }
    else
    {
        /* If there's no positive light on this grid and it's not sunlit, treat as dark */
        if (light <= 0 && !(info & (CAVE_GLOW)))
            is_dark = true;
    }

    switch (use_graphics)
    {
    case GRAPHICS_NONE:
    case GRAPHICS_PSEUDO:
        if (is_dark)
        {
            /* darken the colour */
            *a = darken(*a, *c);
        }
        break;
    case GRAPHICS_MICROCHASM:
        if (feat_supports_lighting(feat)
            && (is_dark
                || (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
                    && !(info & (CAVE_SEEN)))))
        {
            /* use darker tile variant */
            *c += 1;
        }
        break;
    }

    if (use_background_colors)
    {
        switch (use_graphics)
        {
        case GRAPHICS_NONE:
        case GRAPHICS_PSEUDO:
        {
            if (hybrid_walls && ((*c == '#') || (*c == '%')))
            {
                *a = *a + (MAX_COLORS * BG_DARK);
            }
            else if (solid_walls && ((*c == '#') || (*c == '%')))
            {
                *a = *a + (MAX_COLORS * BG_SAME);
            }
        }
        break;
        }
    }
}

/*
 * Group-aware floor and door graphics (extensible)
 * These helpers allow selecting alternative tiles for floors and doors
 * based on the current level's group color (cave_color). If no specific
 * override exists for a group, they return false and the caller keeps
 * the default feature tiles. This provides a safe default while enabling
 * future per-group customization for floors and doors.
 */
static bool is_door_feat(int feat)
{
    /* Open / broken doors */
    if (feat == FEAT_OPEN || feat == FEAT_BROKEN) return true;
    /* Closed/locked/jammed doors */
    if ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL)) return true;
    return false;
}

static bool apply_style_floor_graphics(int y, int x, int feat, int info, byte* a, char* c)
{
    /* Only consider non-ASCII graphics; ASCII uses chars/colors directly */
    if (graphics_are_ascii()) return false;

    /* Safety: only floors here */
    if (feat != FEAT_FLOOR && feat != FEAT_RAGE_FLOOR && feat != FEAT_SUNLIGHT)
        return false;

    /* Respect per-cell color selection; 0/1/2 are defaults/legacy/vault */
    byte color_value = cave_color[y][x];

    /* Use style from absolute override or group */
    int sidx = cave_style_index_for_color(color_value);
    if (sidx >= 0)
    {
        sidx = cave_hallucination_style_for_display(sidx);
        style_type* s = &style_info[sidx];
        /* Halo can force variant 0 via color flag */
        byte choice = 0;
        if (!cave_style_color_force_first_variant(color_value) && s->floor_count > 1) {
            choice = cave_style_floor_choice(sidx);
        }
        if (s->floor_count > 0 && choice >= s->floor_count)
            choice = 0;
        byte fr = (s->floor_count > 0) ? s->floor_rowv[choice] : s->floor_row;
        byte fc = (s->floor_count > 0) ? s->floor_colv[choice] : s->floor_col;
    *a = (byte)(fr | 0x80);
    *c = (char)(fc | 0x80);
        /* Let special_lighting_floor() adjust brightness afterwards */
        return true;
    }

    (void)info; (void)a; (void)c; (void)y; (void)x; (void)feat;
    return false;
}

static bool apply_style_door_graphics(int y, int x, int feat, int info, byte* a, char* c)
{
    /* Only consider non-ASCII graphics; ASCII uses chars/colors directly */
    if (graphics_are_ascii()) return false;

    if (!is_door_feat(feat)) return false;

    /* Respect per-cell color selection; 0/1/2 are defaults/legacy/vault */
    byte color_value = cave_color[y][x];

    /* Resolve style from absolute override or group selection */
    int sidx = cave_style_index_for_color(color_value);
    if (sidx >= 0)
    {
        sidx = cave_hallucination_style_for_display(sidx);
        style_type* s = &style_info[sidx];
        /* Respect first-variant override (if ever used for doors) */
        byte choice = 0;
        if (!cave_style_color_force_first_variant(color_value) && s->door_count > 1) {
            choice = cave_style_door_choice(sidx);
        }
        if (s->door_count > 0 && choice >= s->door_count)
            choice = 0;
        int row = (s->door_count > 0) ? s->door_rowv[choice] : s->door_row;
        int col = (s->door_count > 0) ? s->door_colv[choice] : s->door_col;
        if (feat == FEAT_OPEN) col += 1; else if (feat == FEAT_BROKEN) col += 2;
    *a = (byte)(row | 0x80);
    *c = (char)(col | 0x80);
        return true;
    }

    (void)info; (void)feat; (void)a; (void)c; (void)y; (void)x;
    return false;
}

int player_tile_offset()
{
    object_type * main_wield_ptr = &inventory[INVEN_WIELD];
    object_type * secondary_wield_ptr = &inventory[INVEN_ARM];
    int active_quiver_slot;

    if (player_active_weapon_is_ranged())
    {
        active_quiver_slot = player_active_weapon_quiver_slot();

        if (!active_quiver_slot || !inventory[active_quiver_slot].k_idx)
        {
            return 0;
        }
        if (player_can_treat_as_throwing(&inventory[active_quiver_slot]))
        {
            main_wield_ptr = &inventory[active_quiver_slot];
            secondary_wield_ptr = NULL;
        }
        else if (inventory[active_quiver_slot].tval == TV_ARROW &&
            inventory[INVEN_BOW].tval == TV_BOW)
        {
            return 15;
        }
        else
        {
            return 0;
        }
    }

    byte main_type = main_wield_ptr->tval;
    byte main_subtype = main_wield_ptr->sval;

    byte secondary_type = secondary_wield_ptr
        ? secondary_wield_ptr->tval : 0;

    if (secondary_type && !main_type)
    {
        main_type = secondary_type;
        main_subtype = secondary_wield_ptr->sval;
        secondary_type = 0;
    }

    bool smallSwordMain =
        (main_type == TV_SWORD && main_subtype == SV_DAGGER) ||
        (main_type == TV_SWORD && main_subtype == SV_SHORT_SWORD);
    bool curvedSwordMain =
        (main_type == TV_SWORD && main_subtype == SV_CURVED_SWORD);
    bool bigSwordMain =
        (main_type == TV_SWORD && main_subtype > SV_CURVED_SWORD) ||
        (main_type == TV_DIGGING && main_subtype == SV_SHOVEL);
    bool spearMain =
        (main_type == TV_POLEARM && main_subtype < SV_HAND_AXE);
    bool smallAxeMain =
        (main_type == TV_POLEARM && main_subtype == SV_HAND_AXE) ||
        (main_type == TV_HAFTED && main_subtype == SV_WAR_HAMMER) ||
        (main_type == TV_DIGGING && main_subtype == SV_MATTOCK);
    bool bigAxeMain =
        (main_type == TV_POLEARM && main_subtype > SV_HAND_AXE);
    bool quarterstaffMain =
        (main_type == TV_HAFTED && main_subtype == SV_QUARTERSTAFF);
    bool shieldOffhand =
        (secondary_type == TV_SHIELD);
    bool axeOffhand =
        (secondary_type == TV_POLEARM);
    bool swordOffhand =
        (secondary_type == TV_SWORD);

    if (!secondary_type)
    {
        if (smallSwordMain)
        {
            return 1;
        }
        if (curvedSwordMain)
        {
            return 2;
        }
        if (bigSwordMain)
        {
            return 3;
        }
        if (spearMain)
        {
            return 4;
        }
        if (smallAxeMain)
        {
            return 5;
        }
        if (bigAxeMain)
        {
            return 6;
        }
        if (quarterstaffMain)
        {
            return 7;
        }
    }
    else if (shieldOffhand)
    {
        if (bigAxeMain)
        {
            return 9;
        }
        if (smallAxeMain)
        {
            return 10;
        }
        return 8;
    }
    else if (swordOffhand)
    {
        if (smallAxeMain || bigAxeMain)
        {
            return 13;
        }
        return 11;
    }
    else if (axeOffhand)
    {
        if (smallAxeMain || bigAxeMain)
        {
            return 14;
        }
        return 12;
    }

    return 0;
}


/*
 * Extract the attr/char to display at the given (legal) map location
 *
 * Note that this function, since it is called by "lite_spot()" which
 * is called by "update_view()", is a major efficiency concern.
 *
 * Basically, we examine each "layer" of the world (terrain, objects,
 * monsters/players), from the bottom up, extracting a new attr/char
 * if necessary at each layer, and defaulting to "darkness".  This is
 * not the fastest method, but it is very simple, and it is about as
 * fast as it could be for grids which contain no "marked" objects or
 * "visible" monsters.
 *
 * We apply the effects of hallucination during each layer.  Objects will
 * always appear as random "objects", monsters will always appear as random
 * "monsters", and normal grids occasionally appear as random "monsters" or
 * "objects", but note that these random "monsters" and "objects" are really
 * just "colored ascii symbols" (which may look silly on some machines).
 *
 * The hallucination functions avoid taking any pointers to local variables
 * because some compilers refuse to use registers for any local variables
 * whose address is taken anywhere in the function.
 *
 * As an optimization, we can handle the "player" grid as a special case.
 *
 * Note that the memorization of "objects" and "monsters" is not related
 * to the memorization of "terrain".  This allows the player to memorize
 * the terrain of a grid without memorizing any objects in that grid, and
 * to detect monsters without detecting anything about the terrain of the
 * grid containing the monster.
 *
 * The fact that all interesting "objects" and "terrain features" are
 * memorized as soon as they become visible for the first time means
 * that we only have to check the "CAVE_SEEN" flag for "boring" grids.
 *
 * Note that bizarre things must be done when the "attr" and/or "char"
 * codes have the "high-bit" set, since these values are used to encode
 * various "special" pictures in some versions, and certain situations,
 * such as "multi-hued" or "clear" monsters, cause the attr/char codes
 * to be "scrambled" in various ways.
 *
 * Note that the "zero" entry in the feature/object/monster arrays are
 * used to provide "special" attr/char codes, with "monster zero" being
 * used for the player attr/char, "object zero" being used for the "pile"
 * attr/char, and "feature zero" being used for the "darkness" attr/char.
 *
 * Note the assumption that doing "x_ptr = &x_info[x]" plus a few of
 * "x_ptr->xxx", is quicker than "x_info[x].xxx", even if "x" is a fixed
 * constant.  If this is incorrect then a lot of code should be changed.
 *
 *
 * Some comments on the "terrain" layer...
 *
 * Note that "boring" grids (floors, invisible traps, and any illegal grids)
 * are very different from "interesting" grids (all other terrain features),
 * and the two types of grids are handled completely separately.  The most
 * important distinction is that "boring" grids may or may not be memorized
 * when they are first encountered, and so we must use the "CAVE_SEEN" flag
 * to see if they are "see-able".
 *
 *
 * Some comments on the "terrain" layer (boring grids)...
 *
 * Note that "boring" grids are always drawn using the picture for "empty
 * floors", which is stored in "f_info[FEAT_FLOOR]".  Sometimes, special
 * lighting effects may cause this picture to be modified.
 *
 * Note that "invisible traps" are always displayes exactly like "empty
 * floors", which prevents various forms of "cheating", with no loss of
 * efficiency.  There are still a few ways to "guess" where traps may be
 * located, for example, objects will never fall into a grid containing
 * an invisible trap.  XXX XXX
 *
 * Boring grids are displayed normally while currently "see-able"
 * ("CAVE_SEEN").  A memorized floor ("CAVE_MARK") that remains illuminated
 * outside line of sight is displayed with the dark floor appearance.  A
 * memorized but genuinely unlit floor remains darkness.
 *
 *
 * Some comments on the "terrain" layer (non-boring grids)...
 *
 * Note the use of the "mimic" field in the "terrain feature" processing,
 * which allows any feature to "pretend" to be another feature.  This is
 * used to "hide" secret doors, and to make all "doors" appear the same,
 * and all "walls" appear the same, and "hidden" treasure stay hidden.
 *
 * Since "interesting" grids are always memorized as soon as they become
 * "see-able" by the player ("CAVE_SEEN"), such a grid only needs to be
 * displayed if it is memorized ("CAVE_MARK").  Most "interesting" grids
 * are in fact non-memorized, non-see-able, wall grids, so the fact that
 * we do not have to check the "CAVE_SEEN" flag adds some efficiency, at
 * the cost of *forcing* the memorization of all "interesting" grids when
 * they are first seen.  Since the "CAVE_SEEN" flag is now maintained by
 * the "update_view()" function, this efficiency is not as significant as
 * it was in previous versions, and could perhaps be removed.
 *
 * Note that "wall" grids are more complicated than "boring" grids, due to
 * the fact that "CAVE_GLOW" for a "wall" grid means that the grid *might*
 * be glowing, depending on where the player is standing in relation to the
 * wall.  In particular, the wall of an illuminated room should look just
 * like any other (dark) wall unless the player is actually inside the room.
 *
 * Thus, we do not support as many visual special effects for "wall" grids
 * as we do for "boring" grids, since many of them would give the player
 * information about the "CAVE_GLOW" flag of the wall grid, in particular,
 * it would allow the player to notice the walls of illuminated rooms from
 * a dark hallway that happened to run beside the room.
 *
 *
 * Some comments on the "object" layer...
 *
 * Currently, we do nothing with multi-hued objects, because there are
 * not any.  If there were, they would have to set "shimmer_objects"
 * when they were created, and then new "shimmer" code in "dungeon.c"
 * would have to be created handle the "shimmer" effect, and the code
 * in the cave visual code would have to be updated to create the shimmer effect.
 * This did not seem worth the effort.  XXX XXX
 *
 *
 * Some comments on the "monster"/"player" layer...
 *
 * Note that monsters can have some "special" flags, including "ATTR_MULTI",
 * which means their color changes, and "ATTR_CLEAR", which means they take
 * the color of whatever is under them, and "CHAR_CLEAR", which means that
 * they take the symbol of whatever is under them.  Technically, the flag
 * "CHAR_MULTI" is supposed to indicate that a monster looks strange when
 * examined, but this flag is currently ignored.
 *
 * Normally, players could be handled just like monsters, except that the
 * concept of the "torch lite" of others player would add complications.
 * For efficiency, however, we handle the (only) player first, since the
 * "player" symbol always "pre-empts" any other facts about the grid.
 *
 * ToDo: The transformations for tile colors, or brightness for the 16x16
 * tiles should be handled differently.  One possibility would be to
 * extend feature_type with attr/char definitions for the different states.
 */

#define GRAF_BROKEN_BONE 440

static bool monster_can_see_player_for_stealth_vision(monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->r_idx)
        return false;

    if (monster_race_is_vala(m_ptr->r_idx))
        return true;

    /* Sleeping creatures cannot see */
    if (m_ptr->alertness < ALERTNESS_UNWARY)
        return false;

    const monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Peaceful creatures don't matter for stealth vision */
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return false;

    /* Shortsighted creatures can't see beyond 2 squares */
    if ((r_ptr->flags2 & (RF2_SHORT_SIGHTED)) && (m_ptr->cdis > 2))
        return false;

    if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        return false;

    /* Visual recognition for intelligent monsters */
    if (visual_recognition && (r_ptr->flags2 & (RF2_SMART)))
    {
        /* Disguise reduces monster's effective perception */
        int per_divisor = p_ptr->active_ability[S_STL][STL_DISGUISE] ? 4 : 2;

        int vision_score = monster_skill(m_ptr, S_PER) / per_divisor + p_ptr->cur_light
            + ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)) ? 2 : 0);

        if (vision_score < m_ptr->cdis)
            return false;
    }

    return true;
}

void map_info(int y, int x, byte* ap, char* cp, byte* tap, char* tcp)
{
    byte a = TERM_DARK; // these are defaults to soothe compilation warnings
    char c = ' '; //

    byte feat;
    u16b info;

    feature_type* f_ptr;
    object_type* o_ptr;

    s16b m_idx;

    s16b image = p_ptr->image;

    /* Monster/Player */
    m_idx = cave_m_idx[y][x];

    /* Feature */
    feat = cave_feat[y][x];

    /* Cave flags */
    info = cave_info[y][x];

    bool hide_square = false;
    bool rage_active = false;

    // Hide memorized squares out of line of sight during rage, and while in labyrinth partitions.
    if ((!p_ptr->is_dead) && (p_ptr->rage || g_labyrinth_view_active) && !(info & (CAVE_SEEN)))
        hide_square = true;

    // 'rage' visuals (red filter, rage tiles, etc.) - labyrinth uses the hide-only behavior above.
    if ((!p_ptr->is_dead) && p_ptr->rage)
        rage_active = true;

    /* make sure not to display things off screen */
    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
    {
        /* Get the darkness feature */
        f_ptr = &f_info[FEAT_NONE];

        cave_feature_visual(f_ptr, &a, &c);
    }

    // hiding squares out of line of sight during rage
    else if (hide_square)
    {
        /* Get the darkness feature */
        f_ptr = &f_info[FEAT_NONE];

        cave_feature_visual(f_ptr, &a, &c);
    }

    /* Boring grids (floors, etc) */
    else if (cave_floorlike_bold(y, x))
    {
        /* Seen floors are normal; marked, illuminated floors outside LOS use
         * the dark floor appearance selected by special_lighting_floor(). */
        if ((info & CAVE_SEEN)
            || ((info & CAVE_MARK) && cave_light[y][x] > 0))
        {
            int feat = FEAT_FLOOR;

            /* Get the floor feature */
            f_ptr = &f_info[feat];

            cave_feature_visual(f_ptr, &a, &c);

            /* Optional: apply group-based override for floor tiles */
            (void)apply_style_floor_graphics(y, x, feat, info, &a, &c);

            /* Skip special light for the player tile. */
            special_lighting_floor(&a, &c, info, cave_light[y][x]);
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            cave_feature_visual(f_ptr, &a, &c);
        }
    }

    /* Interesting grids (non-floors) */
    else
    {
        /* Memorized grids */
        if (info & (CAVE_MARK))
        {
            /* Apply "mimic" field */
            feat = f_info[feat].mimic;

            /* Get the feature */
            f_ptr = &f_info[feat];

            cave_feature_visual(f_ptr, &a, &c);

            /* Mark a rewired trap distinctly in ASCII view (tiles are tinted
             * by the renderer instead -- see sdl_rewired_trap_tint_active). */
            if (graphics_are_ascii() && cave_rewired[y][x]
                && (feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
            {
                a = TERM_VIOLET;
            }

            /* Optional: apply group-based override for doors */
            (void)apply_style_door_graphics(y, x, feat, info, &a, &c);

#if DEPTH_BASED_WALLS
            /* Apply style-based wall/vein graphics for non-ASCII graphics */
            if (!graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL) && feat != FEAT_RUBBLE)
            {
                /* Get the cave color for this location */
                byte color_value = cave_color[y][x];

                /* Decode style index from cave_color (first-variant flag is ignored here) */
                int sidx2 = cave_style_index_for_color(color_value);
                if (feat == FEAT_QUARTZ) {
                    /* Veins */
                    if (sidx2 >= 0) {
                        int display_sidx2 = cave_hallucination_style_for_display(sidx2);
                        style_type* s = &style_info[display_sidx2];
                        if (s->vein_defined) {
                            /* Full replacement vein tile */
                            a = (byte)(s->vein_row | 0x80);
                            c = (char)(s->vein_col | 0x80);
                        } else {
                            /* Overlay default vein tile on this style's wall tile */
                            extern byte get_default_vein_row(void);
                            extern byte get_default_vein_col(void);
                            byte dv_r = get_default_vein_row();
                            byte dv_c = get_default_vein_col();
                            byte wall_a = (byte)(s->wall_row | 0x80);
                            byte wall_c = (byte)(s->wall_col | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    wall_c += 1;
                                }
                            }
                            *tap = wall_a; *tcp = wall_c;
                            a = (byte)(dv_r | 0x80); c = (char)(dv_c | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    c += 1;
                                }
                            }
                            
                            /* Check for visible monster on this vein before returning */
                            if ((m_idx > 0) && !hide_square) {
                                monster_type* m_ptr = &mon_list[m_idx];
                                if (m_ptr->ml) {
                                    monster_race* r_ptr = &r_info[m_ptr->r_idx];
                                    if (image) r_ptr = &r_info[m_ptr->image_r_idx];
                                    byte da = r_ptr->x_attr;
                                    char dc = r_ptr->x_char;
                                    if ((da & 0x80) && (dc & 0x80)) {
                                        a = da; c = dc;
                                    } else if (r_ptr->flags1 & (RF1_ATTR_MULTI)) {
                                        a = multi_hued_attr(r_ptr); c = dc;
                                    } else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR))) {
                                        a = da; c = dc;
                                    }
                                    if (rage_active && graphics_are_ascii()) a = TERM_RED;
                                    if (!monster_race_is_vala(m_ptr->r_idx)
                                        && !graphics_are_ascii()
                                        && m_ptr->alertness >= ALERTNESS_ALERT) c += GRAPHICS_ALERT_MASK;
                                }
                            }
                            
                            *ap = a; *cp = c;
                            return;
                        }
                    } else {
                        /* No encoded style in cave_color: fall back to primary style (level or vault). */
                        int fb = cave_style_primary_for_grid(y, x);
                        fb = cave_hallucination_style_for_display(fb);
                        if (cave_style_index_is_valid(fb)) {
                            style_type* sfb = &style_info[fb];
                            byte wall_a = (byte)(sfb->wall_row | 0x80);
                            byte wall_c = (byte)(sfb->wall_col | 0x80);
                            extern byte get_default_vein_row(void);
                            extern byte get_default_vein_col(void);
                            byte dv_r = get_default_vein_row();
                            byte dv_c = get_default_vein_col();
                            a = (byte)(dv_r | 0x80); c = (char)(dv_c | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    c += 1; wall_c += 1;
                                }
                            }
                            *tap = wall_a; *tcp = wall_c; /* base wall */
                            log_warn("VEIN fallback: unencoded cave_color=%d at (%d,%d); using primary style %d wall(row=%d,col=%d)", color_value, y, x, fb, sfb->wall_row, sfb->wall_col);
                            
                            /* Check for visible monster on this vein before returning */
                            if ((m_idx > 0) && !hide_square) {
                                monster_type* m_ptr = &mon_list[m_idx];
                                if (m_ptr->ml) {
                                    monster_race* r_ptr = &r_info[m_ptr->r_idx];
                                    if (image) r_ptr = &r_info[m_ptr->image_r_idx];
                                    byte da = r_ptr->x_attr;
                                    char dc = r_ptr->x_char;
                                    if ((da & 0x80) && (dc & 0x80)) {
                                        a = da; c = dc;
                                    } else if (r_ptr->flags1 & (RF1_ATTR_MULTI)) {
                                        a = multi_hued_attr(r_ptr); c = dc;
                                    } else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR))) {
                                        a = da; c = dc;
                                    }
                                    if (rage_active && graphics_are_ascii()) a = TERM_RED;
                                    if (!monster_race_is_vala(m_ptr->r_idx)
                                        && !graphics_are_ascii()
                                        && m_ptr->alertness >= ALERTNESS_ALERT) c += GRAPHICS_ALERT_MASK;
                                }
                            }
                            
                            *ap = a; *cp = c;
                            return;
                        } else {
                            /* Give up: leave existing tiles */
                            log_warn("VEIN fallback: no primary style available at (%d,%d)", y, x);
                            return;
                        }
                    }
                } else {
                    /* Walls */
                    if (sidx2 >= 0) {
                        int display_sidx2 = cave_hallucination_style_for_display(sidx2);
                        style_type* s2 = &style_info[display_sidx2];
                        a = (byte)(s2->wall_row | 0x80);
                        c = (char)(s2->wall_col | 0x80);
                    } else {
                        int fb = cave_style_primary_for_grid(y, x);
                        fb = cave_hallucination_style_for_display(fb);
                        if (cave_style_index_is_valid(fb)) {
                            style_type* sfb = &style_info[fb];
                            a = (byte)(sfb->wall_row | 0x80);
                            c = (char)(sfb->wall_col | 0x80);
                            log_warn("WALL fallback: unencoded cave_color=%d at (%d,%d); using primary style %d (row=%d,col=%d)", color_value, y, x, fb, sfb->wall_row, sfb->wall_col);
                        } else {
                            a = (byte)(15 | 0x80);
                            c = (char)(14 | 0x80);
                            log_warn("WALL fallback: no primary style; using hard fallback at (%d,%d)", y, x);
                        }
                    }
                }

                /* Apply standard lighting effects (+1 for dark version)
                 * Dark variant only if not sunlit (no CAVE_GLOW) or if blind. */
                if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                    if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                        c += 1;
                    }
                }

                /* Save both terrain and display attributes */
                *tap = a;
                *tcp = c;

                /* Don't return early - let monster display code run */
            }
            else {
                /* ASCII/text mode: colour wall & vein glyphs by their style. */
                if (graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL) && feat != FEAT_RUBBLE)
                {
                    int sidx2 = cave_style_index_for_color(cave_color[y][x]);
                    if (sidx2 < 0) sidx2 = cave_style_primary_for_grid(y, x);
                    sidx2 = cave_hallucination_style_for_display(sidx2);
                    int style_attr = cave_style_ascii_attr(sidx2);
                    if (style_attr >= 0) a = (byte)style_attr;
                }

                /* Standard lighting effects (darkens unlit walls). */
                special_lighting_wall(&a, &c, feat, info, cave_light[y][x]);
            }
#else
            /* Depth-based walls disabled, use standard lighting only */
            special_lighting_wall(&a, &c, feat, info, cave_light[y][x]);
#endif /* DEPTH_BASED_WALLS */
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            cave_feature_visual(f_ptr, &a, &c);
        }
    }

    /* Save the terrain info for the transparency effects */
    byte terrain_a = a;
    char terrain_c = c;

    /* Traps, stairs, shafts, forges, sunlight, and rubble are drawn as a middle layer in
     * the SDL renderer (floor -> feature -> monster). For transparency to work, use a
     * floor tile as the terrain underlay when one of these features is visible. */
    if ((info & (CAVE_MARK)) &&
        (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL)) ||
         ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL)) ||
         ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL)) ||
         (feat == FEAT_SUNLIGHT) ||
         (feat == FEAT_RUBBLE)))
    {
        int floor_feat = FEAT_FLOOR;
        feature_type* floor_ptr = &f_info[floor_feat];
        cave_feature_visual(floor_ptr, &terrain_a, &terrain_c);

        (void)apply_style_floor_graphics(y, x, floor_feat, info, &terrain_a, &terrain_c);
        special_lighting_floor(&terrain_a, &terrain_c, info, cave_light[y][x]);
    }

    (*tap) = terrain_a;
    (*tcp) = terrain_c;

    /* Objects (only shown when on floors, not when in rubble) */
    if (feat == FEAT_FLOOR || feat == FEAT_SUNLIGHT)
    {
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Memorized objects */
            if (o_ptr->marked && !hide_square)
            {
                /* Normal attr */
                a = object_attr(o_ptr);

                /* Normal char */
                c = object_char(o_ptr);

                /* display this */
                break;
            }
        }
    }

    /* Monsters */
    if ((m_idx > 0) && !hide_square)
    {
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if (!m_ptr->ml)
        {
            byte ha;
            char hc;

            if (song_revealing_overlay(m_idx, &ha, &hc))
            {
                a = ha;
                c = hc;
            }
        }

        /* Visible monster*/
        if (m_ptr->ml)
        {
            byte da;
            char dc;
            bool is_vala = monster_race_is_vala(m_ptr->r_idx);

            /* Hack -- monster hallucination */
            if (image)
            {
                r_ptr = &r_info[m_ptr->image_r_idx];
            }

            /* Desired attr */
            cave_monster_visual(r_ptr, &da, &dc);

            /* Special attr/char codes */
            if ((da & 0x80) && (dc & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Multi-hued monster */
            else if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            {
                /* Multi-hued attr */
                a = multi_hued_attr(r_ptr);

                /* Normal char */
                c = dc;
            }

            /* Normal monster (not "clear" in any way) */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR)))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Hack -- Bizarre grid under monster */
            else if ((a & 0x80) || (c & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Normal char, Clear attr, monster */
            else if (!(r_ptr->flags1 & (RF1_CHAR_CLEAR)))
            {
                /* Normal char */
                c = dc;
            }

            /* Normal attr, Clear char, monster */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR)))
            {
                /* Normal attr */
                a = da;
            }

            if (rage_active && graphics_are_ascii())
            {
                a = TERM_RED;
            }

            if (!is_vala && hilite_unwary && (m_ptr->alertness < ALERTNESS_ALERT)
                && use_background_colors && graphics_are_ascii())
            {
                a += (MAX_COLORS * BG_DARK);
            }
            else if (!is_vala && !graphics_are_ascii()
                && m_ptr->alertness >= ALERTNESS_ALERT)
            {
                c += GRAPHICS_ALERT_MASK;
            }

            /* Sleeping overlay: indicate when this monster is asleep. */
            if (!is_vala && !graphics_are_ascii() && sleep_icon && tap != ap
                && m_ptr->alertness < ALERTNESS_UNWARY)
            {
                *tap = (byte)(((byte)(*tap)) | GRAPHICS_SLEEP_MASK);
            }

            /* Stealth vision overlay: indicate when this monster can see you. */
            if (!is_vala && !graphics_are_ascii() && stealth_vision && tcp != cp
                && monster_can_see_player_for_stealth_vision(m_ptr))
            {
                *tcp = (char)(((byte)(*tcp)) | GRAPHICS_SEEN_MASK);
            }
        }
    }

    /* Handle "player" */
    else if (m_idx < 0)
    {
        monster_race* r_ptr = &r_info[0];

        if (graphics_are_ascii())
        {
            a = health_attr(p_ptr->chp, p_ptr->mhp);
            c = r_ptr->d_char;
        }
        else
        {
            r_ptr = &r_info[p_ptr->prace]; // XXX grafic for player

            cave_monster_visual(r_ptr, &a, &c);
            c += player_tile_offset();
        }
    }

    /* Result */
    (*ap) = a;
    (*cp) = c;
}

/*
 * Same as map_info, but always return the char/attr specified by the
 * info files.
 * This IS an hack, I dont like to duplicate code like that, but the only
 * other way it to hack map_info itself and put lots of if statements in it,
 * which could reduce speed.
 */
void map_info_default(int y, int x, byte* ap, char* cp)
{
    byte a;
    char c;

    byte feat;
    u16b info;

    feature_type* f_ptr;
    object_type* o_ptr;

    s16b m_idx;

    s16b image = p_ptr->image;

    /* Monster/Player */
    m_idx = cave_m_idx[y][x];

    /* Feature */
    feat = cave_feat[y][x];

    /* Cave flags */
    info = cave_info[y][x];

    /* Hack -- rare random hallucination on non-outer walls */
    if ((image) && (feat < FEAT_WALL_PERM) && (image_count-- <= 0))
    {
        int i = image_random();

        a = PICT_A(i);
        c = PICT_C(i);
    }

    /* Boring grids (floors, etc) */
    else if (cave_floorlike_bold(y, x))
    {
        /* Seen floors are normal; marked and illuminated floors remain mapped
         * outside LOS and are darkened by the logic below. */
        if ((info & CAVE_SEEN)
            || ((info & CAVE_MARK) && cave_light[y][x] > 0))
        {
            /* Get the floor feature */
            f_ptr = &f_info[FEAT_FLOOR];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;

            /* Handle "seen" grids */
            if (info & (CAVE_SEEN))
            {
                /* Do Nothing */
            }

            /* Handle "blind" */
            else if ((p_ptr->blind) || (!(info & (CAVE_GLOW))))
            {
                /* Darken the colour */
                a = darken(a, c);
            }

            /* Handle "unseen" grids */
            else
            {
                /* Darken the colour */
                a = darken(a, c);
            }
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;
        }
    }

    /* Interesting grids (non-floors) */
    else
    {
        /* Memorized grids */
        if (info & (CAVE_MARK))
        {
            /* Apply "mimic" field */
            feat = f_info[feat].mimic;

            /* Get the feature */
            f_ptr = &f_info[feat];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;

            /* Special lighting effects (walls only) */
            if (cave_wall_bold(y, x) || feat_supports_lighting(feat))
            {
                /* Handle "seen" grids */
                if (info & (CAVE_SEEN))
                {
                    /* Do nothing */
                }

                /* Handle "blind" */
                else if (p_ptr->blind)
                {
                    /* Darken the colour */
                    a = darken(a, c);
                }

                /* Handle "unseen" grids */
                else
                {
                    /* Darken the colour */
                    a = darken(a, c);
                }
            }
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;
        }
    }

    /* Objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        /* Memorized objects */
        if (o_ptr->marked)
        {
            /* Hack -- object hallucination */
            if (image)
            {
                int i = image_object();

                a = PICT_A(i);
                c = PICT_C(i);

                break;
            }

            /* Normal attr */
            a = object_attr_default(o_ptr);

            /* Normal char */
            c = object_char_default(o_ptr);

            /* Display the top item of the pile */
            break;
        }
    }

    /* Monsters */
    if (m_idx > 0)
    {
        monster_type* m_ptr = &mon_list[m_idx];

        /* Visible monster */
        if (m_ptr->ml)
        {
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            byte da;
            char dc;

            /* Desired attr */
            da = r_ptr->d_attr;

            /* Desired char */
            dc = r_ptr->d_char;

            /* Hack -- monster hallucination */
            if (image)
            {
                int i = image_monster();

                a = PICT_A(i);
                c = PICT_C(i);
            }

            /* Special attr/char codes */
            else if ((da & 0x80) && (dc & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Multi-hued monster */
            else if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            {
                /* Multi-hued attr */
                a = dieroll(15);

                /* Normal char */
                c = dc;
            }

            /* Normal monster (not "clear" in any way) */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR)))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Hack -- Bizarre grid under monster */
            else if ((a & 0x80) || (c & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Normal char, Clear attr, monster */
            else if (!(r_ptr->flags1 & (RF1_CHAR_CLEAR)))
            {
                /* Normal char */
                c = dc;
            }

            /* Normal attr, Clear char, monster */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR)))
            {
                /* Normal attr */
                a = da;
            }
        }
    }

    /* Handle "player" */
    else if (m_idx < 0)
    {
        monster_race* r_ptr = &r_info[0];

        /*change player color with HP*/
        a = health_attr(p_ptr->chp, p_ptr->mhp);

        /* Get the "player" char */
        c = r_ptr->d_char;
    }

#ifdef MAP_INFO_MULTIPLE_PLAYERS
    /* Players */
    else if (m_idx < 0)
#else /* MAP_INFO_MULTIPLE_PLAYERS */
    /* Handle "player" */
    else if (m_idx < 0)
#endif /* MAP_INFO_MULTIPLE_PLAYERS */
    {
        monster_race* r_ptr = &r_info[0];

        /* Get the "player" attr */
        a = r_ptr->d_attr;

        /* Get the "player" char */
        c = r_ptr->d_char;
    }

    /* Result */
    (*ap) = a;
    (*cp) = c;
}
