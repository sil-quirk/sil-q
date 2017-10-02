/* File: cave.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * Support for Adam Bolt's tileset, lighting and transparency effects
 * by Robert Ruehlmann (rr9@thangorodrim.net)
 */

/*
 * Approximate distance between two points.
 *
 * When either the X or Y component dwarfs the other component,
 * this function is almost perfect, and otherwise, it tends to
 * over-estimate about one grid per fifteen grids of distance.
 *
 * Algorithm: hypot(dy,dx) = max(dy,dx) + min(dy,dx) / 2
 */
int distance(int y1, int x1, int y2, int x2)
{
	int ay, ax;

	/* Find the absolute y/x distance components */
	ay = (y1 > y2) ? (y1 - y2) : (y2 - y1);
	ax = (x1 > x2) ? (x1 - x2) : (x2 - x1);

	/* Hack -- approximate the distance */
	return ((ay > ax) ? (ay + (ax>>1)) : (ax + (ay>>1)));
}

/*
 * The square of the distance between two points.
 *
 * Used when we need a fine-grained ordering of euclidean distance.
 * e.g. helps an archer who is stuck against a wall to find his way out.
 */
int distance_squared(int y1, int x1, int y2, int x2)
{
	int ay, ax;
	
	/* Find the absolute y/x distance components */
	ay = (y1 > y2) ? (y1 - y2) : (y2 - y1);
	ax = (x1 > x2) ? (x1 - x2) : (x2 - x1);
	
	/* Hack -- approximate the distance */
	return (ay*ay + ax+ax);
}


/*
 * A simple, fast, integer-based line-of-sight algorithm.  By Joseph Hall,
 * 4116 Brewster Drive, Raleigh NC 27606.  Email to jnh@ecemwl.ncsu.edu.
 *
 * This function returns TRUE if a "line of sight" can be traced from the
 * center of the grid (x1,y1) to the center of the grid (x2,y2), with all
 * of the grids along this path (except for the endpoints) being non-wall
 * grids.  Actually, the "chess knight move" situation is handled by some
 * special case code which allows the grid diagonally next to the player
 * to be obstructed, because this yields better gameplay semantics.  This
 * algorithm is totally reflexive, except for "knight move" situations.
 *
 * Because this function uses (short) ints for all calculations, overflow
 * may occur if dx and dy exceed 90.
 *
 * Once all the degenerate cases are eliminated, we determine the "slope"
 * ("m"), and we use special "fixed point" mathematics in which we use a
 * special "fractional component" for one of the two location components
 * ("qy" or "qx"), which, along with the slope itself, are "scaled" by a
 * scale factor equal to "abs(dy*dx*2)" to keep the math simple.  Then we
 * simply travel from start to finish along the longer axis, starting at
 * the border between the first and second tiles (where the y offset is
 * thus half the slope), using slope and the fractional component to see
 * when motion along the shorter axis is necessary.  Since we assume that
 * vision is not blocked by "brushing" the corner of any grid, we must do
 * some special checks to avoid testing grids which are "brushed" but not
 * actually "entered".
 *
 * Sil has three different "line of sight" type concepts, including this
 * function (which is used almost nowhere), the "project()" method (which
 * is used for determining the paths of projectables and spells and such),
 * and the "update_view()" concept (which is used to determine which grids
 * are "viewable" by the player, which is used for many things, such as
 * determining which grids are illuminated by the player's torch, and which
 * grids and monsters can be "seen" by the player, etc).
 */
bool los(int y1, int x1, int y2, int x2)
{
	/* Delta */
	int dx, dy;

	/* Absolute */
	int ax, ay;

	/* Signs */
	int sx, sy;

	/* Fractions */
	int qx, qy;

	/* Scanners */
	int tx, ty;

	/* Scale factors */
	int f1, f2;

	/* Slope, or 1/Slope, of LOS */
	int m;

	/* Extract the offset */
	dy = y2 - y1;
	dx = x2 - x1;

	/* Extract the absolute offset */
	ay = ABS(dy);
	ax = ABS(dx);


	/* Handle adjacent (or identical) grids */
	if ((ax < 2) && (ay < 2)) return (TRUE);


	/* Directly South/North */
	if (!dx)
	{
		/* South -- check for walls */
		if (dy > 0)
		{
			for (ty = y1 + 1; ty < y2; ty++)
			{
				if (!cave_floor_bold(ty, x1)) return (FALSE);
			}
		}

		/* North -- check for walls */
		else
		{
			for (ty = y1 - 1; ty > y2; ty--)
			{
				if (!cave_floor_bold(ty, x1)) return (FALSE);
			}
		}

		/* Assume los */
		return (TRUE);
	}

	/* Directly East/West */
	if (!dy)
	{
		/* East -- check for walls */
		if (dx > 0)
		{
			for (tx = x1 + 1; tx < x2; tx++)
			{
				if (!cave_floor_bold(y1, tx)) return (FALSE);
			}
		}

		/* West -- check for walls */
		else
		{
			for (tx = x1 - 1; tx > x2; tx--)
			{
				if (!cave_floor_bold(y1, tx)) return (FALSE);
			}
		}

		/* Assume los */
		return (TRUE);
	}


	/* Extract some signs */
	sx = (dx < 0) ? -1 : 1;
	sy = (dy < 0) ? -1 : 1;


	/* Vertical "knights" */
	if (ax == 1)
	{
		if (ay == 2)
		{
			if (cave_floor_bold(y1 + sy, x1)) return (TRUE);
		}
	}

	/* Horizontal "knights" */
	else if (ay == 1)
	{
		if (ax == 2)
		{
			if (cave_floor_bold(y1, x1 + sx)) return (TRUE);
		}
	}


	/* Calculate scale factor div 2 */
	f2 = (ax * ay);

	/* Calculate scale factor */
	f1 = f2 << 1;

	/* Travel horizontally */
	if (ax >= ay)
	{
		/* Let m = dy / dx * 2 * (dy * dx) = 2 * dy * dy */
		qy = ay * ay;
		m = qy << 1;

		tx = x1 + sx;

		/* Consider the special case where slope == 1. */
		if (qy == f2)
		{
			ty = y1 + sy;
			qy -= f1;
		}
		else
		{
			ty = y1;
		}

		/* Note (below) the case (qy == f2), where */
		/* the LOS exactly meets the corner of a tile. */
		while (x2 - tx)
		{
			if (!cave_floor_bold(ty, tx)) return (FALSE);

			qy += m;

			if (qy < f2)
			{
				tx += sx;
			}
			else if (qy > f2)
			{
				ty += sy;
				if (!cave_floor_bold(ty, tx)) return (FALSE);
				qy -= f1;
				tx += sx;
			}
			else
			{
				ty += sy;
				qy -= f1;
				tx += sx;
			}
		}
	}

	/* Travel vertically */
	else
	{
		/* Let m = dx / dy * 2 * (dx * dy) = 2 * dx * dx */
		qx = ax * ax;
		m = qx << 1;

		ty = y1 + sy;

		if (qx == f2)
		{
			tx = x1 + sx;
			qx -= f1;
		}
		else
		{
			tx = x1;
		}

		/* Note (below) the case (qx == f2), where */
		/* the LOS exactly meets the corner of a tile. */
		while (y2 - ty)
		{
			if (!cave_floor_bold(ty, tx)) return (FALSE);

			qx += m;

			if (qx < f2)
			{
				ty += sy;
			}
			else if (qx > f2)
			{
				tx += sx;
				if (!cave_floor_bold(ty, tx)) return (FALSE);
				qx -= f1;
				ty += sy;
			}
			else
			{
				tx += sx;
				qx -= f1;
				ty += sy;
			}
		}
	}

	/* Assume los */
	return (TRUE);
}


void random_unseen_floor(int *ry, int *rx)
{
	int i, y, x;
		
	// (poor) defaults in case no floor space can be found
	*ry = p_ptr->py;
	*rx = p_ptr->px;

	/* simple way to pick a random floor tile */
	for (i = 0; i <= 1000; i++)
	{
		y = rand_int(p_ptr->cur_map_hgt);
		x = rand_int(p_ptr->cur_map_wid);
		if (cave_naked_bold(y,x) && !player_can_see_bold(y,x))
		{
			*ry = y;
			*rx = x;
			return;
		}
	}
	return;
}


/*
 * Returns true if the player's grid is dark
 */
bool no_light(void)
{
	return (!player_can_see_bold(p_ptr->py, p_ptr->px));
}


/*
 *  Determines whether a square is viewable (only) by the keen senses ability.
 *
 * Sil-y: Note that there is a slight flaw in the implementation.
 *        Since light levels are not adjusted by monsters/items if out of view (CAVE_VIEW)
 *        this means that if a square adjacent to the monster is out of view but lit and
 *        the monster's square is not lit, then sometimes it won't show when it should show.
 *        This is pretty rare though, and I doubt it is very noticeable.
 */
bool seen_by_keen_senses(int fy, int fx)
{
	int d;
	
	if (p_ptr->active_ability[S_PER][PER_KEEN_SENSES] &&
		(cave_info[fy][fx] & (CAVE_VIEW)) &&
		(cave_light[fy][fx] == 0))
	{
		for (d = 0; d < 8; d++)
		{
			/* Child location */
			int y2 = fy + ddy_ddd[d];
			int x2 = fx + ddx_ddd[d];
			
			/* Check Bounds */
			if (!in_bounds(y2, x2)) continue;
			
			if ((cave_light[y2][x2] > 0) && cave_floor_bold(y2,x2) && (cave_info[y2][x2] & (CAVE_VIEW)))
			{
				return (TRUE);
			}
		}
	}
	
	return (FALSE);
}


/*
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool cave_valid_bold(int y, int x)
{
	object_type *o_ptr;
	u32b f1, f2, f3;

	/* Forbid perma-grids */
	if (cave_perma_bold(y, x)) return (FALSE);

	/* Check objects */
	for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
	{
		object_flags(o_ptr, &f1, &f2, &f3);

		/* Forbid grids with indestructable items */
		if (f3 & (TR3_INDESTRUCTIBLE)) return (FALSE);
	}

	/* Accept */
	return (TRUE);
}



/*
 * Multi-hued monsters shimmer according to their base colour.
 */
static byte multi_hued_attr(monster_race *r_ptr)
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
		if ((r_ptr->d_attr == TERM_VIOLET) || (r_ptr->d_attr == TERM_VIOLET + TERM_SHADE))
			return ((one_in_(2)) ? TERM_VIOLET :  TERM_VIOLET + TERM_SHADE);
	}

	/* Otherwise can be any color */
	return (dieroll(15));
}


/*
 * Hack -- Legal monster codes
 */
static const char image_monster_hack[] = \
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

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
	return (PICT(a,c));
}


/*
 * Hack -- Legal object codes
 */
static const char image_object_hack[] = \
"?/|\\\"!$()_-=[]{},~"; /* " */

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
	return (PICT(a,c));
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
	if (use_graphics == GRAPHICS_PSEUDO) return FALSE;

	if 	((use_graphics != GRAPHICS_DAVID_GERVAIS) &&
	    (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))))
	{
		return TRUE;
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
			return TRUE;
		default:
			return FALSE;
	}
}

static byte darken(byte a, byte c)
{
	// don't darken the symbols for traps
	if (c == '^') return (a);

	if (a == TERM_WHITE)                    return (TERM_SLATE);
	if (a == TERM_L_WHITE)                  return (TERM_L_DARK);
	if (a == TERM_SLATE)                    return (TERM_L_DARK);
	if (a == TERM_L_DARK)                   return (TERM_DARK + TERM_SHADE);
	if (a == TERM_L_UMBER)                  return (TERM_UMBER);
	if (a == TERM_L_BLUE)                   return (TERM_BLUE);
	if (a == TERM_L_GREEN)                  return (TERM_GREEN);
	if (a == TERM_L_WHITE + TERM_SHADE)     return (TERM_L_DARK + TERM_SHADE);
	
	return (a);
}

static void special_lighting_floor(byte *a, char *c, int info, int light)
{
	/* Handle "seen" grids */
	if (info & (CAVE_SEEN))
	{
		/* Do nothing */
	}
	
	/* Handle "dark" grids and "blindness" */
	else if (p_ptr->blind || (light <= 0))
	{
		/* Use a dark tile */
		switch (use_graphics)
		{
			case GRAPHICS_NONE:
			case GRAPHICS_PSEUDO:
				/* special darkening */
				*a = TERM_DARK + TERM_SHADE;
				break;
			case GRAPHICS_ADAM_BOLT:
			case GRAPHICS_DAVID_GERVAIS:
				*c += 1;
				break;
		}
	}

	/* Handle "unseen" grids */
	else
	{
		switch (use_graphics)
		{
			case GRAPHICS_NONE:
			case GRAPHICS_PSEUDO:
				/* darken the colour */
				*a = darken(*a,*c);
				break;
			case GRAPHICS_ADAM_BOLT:
			case GRAPHICS_DAVID_GERVAIS:
				*c += 1;
				break;
		}
	}
}


static void special_lighting_wall(byte *a, char *c, int feat, int info)
{
	/* Handle "seen" grids */
	if (info & (CAVE_SEEN))
	{
		/* Do nothing */
	}

	/* Handle "blind" */
	else if (p_ptr->blind)
	{
		switch (use_graphics)
		{
			case GRAPHICS_NONE:
			case GRAPHICS_PSEUDO:
				/* darken the colour */
				*a = darken(*a,*c);
				break;
			case GRAPHICS_ADAM_BOLT:
			case GRAPHICS_DAVID_GERVAIS:
				if (feat_supports_lighting(feat)) *c += 1;
				break;
		}
	}

	/* Handle "unseen" grids */
	else
	{
		switch (use_graphics)
		{
			case GRAPHICS_NONE:
			case GRAPHICS_PSEUDO:
				/* darken the colour */
				*a = darken(*a,*c);
				break;
			case GRAPHICS_ADAM_BOLT:
			case GRAPHICS_DAVID_GERVAIS:
				if (feat_supports_lighting(feat)) *c += 1;
				break;
		}
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
 * To determine if a "boring" grid should be displayed, we simply check to
 * see if it is either memorized ("CAVE_MARK"), or currently "see-able" by
 * the player ("CAVE_SEEN").  Note that "CAVE_SEEN" is now maintained by the
 * "update_view()" function.
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
 * in "cave.c" would have to be updated to create the shimmer effect.
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

void map_info(int y, int x, byte *ap, char *cp, byte *tap, char *tcp)
{
	byte a = TERM_DARK; // these are defaults to soothe compilation warnings
	char c = ' ';		//

	byte feat;
	u16b info;

	feature_type *f_ptr;
	object_type *o_ptr;

	s16b m_idx;

	s16b image = p_ptr->image;

	int floor_num = 0;

	/* Monster/Player */
	m_idx = cave_m_idx[y][x];

	/* Feature */
	feat = cave_feat[y][x];

	/* Cave flags */
	info = cave_info[y][x];

	bool hide_square = FALSE;
	bool blood_red = FALSE;
	
	// 'rage' effects...
	if ((!p_ptr->is_dead) && p_ptr->rage && !(info & (CAVE_SEEN))) hide_square = TRUE;
	if ((!p_ptr->is_dead) && p_ptr->rage)                          blood_red = TRUE;

	/* make sure not to display things off screen */
	if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt) || (x >= p_ptr->cur_map_wid))
	{
		/* Get the darkness feature */
		f_ptr = &f_info[FEAT_NONE];

		/* Normal attr */
		a = f_ptr->x_attr;

		/* Normal char */
		c = f_ptr->x_char;
	}
    
	/* Hack -- rare random hallucination on non-outer walls */
	else if ((image) && (feat < FEAT_WALL_PERM) && (image_count-- <= 0))
	{
		int i;

		image_count = dieroll(200);

		if (player_can_see_bold(y, x))
		{
			/* Display a random image, reset count. */
			i = image_random();

			a = PICT_A(i);
			c = PICT_C(i);
		}
	}
    
    // hiding squares out of line of sight during rage
	else if (hide_square)
	{
		/* Get the darkness feature */
		f_ptr = &f_info[FEAT_NONE];
		
		/* Normal attr */
		a = f_ptr->x_attr;
		
		/* Normal char */
		c = f_ptr->x_char;
	}
    
	/* Boring grids (floors, etc) */
	else if (cave_floorlike_bold(y,x))
	{
		/* Memorized (or seen) floor */
		if ((info & (CAVE_MARK)) ||
		    (info & (CAVE_SEEN)))
		{
			/* Get the floor feature */
			f_ptr = &f_info[FEAT_FLOOR];

			/* Normal attr */
			a = f_ptr->x_attr;

			/* Normal char */
			c = f_ptr->x_char;

			/* Special lighting effects */
			special_lighting_floor(&a, &c, info, cave_light[y][x]);
		}

		/* Unknown */
		else
		{
			/* Get the darkness feature */
			f_ptr = &f_info[FEAT_NONE];

			/* Normal attr */
			a = f_ptr->x_attr;

			/* Normal char */
			c = f_ptr->x_char;
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
			a = f_ptr->x_attr;

			/* Normal char */
			c = f_ptr->x_char;

			/* Special lighting effects (walls only) */
			special_lighting_wall(&a, &c, feat, info);
		}

		/* Unknown */
		else
		{
			/* Get the darkness feature */
			f_ptr = &f_info[FEAT_NONE];

			/* Normal attr */
			a = f_ptr->x_attr;

			/* Normal char */
			c = f_ptr->x_char;
		}
	}

	/* Save the terrain info for the transparency effects */
	(*tap) = a;
	(*tcp) = c;

	/* Objects (only shown when on floors, not when in rubble) */
	if (feat == FEAT_FLOOR)
	{
		for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
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
				
				
				/* Special stack symbol, unless everything in the pile is squelchable */
				if ((++floor_num > 1) &&
					(use_graphics ? ((a != k_info[GRAF_BROKEN_BONE].x_attr) ||
									 (c != k_info[GRAF_BROKEN_BONE].x_char)) :
					 ((a != TERM_VIOLET) ||
					  (c != f_info[1].x_char))))
				{
					object_kind *k_ptr;
					
					/* Get the "pile" feature */
					k_ptr = &k_info[0];
					
					/* Normal attr */
					a = k_ptr->x_attr;
					
					/* Normal char */
					c = k_ptr->x_char;
					
					break;
				}
			}
		}
	}

	/* Monsters */
	if ((m_idx > 0) && !hide_square)
	{
		monster_type *m_ptr = &mon_list[m_idx];
        monster_race *r_ptr = &r_info[m_ptr->r_idx];

		/* Visible monster*/
		if (m_ptr->ml)
		{

			byte da;
			char dc;
			
			/* Hack -- monster hallucination */
			if (image)
			{
				r_ptr = &r_info[m_ptr->image_r_idx];
			}
						
			/* Desired attr */
			da = r_ptr->x_attr;

			/* Desired char */
			dc = r_ptr->x_char;

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
			
			if (blood_red)
			{
				a = TERM_RED;
			}
			
			if (use_background_colors)
			{
				switch (use_graphics)
				{
					case GRAPHICS_NONE:
					case GRAPHICS_PSEUDO:
						if (1)
						{
							if (hilite_unwary && (m_ptr->alertness < ALERTNESS_ALERT))
							{
								a += (MAX_COLORS * BG_DARK);
							}
						}
						break;
				}
			}
		}
	}

	/* Handle "player" */
	else if (m_idx < 0)
	{
		monster_race *r_ptr = &r_info[0];

		/* Get the "player" attr */

		if (arg_graphics == GRAPHICS_NONE)
		{
			a = health_attr(p_ptr->chp, p_ptr->mhp);
		}

		else a = r_ptr->x_attr;

		/* Get the "player" char */
		c = r_ptr->x_char;
	}

#ifdef MAP_INFO_MULTIPLE_PLAYERS
	/* Players */
	else if (m_idx < 0)
#else /* MAP_INFO_MULTIPLE_PLAYERS */
	/* Handle "player" */
	else if (m_idx < 0)
#endif /* MAP_INFO_MULTIPLE_PLAYERS */
	{
		monster_race *r_ptr = &r_info[0];

		/* Get the "player" attr */
		a = r_ptr->x_attr;

		/* Get the "player" char */
		c = r_ptr->x_char;
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
void map_info_default(int y, int x, byte *ap, char *cp)
{
	byte a;
	char c;

	byte feat;
	u16b info;

	feature_type *f_ptr;
	object_type *o_ptr;

	s16b m_idx;

	s16b image = p_ptr->image;

	int floor_num = 0;

	bool sq_flag = FALSE;
	bool do_purple_dot = TRUE;

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
	else if (cave_floorlike_bold(y,x))
	{
		/* Memorized (or seen) floor */
		if ((info & (CAVE_MARK)) ||
		    (info & (CAVE_SEEN)))
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
				a = darken(a,c);
			}

			/* Handle "unseen" grids */
			else
			{
				/* Darken the colour */
				a = darken(a,c);
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
                	a = darken(a,c);
				}

				/* Handle "unseen" grids */
				else
				{
					/* Darken the colour */
                	a = darken(a,c);
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

			/*autosquelch insert*/
			sq_flag =  ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS) &&
 						(k_info[o_ptr->k_idx].aware));

			if ((!sq_flag))
			{
				/* Normal attr */
				a = object_attr_default(o_ptr);

				/* Normal char */
				c = object_char_default(o_ptr);

				/*found a non-squelchable item, display this one*/
				break;


			}
			else if (do_purple_dot)
			{
				/* Special squelch character HACK */
				/* Colour of Blade of Chaos */
				a = TERM_VIOLET;

				/* Symbol of floor */
				c = f_info[1].x_char;
			}

			/* Special stack symbol, unless everything in the pile is squelchable */
			if ((++floor_num > 1) && ((a != TERM_VIOLET) ||
				(c != f_info[1].x_char)))
			{
				object_kind *k_ptr;

				/* Get the "pile" feature */
				k_ptr = &k_info[0];

				/* Normal attr */
                a = k_ptr->d_attr;

				/* Normal char */
                c = k_ptr->d_char;

				break;
			}
		}
	}


	/* Monsters */
	if (m_idx > 0)
	{
		monster_type *m_ptr = &mon_list[m_idx];

		/* Visible monster */
		if (m_ptr->ml)
		{
			monster_race *r_ptr = &r_info[m_ptr->r_idx];

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
		monster_race *r_ptr = &r_info[0];

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
		monster_race *r_ptr = &r_info[0];

		/* Get the "player" attr */
        a = r_ptr->d_attr;

		/* Get the "player" char */
        c = r_ptr->d_char;
	}

	/* Result */
	(*ap) = a;
	(*cp) = c;
}

/*
 * Move the cursor to a given map location.
 *
 * The main screen will always be at least 24x80 in size.
 */
void move_cursor_relative(int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Location relative to panel */
	ky = y - p_ptr->wy;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT)) return;

	/* Location relative to panel */
	kx = x - p_ptr->wx;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID)) return;

	/* Location in window */
	vy = ky + ROW_MAP;

	/* Location in window */
	vx = kx + COL_MAP;

	if (use_bigtile) vx += kx;

	/* Go there */
	(void)Term_gotoxy(vx, vy);
}



/*
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void print_rel(char c, byte a, int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Location relative to panel */
	ky = y - p_ptr->wy;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT)) return;

	/* Location relative to panel */
	kx = x - p_ptr->wx;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID)) return;

	/* Location in window */
	vy = ky + ROW_MAP;

	/* Location in window */
	vx = kx + COL_MAP;

	if (use_bigtile) vx += kx;

	/* Hack -- Queue it */
	Term_queue_char(vx, vy, a, c, 0, 0);

	if (use_bigtile)
	{
		/* Mega-Hack : Queue dummy char */
		if (a & 0x80)
			Term_queue_char(vx+1, vy, 255, -1, 0, 0);
		else
			Term_queue_char(vx+1, vy, TERM_WHITE, ' ', 0, 0);
	}
}




/*
 * Memorize interesting viewable object/features in the given grid
 *
 * This function should only be called on "legal" grids.
 *
 * This function will memorize the object and/or feature in the given grid,
 * if they are (1) see-able and (2) interesting.  Note that all objects are
 * interesting, all terrain features except floors (and invisible traps) are
 * interesting, and floors (and invisible traps) are interesting sometimes
 * (depending on various options involving the illumination of floor grids).
 *
 * The automatic memorization of all objects and non-floor terrain features
 * as soon as they are displayed allows incredible amounts of optimization
 * in various places, especially "map_info()" and this function itself.
 *
 * Note that the memorization of objects is completely separate from the
 * memorization of terrain features, preventing annoying floor memorization
 * when a detected object is picked up from a dark floor, and object
 * memorization when an object is dropped into a floor grid which is
 * memorized but out-of-sight.
 *
 * This function should be called every time the "memorization" of a grid
 * (or the object in a grid) is called into question, such as when an object
 * is created in a grid, when a terrain feature "changes" from "floor" to
 * "non-floor", and when any grid becomes "see-able" for any reason.
 *
 * This function is called primarily from the "update_view()" function, for
 * each grid which becomes newly "see-able".
 */
void note_spot(int y, int x)
{
	u16b info;

	object_type *o_ptr;

	/* Get cave info */
	info = cave_info[y][x];

	/* Require "seen" flag */
	if (!(info & (CAVE_SEEN))) return;

	/* Hack -- memorize objects */
	for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
	{
		/* Memorize objects */
		o_ptr->marked = TRUE;
	}

	/* Hack -- memorize grids */
	if (!(info & (CAVE_MARK)))
	{
		/* Memorize some "boring" grids */
		if (cave_floorlike_bold(y,x))
		{
			/* Option -- memorize certain floors */
			if (info & (CAVE_GLOW))
			{
				/* Memorize */
				cave_info[y][x] |= (CAVE_MARK);
			}
		}

		/* Memorize all "interesting" grids */
		else
		{
			/* Memorize */
			cave_info[y][x] |= (CAVE_MARK);
		}
	}
}


/*
 * Redraw (on the screen) a given map location
 *
 * This function should only be called on "legal" grids.
 *
 * Note the inline use of "print_rel()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void lite_spot(int y, int x)
{
	byte a;
	char c;
	byte ta;
	char tc;

	int ky, kx;
	int vy, vx;

	/* Location relative to panel */
	ky = y - p_ptr->wy;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT)) return;

	/* Location relative to panel */
	kx = x - p_ptr->wx;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID)) return;

	/* Location in window */
	vy = ky + ROW_MAP;

	/* Location in window */
	vx = kx + COL_MAP;

	if (use_bigtile) vx += kx;

	/* Hack -- redraw the grid */
	map_info(y, x, &a, &c, &ta, &tc);

	/* Hack -- Queue it */
	Term_queue_char(vx, vy, a, c, ta, tc);

	if (use_bigtile)
	{
		vx++;

		/* Mega-Hack : Queue dummy char */
		if (a & 0x80)
			Term_queue_char(vx, vy, 255, -1, 0, 0);
		else
			Term_queue_char(vx, vy, TERM_WHITE, ' ', TERM_WHITE, ' ');
	}
	
}



/*
 * Redraw (on the screen) the current map panel
 *
 * Note the inline use of "lite_spot()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void prt_map(void)
{
	byte a;
	char c;
	byte ta;
	char tc;

	int y, x;
	int vy, vx;
	int ty, tx;

	/* Assume screen */
	ty = p_ptr->wy + SCREEN_HGT;
	tx = p_ptr->wx + SCREEN_WID;

	/* Dump the map */
	for (y = p_ptr->wy, vy = ROW_MAP; y < ty; vy++, y++)
	{
		for (x = p_ptr->wx, vx = COL_MAP; x < tx; vx++, x++)
		{
			/* Check bounds */
			if (!in_bounds(y, x)) continue;

			/* Determine what is there */
			map_info(y, x, &a, &c, &ta, &tc);

			/* Hack -- Queue it */
			Term_queue_char(vx, vy, a, c, ta, tc);

			if (use_bigtile)
			{
				vx++;

				/* Mega-Hack : Queue dummy char */
				if (a & 0x80)
					Term_queue_char(vx, vy, 255, -1, 0, 0);
				else
					Term_queue_char(vx, vy, TERM_WHITE, ' ', TERM_WHITE, ' ');
			}
		}
	}
}





/*
 * Hack -- priority array (see below)
 *
 * Note that all "walls" always look like "secret doors" (see "map_info()").
 */
static const int priority_table[13][2] =
{
	/* Dark */
	{ FEAT_NONE, 2 },

	/* Floors */
	{ FEAT_FLOOR, 5 },

	/* Walls */
	{ FEAT_SECRET, 10 },

	/* Quartz */
	{ FEAT_QUARTZ, 11 },

	/* Rubble */
	{ FEAT_RUBBLE, 13 },

	/* Open doors */
	{ FEAT_OPEN, 15 },
	{ FEAT_BROKEN, 15 },

	/* Closed doors */
	{ FEAT_DOOR_HEAD + 0x00, 17 },

	/* Stairs */
	{ FEAT_LESS, 25 },
	{ FEAT_MORE, 25 },
	{ FEAT_LESS_SHAFT, 25 },
	{ FEAT_MORE_SHAFT, 25 },

	/* End */
	{ 0, 0 }
};


/*
 * Hack -- a priority function (see below)
 */
static byte priority(byte a, char c)
{
	int i, p0, p1;

	feature_type *f_ptr;

	/* Scan the table */
	for (i = 0; TRUE; i++)
	{
		/* Priority level */
		p1 = priority_table[i][1];

		/* End of table */
		if (!p1) break;

		/* Feature index */
		p0 = priority_table[i][0];

		/* Get the feature */
		f_ptr = &f_info[p0];

		/* Check character and attribute, accept matches */
		if ((f_ptr->x_char == c) && (f_ptr->x_attr == a)) return (p1);
	}

	/* Default */
	return (20);
}


/*
 * Display a "small-scale" map of the dungeon in the active Term.
 *
 * Note that this function must "disable" the special lighting effects so
 * that the "priority" function will work.
 *
 * Note the use of a specialized "priority" function to allow this function
 * to work with any graphic attr/char mappings, and the attempts to optimize
 * this function where possible.
 *
 * If "cy" and "cx" are not NULL, then returns the screen location at which
 * the player was displayed, so the cursor can be moved to that location,
 * and restricts the horizontal map size to SCREEN_WID.  Otherwise, nothing
 * is returned (obviously), and no restrictions are enforced.
 */
void display_map(int *cy, int *cx)
{
	int map_hgt, map_wid;
	int row, col;

	int x, y;

	byte ta;
	char tc;

	byte tp;

	/* Large array on the stack */
	byte mp[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

	monster_race *r_ptr = &r_info[0];

	/* Desired map height */
	map_hgt = Term->hgt - 2;
	map_wid = Term->wid - 2;

	/* Prevent accidents */
	if (map_hgt > p_ptr->cur_map_hgt) map_hgt = p_ptr->cur_map_hgt;
	if (map_wid > p_ptr->cur_map_wid) map_wid = p_ptr->cur_map_wid;

	/* Prevent accidents */
	if ((map_wid < 1) || (map_hgt < 1)) return;

	/* Nothing here */
	ta = TERM_WHITE;
	tc = ' ';

	/* Clear the priorities */
	for (y = 0; y < map_hgt; ++y)
	{
		for (x = 0; x < map_wid; ++x)
		{
			/* No priority */
			mp[y][x] = 0;
		}
	}

	/* Clear the screen (but don't force a redraw) */
	clear_from(0);

	/* Corners */
	x = map_wid + 1;
	y = map_hgt + 1;

	/* Draw the corners */
	Term_putch(0, 0, ta, '+');
	Term_putch(x, 0, ta, '+');
	Term_putch(0, y, ta, '+');
	Term_putch(x, y, ta, '+');

	/* Draw the horizontal edges */
	for (x = 1; x <= map_wid; x++)
	{
		Term_putch(x, 0, ta, '-');
		Term_putch(x, y, ta, '-');
	}

	/* Draw the vertical edges */
	for (y = 1; y <= map_hgt; y++)
	{
		Term_putch(0, y, ta, '|');
		Term_putch(x, y, ta, '|');
	}

	/* Analyze the actual map */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			row = (y * map_hgt / p_ptr->cur_map_hgt);
			col = (x * map_wid / p_ptr->cur_map_wid);

			if (use_bigtile)
				col = col & ~1;

			/* Get the attr/char at that map location */
			map_info(y, x, &ta, &tc, &ta, &tc);

			/* Get the priority of that attr/char */
			tp = priority(ta, tc);

			/* Examine boring grids */
			if ((tp == 20) && (cave_m_idx[y][x] > 0))
			{
				monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
				monster_race *r_ptr = &r_info[m_ptr->r_idx];

				/* Notice dangerous monsters */
				/* Sil-y: this may need some tweaking */
				tp = MAX(20, (int)r_ptr->level - p_ptr->depth + 20);

				/* Ignore invisible monsters */
				if (!m_ptr->ml) tp = 20;
			}

			/* Save "best" */
			if (mp[row][col] < tp)
			{
				/* Add the character */
				Term_putch(col + 1, row + 1, ta, tc);

				if (use_bigtile)
				{
					if (ta & 0x80)
						Term_putch(col + 2, row + 1, 255, -1);
					else
						Term_putch(col + 2, row + 1, TERM_WHITE, ' ');
				}

				/* Save priority */
				mp[row][col] = tp;
			}
		}
	}


	/* Player location */
	row = (p_ptr->py * map_hgt / p_ptr->cur_map_hgt);
	col = (p_ptr->px * map_wid / p_ptr->cur_map_wid);

	if (use_bigtile)
		col = col & ~1;

	/*** Make sure the player is visible ***/

	/* Get the "player" attr */
	ta = r_ptr->x_attr;

	/* Get the "player" char */
	tc = r_ptr->x_char;

	/* Draw the player */
	Term_putch(col + 1, row + 1, ta, tc);

	/* Return player location */
	if (cy != NULL) (*cy) = row + 1;
	if (cx != NULL) (*cx) = col + 1;

}


/*
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_view_map(void)
{
	int cy, cx;
	cptr prompt = "Hit any key to continue";

	/* Save screen */
	screen_save();

	/* Note */
	prt("Please wait...", 0, 0);

	/* Flush */
	Term_fresh();

	/* Clear the screen */
	Term_clear();

	/* Display the map */
	display_map(&cy, &cx);

	/* Show the prompt */
	put_str(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	/* Hilite the player */
	Term_gotoxy(cx, cy);

	/* Get any key */
	(void)inkey();

	/* Load screen */
	screen_load();
}




/*
 * Maximum number of grids in a single octant
 */
#define VINFO_MAX_GRIDS 161


/*
 * Maximum number of slopes in a single octant
 */
#define VINFO_MAX_SLOPES 126


/*
 * Mask of bits used in a single octant
 */
#define VINFO_BITS_3 0x3FFFFFFF
#define VINFO_BITS_2 0xFFFFFFFF
#define VINFO_BITS_1 0xFFFFFFFF
#define VINFO_BITS_0 0xFFFFFFFF


/*
 * Forward declare
 */
typedef struct vinfo_type vinfo_type;


/*
 * The 'vinfo_type' structure
 */
struct vinfo_type
{
	s16b grid[8];

	/* LOS slopes (up to 128) */
	u32b bits_3;
	u32b bits_2;
	u32b bits_1;
	u32b bits_0;

	/* Index of the first LOF slope */
	byte slope_fire_index1;

	/* Index of the (possible) second LOF slope */
	byte slope_fire_index2;

	vinfo_type *next_0;
	vinfo_type *next_1;

	byte y;
	byte x;
	byte d;
	byte r;
};



/*
 * The array of "vinfo" objects, initialized by "vinfo_init()"
 */
static vinfo_type vinfo[VINFO_MAX_GRIDS];




/*
 * Slope scale factor
 */
#define SCALE 100000L


/*
 * The actual slopes (for reference)
 */

/* Bit :     Slope   Grids */
/* --- :     -----   ----- */
/*   0 :      2439      21 */
/*   1 :      2564      21 */
/*   2 :      2702      21 */
/*   3 :      2857      21 */
/*   4 :      3030      21 */
/*   5 :      3225      21 */
/*   6 :      3448      21 */
/*   7 :      3703      21 */
/*   8 :      4000      21 */
/*   9 :      4347      21 */
/*  10 :      4761      21 */
/*  11 :      5263      21 */
/*  12 :      5882      21 */
/*  13 :      6666      21 */
/*  14 :      7317      22 */
/*  15 :      7692      20 */
/*  16 :      8108      21 */
/*  17 :      8571      21 */
/*  18 :      9090      20 */
/*  19 :      9677      21 */
/*  20 :     10344      21 */
/*  21 :     11111      20 */
/*  22 :     12000      21 */
/*  23 :     12820      22 */
/*  24 :     13043      22 */
/*  25 :     13513      22 */
/*  26 :     14285      20 */
/*  27 :     15151      22 */
/*  28 :     15789      22 */
/*  29 :     16129      22 */
/*  30 :     17241      22 */
/*  31 :     17647      22 */
/*  32 :     17948      23 */
/*  33 :     18518      22 */
/*  34 :     18918      22 */
/*  35 :     20000      19 */
/*  36 :     21212      22 */
/*  37 :     21739      22 */
/*  38 :     22580      22 */
/*  39 :     23076      22 */
/*  40 :     23809      22 */
/*  41 :     24137      22 */
/*  42 :     24324      23 */
/*  43 :     25714      23 */
/*  44 :     25925      23 */
/*  45 :     26315      23 */
/*  46 :     27272      22 */
/*  47 :     28000      23 */
/*  48 :     29032      23 */
/*  49 :     29411      23 */
/*  50 :     29729      24 */
/*  51 :     30434      23 */
/*  52 :     31034      23 */
/*  53 :     31428      23 */
/*  54 :     33333      18 */
/*  55 :     35483      23 */
/*  56 :     36000      23 */
/*  57 :     36842      23 */
/*  58 :     37142      24 */
/*  59 :     37931      24 */
/*  60 :     38461      24 */
/*  61 :     39130      24 */
/*  62 :     39393      24 */
/*  63 :     40740      24 */
/*  64 :     41176      24 */
/*  65 :     41935      24 */
/*  66 :     42857      23 */
/*  67 :     44000      24 */
/*  68 :     44827      24 */
/*  69 :     45454      23 */
/*  70 :     46666      24 */
/*  71 :     47368      24 */
/*  72 :     47826      24 */
/*  73 :     48148      24 */
/*  74 :     48387      24 */
/*  75 :     51515      25 */
/*  76 :     51724      25 */
/*  77 :     52000      25 */
/*  78 :     52380      25 */
/*  79 :     52941      25 */
/*  80 :     53846      25 */
/*  81 :     54838      25 */
/*  82 :     55555      24 */
/*  83 :     56521      25 */
/*  84 :     57575      26 */
/*  85 :     57894      25 */
/*  86 :     58620      25 */
/*  87 :     60000      23 */
/*  88 :     61290      25 */
/*  89 :     61904      25 */
/*  90 :     62962      25 */
/*  91 :     63636      25 */
/*  92 :     64705      25 */
/*  93 :     65217      25 */
/*  94 :     65517      25 */
/*  95 :     67741      26 */
/*  96 :     68000      26 */
/*  97 :     68421      26 */
/*  98 :     69230      26 */
/*  99 :     70370      26 */
/* 100 :     71428      25 */
/* 101 :     72413      26 */
/* 102 :     73333      26 */
/* 103 :     73913      26 */
/* 104 :     74193      27 */
/* 105 :     76000      26 */
/* 106 :     76470      26 */
/* 107 :     77777      25 */
/* 108 :     78947      26 */
/* 109 :     79310      26 */
/* 110 :     80952      26 */
/* 111 :     81818      26 */
/* 112 :     82608      26 */
/* 113 :     84000      26 */
/* 114 :     84615      26 */
/* 115 :     85185      26 */
/* 116 :     86206      27 */
/* 117 :     86666      27 */
/* 118 :     88235      27 */
/* 119 :     89473      27 */
/* 120 :     90476      27 */
/* 121 :     91304      27 */
/* 122 :     92000      27 */
/* 123 :     92592      27 */
/* 124 :     93103      28 */
/* 125 :    100000      13 */



/*
 * Forward declare
 */
typedef struct vinfo_hack vinfo_hack;



/*
 * Temporary data used by "vinfo_init()"
 *
 *	- Number of line of sight slopes
 *
 *	- Slope values
 *
 *	- Slope range for each grid
 */
struct vinfo_hack
{
	int num_slopes;

	long slopes[VINFO_MAX_SLOPES];

	long slopes_min[MAX_SIGHT+1][MAX_SIGHT+1];
	long slopes_max[MAX_SIGHT+1][MAX_SIGHT+1];
};



/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static bool ang_sort_comp_hook_longs(const void *u, const void *v, int a, int b)
{
	long *x = (long*)(u);

	/* Unused parameter */
	(void)v;

	return (x[a] <= x[b]);
}


/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static void ang_sort_swap_hook_longs(void *u, void *v, int a, int b)
{
	long *x = (long*)(u);

	long temp;

	/* Unused parameter */
	(void)v;

	/* Swap */
	temp = x[a];
	x[a] = x[b];
	x[b] = temp;
}


/*
 * Save a slope
 */
static void vinfo_init_aux(vinfo_hack *hack, int y, int x, long m)
{
	int i;

	/* Handle "legal" slopes */
	if ((m > 0) && (m <= SCALE))
	{
		/* Look for that slope */
		for (i = 0; i < hack->num_slopes; i++)
		{
			if (hack->slopes[i] == m) break;
		}

		/* New slope */
		if (i == hack->num_slopes)
		{
			/* Paranoia */
			if (hack->num_slopes >= VINFO_MAX_SLOPES)
			{
				quit_fmt("Too many LOS slopes (%d)!", VINFO_MAX_SLOPES);
			}

			/* Save the slope, increment count */
			hack->slopes[hack->num_slopes++] = m;
		}
	}

	/* Track slope range */
	if (hack->slopes_min[y][x] > m) hack->slopes_min[y][x] = m;
	if (hack->slopes_max[y][x] < m) hack->slopes_max[y][x] = m;

}


/*
 * Initialize the "vinfo" array
 *
 * Full Octagon (radius 20), Grids=1149
 *
 * Quadrant (south east), Grids=308, Slopes=251
 *
 * Octant (east then south), Grids=161, Slopes=126
 *
 * This function assumes that VINFO_MAX_GRIDS and VINFO_MAX_SLOPES
 * have the correct values, which can be derived by setting them to
 * a number which is too high, running this function, and using the
 * error messages to obtain the correct values.
 */
errr vinfo_init(void)
{
	int i, g;
	int y, x;

	long m;

	vinfo_hack *hack;

	int num_grids = 0;

	int queue_head = 0;
	int queue_tail = 0;
	vinfo_type *queue[VINFO_MAX_GRIDS*2];


	/* Make hack */
	MAKE(hack, vinfo_hack);


	/* Analyze grids */
	for (y = 0; y <= MAX_SIGHT; ++y)
	{
		for (x = y; x <= MAX_SIGHT; ++x)
		{
			/* Skip grids which are out of sight range */
			if (distance(0, 0, y, x) > MAX_SIGHT) continue;

			/* Default slope range */
			hack->slopes_min[y][x] = 999999999;
			hack->slopes_max[y][x] = 0;

			/* Paranoia */
			if (num_grids >= VINFO_MAX_GRIDS)
			{
				quit_fmt("Too many grids (%d >= %d)!",
					num_grids, VINFO_MAX_GRIDS);
			}

			/* Count grids */
			num_grids++;

			/* Slope to the top right corner */
			m = SCALE * (1000L * y - 500) / (1000L * x + 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to top left corner */
			m = SCALE * (1000L * y - 500) / (1000L * x - 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to bottom right corner */
			m = SCALE * (1000L * y + 500) / (1000L * x + 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to bottom left corner */
			m = SCALE * (1000L * y + 500) / (1000L * x - 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);
		}
	}

	/* Enforce maximal efficiency (grids) */
	if (num_grids < VINFO_MAX_GRIDS)
	{
		quit_fmt("Too few grids (%d < %d)!",
			num_grids, VINFO_MAX_GRIDS);
	}

	/* Enforce maximal efficiency (line of sight slopes) */
	if (hack->num_slopes < VINFO_MAX_SLOPES)
	{
		quit_fmt("Too few LOS slopes (%d < %d)!",
			hack->num_slopes, VINFO_MAX_SLOPES);
	}


	/* Sort slopes numerically */
	ang_sort_comp = ang_sort_comp_hook_longs;

	/* Sort slopes numerically */
	ang_sort_swap = ang_sort_swap_hook_longs;

	/* Sort the (unique) LOS slopes */
	ang_sort(hack->slopes, NULL, hack->num_slopes);


	/* Enqueue player grid */
	queue[queue_tail++] = &vinfo[0];

	/* Process queue */
	while (queue_head < queue_tail)
	{
		int e;

		/* Index */
		e = queue_head++;

		/* Main Grid */
		g = vinfo[e].grid[0];

		/* Location */
		y = GRID_Y(g);
		x = GRID_X(g);


		/* Compute grid offsets */
		vinfo[e].grid[0] = GRID(+y,+x);
		vinfo[e].grid[1] = GRID(+x,+y);
		vinfo[e].grid[2] = GRID(+x,-y);
		vinfo[e].grid[3] = GRID(+y,-x);
		vinfo[e].grid[4] = GRID(-y,-x);
		vinfo[e].grid[5] = GRID(-x,-y);
		vinfo[e].grid[6] = GRID(-x,+y);
		vinfo[e].grid[7] = GRID(-y,+x);


		/* Skip player grid */
		if (e > 0)
		{
			long slope_fire;

			long tmp0 = 0;
			long tmp1 = 0;
			long tmp2 = 999999L;

			/* Determine LOF slope for this grid */
			if (x == 0) slope_fire = SCALE;
			else slope_fire = SCALE * (1000L * y) / (1000L * x);

			/* Analyze LOS slopes */
			for (i = 0; i < hack->num_slopes; ++i)
			{
				m = hack->slopes[i];

				/* Memorize intersecting slopes */
				if ((hack->slopes_min[y][x] < m) &&
				    (hack->slopes_max[y][x] > m))
				{
					/* Add it to the LOS slope set */
					switch (i / 32)
					{
						case 3: vinfo[e].bits_3 |= (1L << (i % 32)); break;
						case 2: vinfo[e].bits_2 |= (1L << (i % 32)); break;
						case 1: vinfo[e].bits_1 |= (1L << (i % 32)); break;
						case 0: vinfo[e].bits_0 |= (1L << (i % 32)); break;
					}

					/* Check for exact match with the LOF slope */
					if (m == slope_fire) tmp0 = i;

					/* Remember index of nearest LOS slope < than LOF slope */
					else if ((m < slope_fire) && (m > tmp1)) tmp1 = i;

					/* Remember index of nearest LOS slope > than LOF slope */
					else if ((m > slope_fire) && (m < tmp2)) tmp2 = i;
				}
			}

			/* There is a perfect match with one of the LOS slopes */
			if (tmp0)
			{
				/* Save the (unique) slope */
				vinfo[e].slope_fire_index1 = tmp0;

				/* Mark the other empty */
				vinfo[e].slope_fire_index2 = 0;
			}

			/* The LOF slope lies between two LOS slopes */
			else
			{
				/* Save the first slope */
				vinfo[e].slope_fire_index1 = tmp1;

				/* Save the second slope */
				vinfo[e].slope_fire_index2 = tmp2;
			}
		}

		/* Default */
		vinfo[e].next_0 = &vinfo[0];

		/* Grid next child */
		if (distance(0, 0, y, x+1) <= MAX_SIGHT)
		{
			g = GRID(y,x+1);

			if (queue[queue_tail-1]->grid[0] != g)
			{
				vinfo[queue_tail].grid[0] = g;
				queue[queue_tail] = &vinfo[queue_tail];
				queue_tail++;
			}

			vinfo[e].next_0 = &vinfo[queue_tail - 1];
		}


		/* Default */
		vinfo[e].next_1 = &vinfo[0];

		/* Grid diag child */
		if (distance(0, 0, y+1, x+1) <= MAX_SIGHT)
		{
			g = GRID(y+1,x+1);

			if (queue[queue_tail-1]->grid[0] != g)
			{
				vinfo[queue_tail].grid[0] = g;
				queue[queue_tail] = &vinfo[queue_tail];
				queue_tail++;
			}

			vinfo[e].next_1 = &vinfo[queue_tail - 1];
		}


		/* Hack -- main diagonal has special children */
		if (y == x) vinfo[e].next_0 = vinfo[e].next_1;


		/* Grid coordinates, approximate distance  */
		vinfo[e].y = y;
		vinfo[e].x = x;
		vinfo[e].d = ((y > x) ? (y + x/2) : (x + y/2));
		vinfo[e].r = ((!y) ? x : (!x) ? y : (y == x) ? y : 0);
	}

	/* Verify maximal bits XXX XXX XXX */
	if (((vinfo[1].bits_3 | vinfo[2].bits_3) != VINFO_BITS_3) ||
	    ((vinfo[1].bits_2 | vinfo[2].bits_2) != VINFO_BITS_2) ||
	    ((vinfo[1].bits_1 | vinfo[2].bits_1) != VINFO_BITS_1) ||
	    ((vinfo[1].bits_0 | vinfo[2].bits_0) != VINFO_BITS_0))
	{
		quit("Incorrect bit masks!");
	}


	/* Kill hack */
	KILL(hack);


	/* Success */
	return (0);
}



/*
 * Forget the "CAVE_VIEW" grids, redrawing as needed
 */
void forget_view(void)
{
	int i, g;

	int fast_view_n = view_n;
	u16b *fast_view_g = view_g;

	u16b *fast_cave_info = &cave_info[0][0];


	/* None to forget */
	if (!fast_view_n) return;

	/* Clear them all */
	for (i = 0; i < fast_view_n; i++)
	{
		int y, x;

		/* Grid */
		g = fast_view_g[i];

		/* Location */
		y = GRID_Y(g);
		x = GRID_X(g);

		/* Clear "CAVE_VIEW" and "CAVE_SEEN" flags */
		fast_cave_info[g] &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

		/* Clear "CAVE_LITE" flag */
		/* fast_cave_info[g] &= ~(CAVE_LITE); */

		/* Redraw */
		lite_spot(y, x);
	}

	/* None left */
	fast_view_n = 0;

	/* Save 'view_n' */
	view_n = fast_view_n;
}


bool same_side_of_wall_as_player(int y, int x, int fy, int fx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;
	
	bool same = TRUE;
		
	// if one above and one below
	if (((py <= y) && (fy >= y)) || ((py >= y) && (fy <= y)))
	{
		if ((px < x) && (fx < x))
		{
			if (cave_info[y][x-1] & (CAVE_WALL))
			{
				same = FALSE;
			}
		}
		else if ((px > x) && (fx > x))
		{
			if (cave_info[y][x+1] & (CAVE_WALL))
			{
				same = FALSE;
			}
		}
		else
		{
			same = FALSE;
		}
	}

	// if one left and one right
	if (((px <= x) && (fx >= x)) || ((px >= x) && (fx <= x)))
	{
		// if both above
		if ((py < y) && (fy < y))
		{
			if (cave_info[y-1][x] & (CAVE_WALL))
			{
				same = FALSE;
			}
		}
		else if ((py > y) && (fy > y))
		{
			if (cave_info[y+1][x] & (CAVE_WALL))
			{
				same = FALSE;
			}
		}
		else
		{
			same = FALSE;
		}
	}
	
	return (same);
}


/*
 * Calculate the complete field of view using a new algorithm
 *
 * If "view_g" and "temp_g" were global pointers to arrays of grids, as
 * opposed to actual arrays of grids, then we could be more efficient by
 * using "pointer swapping".
 *
 * Note the following idiom, which is used in the function below.
 * This idiom processes each "octant" of the field of view, in a
 * clockwise manner, starting with the east strip, south side,
 * and for each octant, allows a simple calculation to set "g"
 * equal to the proper grids, relative to "pg", in the octant.
 *
 *   for (o2 = 0; o2 < 8; o2++)
 *   ...
 *         g = pg + p->grid[o2];
 *   ...
 *
 *
 * Normally, vision along the major axes is more likely than vision
 * along the diagonal axes, so we check the bits corresponding to
 * the lines of sight near the major axes first.
 *
 * We use the "temp_g" array (and the "CAVE_TEMP" flag) to keep track of
 * which grids were previously marked "CAVE_SEEN", since only those grids
 * whose "CAVE_SEEN" value changes during this routine must be redrawn.
 *
 * This function is now responsible for maintaining the "CAVE_SEEN"
 * flags as well as the "CAVE_VIEW" flags, which is good, because
 * the only grids which normally need to be memorized and/or redrawn
 * are the ones whose "CAVE_SEEN" flag changes during this routine.
 *
 * Basically, this function divides the "octagon of view" into octants of
 * grids (where grids on the main axes and diagonal axes are "shared" by
 * two octants), and processes each octant one at a time, processing each
 * octant one grid at a time, processing only those grids which "might" be
 * viewable, and setting the "CAVE_VIEW" flag for each grid for which there
 * is an (unobstructed) line of sight from the center of the player grid to
 * any internal point in the grid (and collecting these "CAVE_VIEW" grids
 * into the "view_g" array), and setting the "CAVE_SEEN" flag for the grid
 * if, in addition, the grid is "illuminated" in some way.
 *
 * This function relies on a theorem (suggested and proven by Mat Hostetter)
 * which states that in each octant of a field of view, a given grid will
 * be "intersected" by one or more unobstructed "lines of sight" from the
 * center of the player grid if and only if it is "intersected" by at least
 * one such unobstructed "line of sight" which passes directly through some
 * corner of some grid in the octant which is not shared by any other octant.
 * The proof is based on the fact that there are at least three significant
 * lines of sight involving any non-shared grid in any octant, one which
 * intersects the grid and passes though the corner of the grid closest to
 * the player, and two which "brush" the grid, passing through the "outer"
 * corners of the grid, and that any line of sight which intersects a grid
 * without passing through the corner of a grid in the octant can be "slid"
 * slowly towards the corner of the grid closest to the player, until it
 * either reaches it or until it brushes the corner of another grid which
 * is closer to the player, and in either case, the existanc of a suitable
 * line of sight is thus demonstrated.
 *
 * It turns out that in each octant of the radius 20 "octagon of view",
 * there are 161 grids (with 128 not shared by any other octant), and there
 * are exactly 126 distinct "lines of sight" passing from the center of the
 * player grid through any corner of any non-shared grid in the octant.  To
 * determine if a grid is "viewable" by the player, therefore, you need to
 * simply show that one of these 126 lines of sight intersects the grid but
 * does not intersect any wall grid closer to the player.  So we simply use
 * a bit vector with 126 bits to represent the set of interesting lines of
 * sight which have not yet been obstructed by wall grids, and then we scan
 * all the grids in the octant, moving outwards from the player grid.  For
 * each grid, if any of the lines of sight which intersect that grid have not
 * yet been obstructed, then the grid is viewable.  Furthermore, if the grid
 * is a wall grid, then all of the lines of sight which intersect the grid
 * should be marked as obstructed for future reference.  Also, we only need
 * to check those grids for whom at least one of the "parents" was a viewable
 * non-wall grid, where the parents include the two grids touching the grid
 * but closer to the player grid (one adjacent, and one diagonal).  For the
 * bit vector, we simply use 4 32-bit integers.  All of the static values
 * which are needed by this function are stored in the large "vinfo" array
 * (above), which is machine generated by another program.  XXX XXX XXX
 *
 * Hack -- The queue must be able to hold more than VINFO_MAX_GRIDS grids
 * because the grids at the edge of the field of view use "grid zero" as
 * their children, and the queue must be able to hold several of these
 * special grids.  Because the actual number of required grids is bizarre,
 * we simply allocate twice as many as we would normally need.  XXX XXX XXX
 */
void update_view(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int pg = GRID(py,px);

	int i, j, g, o2;

	int player_light = p_ptr->cur_light;
	int player_rad = ABS(player_light);

	int fy, fx, k;

	int fast_view_n = view_n;
	u16b *fast_view_g = view_g;

	int fast_temp_n = 0;
	u16b *fast_temp_g = temp_g;

	u16b *fast_cave_info = &cave_info[0][0];

	u16b info;
	
	bool in_pit = cave_pit_bold(p_ptr->py, p_ptr->px) && !p_ptr->leaping;

	/*** Step 0 -- Begin ***/
	
	/* Save the old "view" grids for later */
	for (i = 0; i < fast_view_n; i++)
	{
		/* Grid */
		g = fast_view_g[i];

		/* Get grid info */
		info = fast_cave_info[g];

		/* Save "CAVE_SEEN" grids */
        if (info & (CAVE_SEEN))
		{
			/* Set "CAVE_TEMP" flag */
			info |= (CAVE_TEMP);

			/* Save grid for later */
			fast_temp_g[fast_temp_n++] = g;
		}

		/* Clear "CAVE_VIEW", "CAVE_SEEN" & cave_fire flags */
		info &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

		/* Clear "CAVE_LITE" flag */
		/* info &= ~(CAVE_LITE); */

		/* Save cave info */
		fast_cave_info[g] = info;
	}

	/* Reset the "view" array */
	fast_view_n = 0;


	/*** Step 1 -- player grid ***/

	/* Player grid */
	g = pg;

	/* Get grid info */
	info = fast_cave_info[g];

	/* Assume viewable */
	info |= (CAVE_VIEW | CAVE_FIRE);

	/* Save cave info */
	fast_cave_info[g] = info;

	/* Save in array */
	fast_view_g[fast_view_n++] = g;

	/*** Step 2 -- octants ***/

	/* Scan each octant */
	for (o2 = 0; o2 < 8; o2++)
	{
		vinfo_type *p;

		/* Last added */
		vinfo_type *last = &vinfo[0];

		/* Grid queue */
		int queue_head = 0;
		int queue_tail = 0;
		vinfo_type *queue[VINFO_MAX_GRIDS*2];

		/* Slope bit vector */
		u32b bits0 = VINFO_BITS_0;
		u32b bits1 = VINFO_BITS_1;
		u32b bits2 = VINFO_BITS_2;
		u32b bits3 = VINFO_BITS_3;

		/* Reset queue */
		queue_head = queue_tail = 0;

		/* Initial grids */
		queue[queue_tail++] = &vinfo[1];
		queue[queue_tail++] = &vinfo[2];

		/* Process queue */
		while (queue_head < queue_tail)
		{
			/* Assume no line of fire */
			bool line_fire = FALSE;

			/* Dequeue next grid */
			p = queue[queue_head++];

			/* Check bits */
			if ((bits0 & (p->bits_0)) ||
			    (bits1 & (p->bits_1)) ||
			    (bits2 & (p->bits_2)) ||
			    (bits3 & (p->bits_3)))
			{
				/* Extract grid value XXX XXX XXX */
				g = pg + p->grid[o2];

				/* Get grid info */
				info = fast_cave_info[g];

				/* Check for first possible line of fire */
				i = p->slope_fire_index1;

				/* Check line(s) of fire */
				while (TRUE)
				{
					switch (i / 32)
					{
						case 3:
						{
							if (bits3 & (1L << (i % 32))) line_fire = TRUE;
							break;
						}
						case 2:
						{
							if (bits2 & (1L << (i % 32))) line_fire = TRUE;
							break;
						}
						case 1:
						{
							if (bits1 & (1L << (i % 32))) line_fire = TRUE;
							break;
						}
						case 0:
						{
							if (bits0 & (1L << (i % 32))) line_fire = TRUE;
							break;
						}
					}

					/* Check second LOF slope if necessary */
					if ((!p->slope_fire_index2) || (line_fire) ||
					    (i == p->slope_fire_index2))
					{
						break;
					}

					/* Check second possible line of fire */
					i = p->slope_fire_index2;
				}

				/* Note line of fire */
				if (line_fire)
				{
					info |= (CAVE_FIRE);
				}

				/* Handle wall */
				if (info & (CAVE_WALL))
				{
					/* Clear bits */
					bits0 &= ~(p->bits_0);
					bits1 &= ~(p->bits_1);
					bits2 &= ~(p->bits_2);
					bits3 &= ~(p->bits_3);

					/* Newly viewable wall */
					if (!(info & (CAVE_VIEW)))
					{
						/* Mark as viewable */
						info |= (CAVE_VIEW);

						/* Torch-lit grids */
						if (p->d <= player_light)
						{
							/* Mark as "CAVE_SEEN" */
							info |= (CAVE_SEEN);
														

							/* Mark as "CAVE_LITE" */
							/* info |= (CAVE_LITE); */
						}

						/* Perma-lit grids */
						else if (info & (CAVE_GLOW))
						{
							int y = GRID_Y(g);
							int x = GRID_X(g);

							/* Hack -- move towards player */
							int yy = (y < py) ? (y + 1) : (y > py) ? (y - 1) : y;
							int xx = (x < px) ? (x + 1) : (x > px) ? (x - 1) : x;

							/* Check for "complex" illumination */
							if ((!(cave_info[yy][xx] & (CAVE_WALL)) &&
							      (cave_info[yy][xx] & (CAVE_GLOW))) ||
							    (!(cave_info[y][xx] & (CAVE_WALL)) &&
							      (cave_info[y][xx] & (CAVE_GLOW))) ||
							    (!(cave_info[yy][x] & (CAVE_WALL)) &&
							      (cave_info[yy][x] & (CAVE_GLOW))))
							{
								/* Mark as seen */
								info |= (CAVE_SEEN);
							}

						}


						/* Save in array */
						fast_view_g[fast_view_n++] = g;
					}
				}

				/* Handle non-wall */
				else
				{
					/* Enqueue child */
					if (last != p->next_0)
					{
						queue[queue_tail++] = last = p->next_0;
					}

					/* Enqueue child */
					if (last != p->next_1)
					{
						queue[queue_tail++] = last = p->next_1;
					}

					/* Newly viewable non-wall */
					if (!(info & (CAVE_VIEW)))
					{
						/* Mark as "viewable" */
						info |= (CAVE_VIEW);

						/* Torch-lit grids */
						if (p->d <= player_light)
						{
							/* Mark as "CAVE_SEEN" */
							info |= (CAVE_SEEN);

							/* Mark as "CAVE_LITE" */
							/* info |= (CAVE_LITE); */
						}

						/* Perma-lit grids */
						else if (info & (CAVE_GLOW))
						{
							/* Mark as "CAVE_SEEN" */
							info |= (CAVE_SEEN);
						}

						/* Save in array */
						fast_view_g[fast_view_n++] = g;
					}
				}

				/* Save cave info */
				fast_cave_info[g] = info;

			}
		}
	}
	
	// restrict the view of players in pits
	if (in_pit)
	{
		for (i = 0; i < fast_view_n; i++)
		{
			int y, x;
			
			g = fast_view_g[i];
			
			y = GRID_Y(g);
			x = GRID_X(g);
			
			// quick check to see if the square is not-adjacent
			if ((abs(y-py)>1) || (abs(x-px)>1))
			{
				fast_cave_info[g] &= ~(CAVE_SEEN | CAVE_VIEW | CAVE_FIRE);
			}
		}
	}

	/*** Step 2b -- handle the Sil-style light ***/
	
	/* this is the only step that even looks at these light values */

	// Sil: get the starting light values based on permanent light (and backup old values)
	for (i = 0; i < MAX_DUNGEON_HGT; i++)
	{
		for (j = 0; j < MAX_DUNGEON_WID; j++)
		{			
			if (cave_info[i][j] & (CAVE_GLOW))
			{
				cave_light[i][j] = 1;
			}
			else
			{
				cave_light[i][j] = 0;
			}
		}
	}
	
	// Sil: update the light values with the torch/lantern light
	for (i = -player_rad; i <= player_rad; i++)
	{
		for (j = -player_rad; j <= player_rad; j++)
		{
			int dist = distance(0,0,i,j);
			int bonus_light = 0;
			
			if (p_ptr->active_ability[S_WIL][WIL_INNER_LIGHT])
			{
				bonus_light = 1;
			}

			
			// Don't darken/brighten the centre square too much
			// if (dist == 0) dist++;
			
			// Sil-y: previously used los(py,px,py+i,px+j) rather than CAVE_VIEW in the below, but this seems better
			if (in_bounds(py+i,px+j) && (dist <= player_rad) && (cave_info[py+i][px+j] & (CAVE_VIEW)))
			{
				if (player_light > 0)
				{
					cave_light[py+i][px+j] += player_rad + 1 - dist + bonus_light;
				}
				if (player_light < 0)
				{
					cave_light[py+i][px+j] -= player_rad + 1 - dist + bonus_light;
				}				
			}
		}
	}
	
	
	// Sil: generate darkness or light for the all the monsters
    for ( k = 1; k < mon_max; k++) // Sil-x: changed to mon_max from z_info->m_max. I think I'm right about this
    {
        /* Check the k'th monster */
        monster_type *m_ptr = &mon_list[k];
        monster_race *r_ptr = &r_info[m_ptr->r_idx];
		
        /* Skip dead monsters */
        if (!m_ptr->r_idx) continue;
		
		/* Access the location */
        fx = m_ptr->fx;
        fy = m_ptr->fy;
		
		int mon_light = r_ptr->light;
		int mon_rad = ABS(mon_light);
		
		bool glow = r_ptr->flags2 & (RF2_GLOW);
		
		// Do darkness or light for this monster
        if (mon_rad > 0)
		{
			for (i = -mon_rad; i <= mon_rad; i++)
			{
				for (j = -mon_rad; j <= mon_rad; j++)
				{
					int y = fy+i;
					int x = fx+j;
					
					int dist = distance(0,0,i,j);

					g = GRID(y,x);

					info = fast_cave_info[g];
					
					// Don't darken/brighten the centre square too much
					// if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;
					
					if (in_bounds(y,x) && (dist <= mon_rad) && los(fy,fx,y,x))
					{
						// Only set it if the player can see it
						if ((distance(py,px,y,x) <= MAX_SIGHT) && (info & (CAVE_VIEW)))
						{
							if (((cave_info[y][x] & (CAVE_WALL)) && same_side_of_wall_as_player(y,x,fy,fx))
								 || !(cave_info[y][x] & (CAVE_WALL)))
							{
								// Glowing monsters lighten their own square
								if ((i == 0) && (j == 0) && glow)
								{
									cave_light[y][x] += 1;
									
									/* Mark as seen */
									info |= (CAVE_SEEN);									
									
									/* Save cave info */
									fast_cave_info[g] = info;
									
									/* Save in array */
									fast_view_g[fast_view_n++] = g;
								}
									
								// Brighten the square
								else if (mon_light > 0)
								{
									cave_light[y][x] += mon_rad + 1 - dist;
									
									/* Mark as seen */
									info |= (CAVE_SEEN);									
									
									/* Save cave info */
									fast_cave_info[g] = info;
									
									/* Save in array */
									fast_view_g[fast_view_n++] = g;
								}
								// Darken the square
								else
								{
									cave_light[y][x] -= mon_rad + 1 - dist;
								}
							}
						}						
					}
				}
			}
		}
    }
	
	
	// Sil: generate darkness or light for the all the objects
	for (k = 1; k < o_max; k++)
	{
		/* Get the next object from the dungeon */
		object_type *o_ptr = &o_list[k];
		
		u32b f1, f2, f3;
		
		int obj_light = 0;
		int obj_rad;
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		/* Access the location */
        fx = o_ptr->ix;
        fy = o_ptr->iy;
		
		object_flags(o_ptr, &f1, &f2, &f3);
		
		/* does this item glow? */
		if (f2 & TR2_LIGHT)
		{ 
			// objects with the Light flag glow on the ground unless they are torches or lanterns
			if (!((o_ptr->tval == TV_LIGHT) && ((o_ptr->sval == SV_LIGHT_TORCH) || (o_ptr->sval == SV_LIGHT_LANTERN)))) obj_light++;
		}

		/* is it a glowing weapon? */
		if (weapon_glows(o_ptr))
		{
			obj_light++;
		}

		// Need to redraw any weapons that may have started/stopped glowing
		if (wield_slot(o_ptr) == INVEN_WIELD)
		{
			if ((fy > 0) && (fx > 0) && (cave_info[fy][fx] & (CAVE_VIEW)))
			{
				lite_spot(fy,fx);
			}
		}
		
		/* does this item create darkness? */
		if ((f2 & TR2_DARKNESS) && (o_ptr->tval != TV_LIGHT)) obj_light--;
		
		/* Examine actual light */
		if (o_ptr->tval == TV_LIGHT)
		{
			/* Some items provide permanent, bright, light */
			if (o_ptr->sval == SV_LIGHT_FEANORIAN)
			{
				obj_light += RADIUS_FEANORIAN;
			}
			if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
			{
				obj_light += RADIUS_LESSER_JEWEL;
			}
			if (o_ptr->sval == SV_LIGHT_SILMARIL)
			{
				obj_light += RADIUS_SILMARIL;
			}
		}
		
		// The Iron Crown also glows
		if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
		{ 
			obj_light += o_ptr->pval;
		}
		
		obj_rad = ABS(obj_light);
		
		// Do darkness or light for this object
        if (obj_rad > 0)
		{
			for (i = -obj_rad; i <= obj_rad; i++)
			{
				for (j = -obj_rad; j <= obj_rad; j++)
				{
					int y = fy+i;
					int x = fx+j;
					
					int dist = distance(0,0,i,j);
					
					g = GRID(y,x);
					
					info = fast_cave_info[g];
					
					// Don't darken/brighten the centre square too much
					// if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;
					
					if (in_bounds(y,x) && (dist <= obj_rad) && los(fy,fx,y,x))
					{
						// Only set it if the player can see it
						if ((distance(py,px,y,x) <= MAX_SIGHT) && (info & (CAVE_VIEW)))
						{
							if (((cave_info[y][x] & (CAVE_WALL)) && same_side_of_wall_as_player(y,x,fy,fx))
								|| !(cave_info[y][x] & (CAVE_WALL)))
							{
								// Brighten the square
								if (obj_light > 0)
								{
									cave_light[y][x] += obj_rad + 1 - dist;
									
									/* Mark as seen */
									info |= (CAVE_SEEN);									
									
									/* Save cave info */
									fast_cave_info[g] = info;
									
									/* Save in array */
									fast_view_g[fast_view_n++] = g;
								}
								// Darken the square
								else
								{
									cave_light[y][x] -= obj_rad + 1 - dist;
								}
							}
						}						
					}
				}
			}
		}
    }
	
		
	
	// Sil: this removes the 'seen' flag from squares that have zero or less light
	for (i = 0; i < fast_view_n; i++)
	{
		int y, x;
		
		g = fast_view_g[i];
		
		y = GRID_Y(g);
		x = GRID_X(g);
		
		// Remove 'seen' flag from squares that have zero or less light
		if (cave_light[y][x] <= 0)
		{
			fast_cave_info[g] &= ~(CAVE_SEEN);
			
			/* Hack -- Forget "boring" grids */
			if (cave_floorlike_bold(y,x) && (cave_info[y][x] & (CAVE_GLOW)))
			{
				/* Forget */
				cave_info[y][x] &= ~(CAVE_MARK);
			}
		}
	}
	

	/*** Step 3 -- Complete the algorithm ***/

	/* Handle blindness */
	if (p_ptr->blind)
	{
		/* Process "new" grids */
		for (i = 0; i < fast_view_n; i++)
		{
			/* Grid */
			g = fast_view_g[i];

			/* Grid cannot be "CAVE_SEEN" */
			fast_cave_info[g] &= ~(CAVE_SEEN);
		}
	}

	/* Process "new" grids */
	for (i = 0; i < fast_view_n; i++)
	{
		/* Grid */
		g = fast_view_g[i];

		/* Get grid info */
		info = fast_cave_info[g];

		/* Was not "CAVE_SEEN", is now "CAVE_SEEN" */
		if ((info & (CAVE_SEEN)) && !(info & (CAVE_TEMP)))
		{
			int y, x;

			/* Location */
			y = GRID_Y(g);
			x = GRID_X(g);

			/* Note */
			note_spot(y, x);

			/* Redraw */
			lite_spot(y, x);
		}
	}

	// Sil-y: for some reason we need to update the visibility info for monsters (the ->ml attribute).
	//        Otherwise, dark producing monsters are occasionally visible when they are following you.
	for (i = 1; i < mon_max; i++)
	{
		monster_type *m_ptr = &mon_list[i];
		
		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;
		
		/* Update the monster */
		update_mon(i, FALSE);
	}
	
	/* Process "old" grids */
	for (i = 0; i < fast_temp_n; i++)
	{
		/* Grid */
		g = fast_temp_g[i];

		/* Get grid info */
		info = fast_cave_info[g];

		/* Clear "CAVE_TEMP" flag */
		info &= ~(CAVE_TEMP);

		/* Save cave info */
		fast_cave_info[g] = info;

		/* Was "CAVE_SEEN", is now not "CAVE_SEEN" */
		if (!(info & (CAVE_SEEN)))
		{
			int y, x;

			/* Location */
			y = GRID_Y(g);
			x = GRID_X(g);

			/* Redraw */
			lite_spot(y, x);
		}
	}

	// Sil: this is needed to properly darken certain spots
	for (i = 0; i < MAX_DUNGEON_HGT; i++)
	{
		for (j = 0; j < MAX_DUNGEON_WID; j++)
		{
			if ((cave_info[i][j] & (CAVE_GLOW)) && (cave_info[i][j] & (CAVE_VIEW)) && 
			    !(cave_info[i][j] & (CAVE_SEEN)))
			{
				/* Redraw */
				lite_spot(i, j);
			}
		}
	}

	// Sil: disturb the player when the lighting changes unexpectedly
	for (i = py - MAX_SIGHT; i <= py + MAX_SIGHT; i++)
	{
		for (j = px - MAX_SIGHT; j <= px + MAX_SIGHT; j++)
		{
			if (in_bounds_fully(i,j) && ((i != py) || (j != px)))
			{
				info = cave_info[i][j];
				
				if ((info & (CAVE_OLD_VIEW)) && (info & (CAVE_VIEW)))
				{
					// check recently darkened squares
					if ((info & (CAVE_OLD_LIT)) && (cave_light[i][j] <= 0))
					{
						// if they didn't just fall out of torch radius
						if (!((info & (CAVE_OLD_TORCH)) && (distance(py, px, i, j) > player_rad)))
						{
							// ignore in some negative light situations (not a perfect fix, but good enough)
							if ((p_ptr->old_light >= 0) || (distance(py, px, i, j) > player_rad + 1))
							{
								disturb(0, 0);
								//msg_format("(%d,%d) Disturbed on loss of light.",i,j);
							}
						}
					}
					
					// check recently lit squares
					if (!(info & (CAVE_OLD_LIT)) && (cave_light[i][j] > 0))
					{
						// if they didn't just enter torch radius
						if (!(!(info & (CAVE_OLD_TORCH)) && (distance(py, px, i, j) <= player_rad)))
						{
							// ignore in some negative light situations (not a perfect fix, but good enough)
							if ((p_ptr->old_light >= 0) || (distance(py, px, i, j) > player_rad + 1))
							{
								disturb(0, 0);
								//msg_format("(%d,%d) Disturbed on gain of light.",i,j);
							}
						}
					}
				}
			}
		}
	}

	// Sil: record information about view and lighting for next call to update_view()
	//      so that they player can be disturbed when lighting changes unexpectedly
	p_ptr->old_light = p_ptr->cur_light;
	for (i = 0; i < MAX_DUNGEON_HGT; i++)
	{
		for (j = 0; j < MAX_DUNGEON_WID; j++)
		{		
			// store view information for last turn
			if (cave_info[i][j] & (CAVE_VIEW))
			{
				cave_info[i][j] |= (CAVE_OLD_VIEW);
			}
			else
			{
				cave_info[i][j] &= ~(CAVE_OLD_VIEW);
			}
			
			// store lighting information for last turn
			if (cave_light[i][j] > 0)
			{
				cave_info[i][j] |= (CAVE_OLD_LIT);
			}
			else
			{
				cave_info[i][j] &= ~(CAVE_OLD_LIT);
			}
			
			// store 'torchlight' information for last turn
			if (distance(py, px, i, j) <= p_ptr->old_light)
			{
				cave_info[i][j] |= (CAVE_OLD_TORCH);
			}
			else
			{
				cave_info[i][j] &= ~(CAVE_OLD_TORCH);
			}
		}
	}
	
	/* Save 'view_n' */
	view_n = fast_view_n;
}

/*
 * Determines how far a grid is from the source using the given flow.
 *
 * Given some changes to the code, it is currently a rather unnecessary
 * abstraction which could be replaced with a direct array reference if desired.
 */
int flow_dist(int which_flow, int y, int x)
{
	int dist;
	
	dist = cave_cost[which_flow][y][x];
		
	return (dist);
}


/*
 * Sil needs various 'flows', which are arrays of the same size as the map,
 * with a number for each map square.
 *
 * One of these flows is used to represent the from the player noise at each location.
 * Another is used to represent the noise from a particular monster.
 *
 * 200 of them are used for alert monster pathfinding, representing the shortest route
 * each monster could take to get to the player.
 *
 * 100 of them are used for the pathfinding of unwary monsters who move in their
 * initial groups to various locations around the map.
 *
 * There is an intermediate data-structure called the flow table, which is 3-dimensional.
 * The first dimension allows the
 * table to both store and overwrite grids safely.  The second indicates
 * whether this value is that for x or for y.  The third is the number
 * of grids able to be stored at any flow distance.
 *
 * Note that the noise is generated around the centre cy, cx
 * This is often the player, but can be a monster (for FLOW_MONSTER_NOISE)
 *
 */

void update_flow(int cy, int cx, int which_flow)
{
	int cost;

	int i, d;
	byte y, x, y2, x2;
	int last_index;
	int grid_count = 0;

	/* Note where we get information from, and where we overwrite */
	int this_cycle = 0;
	int next_cycle = 1;
    
    bool monster_flow = FALSE;
    bool bash = FALSE;
    bool found = FALSE;
    
    monster_type *m_ptr = NULL; // default to soothe compiler warnings
    monster_race *r_ptr = NULL; // default to soothe compiler warnings

	byte flow_table[2][2][8 * FLOW_MAX_DIST];

    // paranoia
    if (which_flow == FLOW_AUTOMATON)
    {
        msg_debug("Tried to use update_flow() with FLOW_AUTOMATON.");
        return;
    }

    // pull out the relevant monster info for the monster flows
    if (which_flow < MAX_MONSTERS)
    {
        monster_flow = TRUE;
        
        m_ptr = &mon_list[which_flow];
        r_ptr = &r_info[m_ptr->r_idx];
    }
    
    // pull out the relevant monster info for the wandering monster flows
    else if (which_flow <= FLOW_WANDERING_TAIL)
    {
        monster_flow = TRUE;
        
        // search the monsters to find one with that flow
        for (i = 1; i < mon_max; i++)
        {
            m_ptr = &mon_list[i];
            
            // Skip dead monsters
            if (!m_ptr->r_idx) continue;

            r_ptr = &r_info[m_ptr->r_idx];

            // find the first monster with this flow
            if (m_ptr->wandering_idx == which_flow) found = TRUE;
            
            if (found) break;
        }
        
        // stop if this is just a vestigial flow left after the monsters died
        // (these are attempted to be reprocessed on save game load)
        if (!found) return;
    }

	/* Save the new flow epicenter */
	flow_center_y[which_flow] = cy;
	flow_center_x[which_flow] = cx;
	update_center_y[which_flow] = cy;
	update_center_x[which_flow] = cx;

	/* Erase all of the current flow (noise) information */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			cave_cost[which_flow][y][x] = FLOW_MAX_DIST;
		}
	}

	/*** Update or rebuild the flow ***/

	/* Store base cost at the character location */
	cave_cost[which_flow][cy][cx] = 0;

	/* Store this grid in the flow table, note that we've done so */
	flow_table[this_cycle][0][0] = cy;
	flow_table[this_cycle][1][0] = cx;
	grid_count = 1;

	/* Extend the noise burst out to its limits */
	for (cost = 1; cost <= FLOW_MAX_DIST; cost++)
	{
		/* Get the number of grids we'll be looking at */
		last_index = grid_count;

		/* Stop if we've run out of work to do */
		if (last_index == 0) break;

		/* Clear the grid count */
		grid_count = 0;

		/* Get each valid entry in the flow table in turn. */
		for (i = 0; i < last_index; i++)
		{
			/* Get this grid */
			y = flow_table[this_cycle][0][i];
			x = flow_table[this_cycle][1][i];
			
			// Some grids are not ready to process immediately.
			// For example doors, which add 5 cost to noise, 3 cost to movement.
			// They keep getting put back on the queue until ready.
			if (cave_cost[which_flow][y][x] >= cost)
			{
				/* Store this grid in the flow table */
				flow_table[next_cycle][0][grid_count] = y;
				flow_table[next_cycle][1][grid_count] = x;
				
				/* Increment number of grids stored */
				grid_count++;
			}
			
			// if the grid is ready to process...
			else
			{
				/* Look at all adjacent grids */
				for (d = 0; d < 8; d++)
				{
                    int extra_cost = 0;
                    
					/* Child location */
					y2 = y + ddy_ddd[d];
					x2 = x + ddx_ddd[d];
					
					/* Check Bounds */
					if (!in_bounds(y2, x2)) continue;
					
					/* Ignore previously marked grids, unless this is a shorter distance */
					if (cave_cost[which_flow][y2][x2] < FLOW_MAX_DIST) continue;
					               
                    // Deal with monster pathfinding
                    if (monster_flow)
                    {
                        // get the percentage chance of the monster being able to move onto that square
                        int chance = cave_passable_mon(m_ptr, y2, x2, &bash);
                        
                        // if there is any chance, then convert it to a number of turns
                        if (chance > 0)
                        {
                            extra_cost += (100 / chance) - 1;
                            
                            // add an extra turn for unlocking/opening doors as this action doesn't move the monster
                            if (cave_any_closed_door_bold(y2, x2) && !bash)
                            {
                                if (!((r_ptr->flags2 & (RF2_PASS_DOOR)) || (r_ptr->flags2 & (RF2_PASS_WALL))))
                                {
                                    extra_cost += 1;
                                }
                            }
                            
                            // add extra turn(s) for tunneling through rubble/walls as this action doesn't move the monster
                            else if (cave_wall_bold(y2, x2) && (r_ptr->flags2 & (RF2_TUNNEL_WALL)))
                            {
                                if (cave_feat[y2][x2] == FEAT_RUBBLE)   extra_cost += 1; // an extra turn to dig through
                                else                                    extra_cost += 2; // two extra turns to dig through granite/quartz
                            }
                            
                            else if (cave_wall_bold(y2, x2) && (r_ptr->flags2 & (RF2_KILL_WALL)))
                            {
                                extra_cost += 1; // pretend it would take an extra turn (to prefer routes with less wall destruction
                            }
                        }
                        
                        // if there is no chance, just skip this square
                        else
                        {
                            continue;
                        }
                    }
                    
                    // Deal with noise flows
                    else
                    {
                        // ignore walls
                        if (cave_wall_bold(y2,x2) && (cave_feat[y2][x2] != FEAT_SECRET)) continue;
 
                        // penalize doors by 5 when calculating the real noise
                        if (cave_any_closed_door_bold(y2,x2))
                        {                                    
                            extra_cost += 5;
                        }
                    }
										
					/* Monsters at this site need to re-consider their targets */
                    
					if (cave_m_idx[y2][x2] > 0)
					{
						monster_type *n_ptr = &mon_list[cave_m_idx[y2][x2]];
						
                        n_ptr->target_x = 0;
                        n_ptr->target_y = 0;
					}

                    /* Store cost at this location */
					cave_cost[which_flow][y2][x2] = cost + extra_cost;

					/* Store this grid in the flow table */
					flow_table[next_cycle][0][grid_count] = y2;
					flow_table[next_cycle][1][grid_count] = x2;
					
					/* Increment number of grids stored */
					grid_count++;
				}
			}
		}

		/* Swap write and read portions of the table */
		if (this_cycle == 0)
		{
			this_cycle = 1;
			next_cycle = 0;
		}
		else
		{
			this_cycle = 0;
			next_cycle = 1;
		}
	}
}


/*
 * Characters leave scent trails for perceptive monsters to track.  -LM-
 *
 * Smell is rather more limited than sound.  Many creatures cannot use
 * it at all, it doesn't extend very far outwards from the character's
 * current position, and monsters can use it to home in the character,
 * but not to run away from him.
 *
 * Smell is valued according to age.  When a character takes his turn,
 * scent is aged by one, and new scent of the current age is laid down.
 * Speedy characters leave more scent, true, but it also ages faster,
 * which makes it harder to hunt them down.
 *
 * Whenever the age count loops, most of the scent trail is erased and
 * the age of the remainder is recalculated.
 */
void update_smell(void)
{
	int i, j;
	int y, x;
	int py = p_ptr->py;
	int px = p_ptr->px;
	/* Create a table that controls the spread of scent */
	int scent_adjust[5][5] =
	{
		{ 250,  2,  2,  2, 250 },
		{   2,  1,  1,  1,   2 },
		{   2,  1,  0,  1,   2 },
		{   2,  1,  1,  1,   2 },
		{ 250,  2,  2,  2, 250 },
	};
	/* Scent becomes "younger" */
	scent_when--;
	
	/* Loop the age and adjust scent values when necessary */
	if (scent_when <= 0)
	{
		/* Scan the entire dungeon */
		for (y = 0; y < p_ptr->cur_map_hgt; y++)
		{
			for (x = 0; x < p_ptr->cur_map_wid; x++)
			{
				/* Ignore non-existent scent */
				if (cave_when[y][x] == 0) continue;

				/* Erase the earlier part of the previous cycle */
				if (cave_when[y][x] > SMELL_STRENGTH) cave_when[y][x] = 0;

				/* Reset the ages of the most recent scent */
				else cave_when[y][x] = 250 - SMELL_STRENGTH + cave_when[y][x];
			}
		}

		/* Reset the age value */
		scent_when = 250 - SMELL_STRENGTH;
	}
	/* Lay down new scent */
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 5; j++)
		{
			/* Translate table to map grids */
			y = i + py - 2;
			x = j + px - 2;

			/* Check Bounds */
			if (!in_bounds(y, x)) continue;

			/* Walls cannot hold scent. */
			if (cave_info[y][x] & (CAVE_WALL))
			{
				continue;
			}

			/* Grid must not be blocked by walls from the character */
			if (!los(p_ptr->py, p_ptr->px, y, x)) continue;

			/* Note grids that are too far away */
			if (scent_adjust[i][j] == 250) continue;

			/* Mark the grid with new scent */
			cave_when[y][x] = scent_when + scent_adjust[i][j];
		}
	}
}




/*
 * Map the dungeon ala "magic mapping"
 *
 * We must never attempt to map the outer dungeon walls, or we
 * might induce illegal cave grid references.
 */
void map_area(void)
{
	int i, x, y;

	/* Scan that area */
	for (y = 1; y < MAX_DUNGEON_HGT; y++)
	{
		for (x = 1; x < MAX_DUNGEON_WID ; x++)
		{
			if (in_bounds_fully(y, x))
			{
				/* All non-walls are "checked", including rubble */
				if ((cave_feat[y][x] < FEAT_WALL_HEAD) ||
					(cave_stair_bold(y,x)) || (cave_feat[y][x] == FEAT_RUBBLE) || cave_forge_bold(y,x) || (cave_feat[y][x] == FEAT_CHASM)) 
				{
					/* Memorize normal features */
					if ((cave_feat[y][x] >= FEAT_DOOR_HEAD) ||
						(cave_stair_bold(y,x)) || (cave_feat[y][x] == FEAT_RUBBLE) || cave_forge_bold(y,x) || (cave_feat[y][x] == FEAT_CHASM))
					{
						/* Memorize the feature */
						cave_info[y][x] |= (CAVE_MARK);
					}
					
					/* Memorize adjacent walls */
					for (i = 0; i < 8; i++)
					{
						int yy = y + ddy_ddd[i];
						int xx = x + ddx_ddd[i];
						
						/* Memorize walls (etc) */
						if (cave_wall_bold(yy,xx))
						{
							/* Memorize the walls */
							cave_info[yy][xx] |= (CAVE_MARK);
						}
					}
				}				
			}
		}
	}

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD);
}



/*
 * Light up the dungeon using "clairvoyance"
 *
 * This function "illuminates" every grid in the dungeon, memorizes all
 * "objects", memorizes all grids as with magic mapping, and
 * memorizes all floor grids too.
 */
void wiz_light(void)
{
	int i, y, x;


	/* Memorize objects */
	for (i = 1; i < o_max; i++)
	{
		object_type *o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;

		/* Skip held objects */
		if (o_ptr->held_m_idx) continue;

		/* Memorize */
		o_ptr->marked = TRUE;
	}

	/* Scan all normal grids */
	for (y = 1; y < p_ptr->cur_map_hgt-1; y++)
	{
		/* Scan all normal grids */
		for (x = 1; x < p_ptr->cur_map_wid-1; x++)
		{
			/* Process all non-walls, but don't count rubble */
			if ((!cave_wall_bold(y,x)) || (cave_feat[y][x] == FEAT_RUBBLE))
			{
				/* Scan all neighbors */
				for (i = 0; i < 9; i++)
				{
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* Perma-lite the grid */
					cave_info[yy][xx] |= (CAVE_GLOW);

					/* Remember the grid */
					cave_info[yy][xx] |= (CAVE_MARK);
				}
			}
		}
	}

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}


/*
 * Forget the dungeon map (a la "Thinking of Maud...").
 */
void wiz_dark(void)
{
	int i, y, x;

	/* Forget every grid */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			/* Process the grid */
			cave_info[y][x] &= ~(CAVE_MARK);
			
			// forget all traps!
			if (cave_trap_bold(y,x) && !((p_ptr->py == y) && (p_ptr->px == x)))
			{
				cave_info[y][x] |= (CAVE_HIDDEN);
			}
		}
	}

	/* Forget all objects */
	for (i = 1; i < o_max; i++)
	{
		object_type *o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;

		/* Skip held objects */
		if (o_ptr->held_m_idx) continue;

		/* Forget the object */
		o_ptr->marked = FALSE;
	}

	/* Process monsters */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *m_ptr = &mon_list[i];
		monster_race *r_ptr = &r_info[m_ptr->r_idx];
		
        // unmoving mindless monsters need to be forgotten too
        if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && (r_ptr->flags2 & (RF2_MINDLESS)) && m_ptr->encountered)
        {
            // Sil-y: this is a bit of a hack as it means you can get the experience for seeing them again
            // but it will only be at most an extra 50 experience per game, more likely about 10
            m_ptr->encountered = FALSE;
        }
	}

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}



/*
 * Light or Darken the gates
 */
void gates_illuminate(bool daytime)
{
	int y, x;


	/* Apply light or darkness */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			/* Interesting grids */
			if (!cave_floorlike_bold(y,x))
			{
				/* Illuminate the grid */
				cave_info[y][x] |= (CAVE_GLOW);

				/* Memorize the grid */
				cave_info[y][x] |= (CAVE_MARK);
			}

			/* Boring grids (light) */
			else if (daytime)
			{
				/* Illuminate the grid */
				cave_info[y][x] |= (CAVE_GLOW);

				/* Memorize grids */
				cave_info[y][x] |= (CAVE_MARK);
			}

			/* Boring grids (dark) */
			else
			{
				/* Darken the grid */
				cave_info[y][x] &= ~(CAVE_GLOW);

				/* Forget grids */
				cave_info[y][x] &= ~(CAVE_MARK);
			}
		}
	}

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD | PW_MONLIST);
}



/*
 * Change the "feat" flag for a grid, and notice/redraw the grid
 */
void cave_set_feat(int y, int x, int feat)
{

	/* Change the feature */
	cave_feat[y][x] = feat;

	/* Handle "wall/door" grids */
	if ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_WALL_TAIL))
	{
		cave_info[y][x] |= (CAVE_WALL);
	}

	/* Handle "floor"/etc grids */
	else
	{
		cave_info[y][x] &= ~(CAVE_WALL);
	}

	/* Notice/Redraw */
	if (character_dungeon)
	{
		/* Notice */
		note_spot(y, x);

		/* Redraw */
		lite_spot(y, x);
	}
}


/*
 * Determine the path taken by a projection.  -BEN-, -LM-
 *
 * The projection will always start one grid from the grid (y1,x1), and will
 * travel towards the grid (y2,x2), touching one grid per unit of distance
 * along the major axis, and stopping when it satisfies certain conditions
 * or has travelled the maximum legal distance of "range".  Projections
 * cannot extend further than MAX_SIGHT (at least at present).
 *
 * A projection only considers those grids which contain the line(s) of fire
 * from the start to the end point.  Along any step of the projection path,
 * either one or two grids may be valid options for the next step.  When a
 * projection has a choice of grids, it chooses that which offers the least
 * resistance.  Given a choice of clear grids, projections prefer to move
 * orthogonally.
 *
 * Also, projections to or from the character must stay within the pre-
 * calculated field of fire ("cave_info & (CAVE_FIRE)").  This is a hack.
 * XXX XXX
 *
 * The path grids are saved into the grid array pointed to by "gp", and
 * there should be room for at least "range" grids in "gp".  Note that
 * due to the way in which distance is calculated, this function normally
 * uses fewer than "range" grids for the projection path, so the result
 * of this function should never be compared directly to "range".  Note
 * that the initial grid (y1,x1) is never saved into the grid array, not
 * even if the initial grid is also the final grid.  XXX XXX XXX
 *
 * We modify y2 and x2 if they are too far away, or (for PROJECT_PASS only)
 * if the projection threatens to leave the dungeon.
 *
 * The "flg" flags can be used to modify the behavior of this function:
 *    PROJECT_STOP:  projection stops when it cannot bypass a monster.
 *    PROJECT_CHCK:  projection notes when it cannot bypass a monster.
 *    PROJECT_THRU:  projection extends past destination grid
 *    PROJECT_PASS:  projection passes through walls
 *    PROJECT_INVISIPASS:  projection passes through invisible walls (ie unknown ones)
 *
 * This function returns the number of grids (if any) in the path.  This
 * may be zero if no grids are legal except for the starting one.
 */
int project_path(u16b *gp, int range, int y1, int x1, int *y2, int *x2, u32b flg)
{
	int i, j, k;
	int dy, dx;
	int num, dist, octant;
	int grids = 0;
	bool line_fire;
	bool full_stop = FALSE;

	int y_a, x_a, y_b, x_b;
	int y = 0, old_y = 0;
	int x = 0, old_x = 0;

	/* Start with all lines of sight unobstructed */
	u32b bits0 = VINFO_BITS_0;
	u32b bits1 = VINFO_BITS_1;
	u32b bits2 = VINFO_BITS_2;
	u32b bits3 = VINFO_BITS_3;

	int slope_fire1 = -1, slope_fire2 = 0;

	/* Projections are either vertical or horizontal */
	bool vertical;

	/* Require projections to be strictly LOF when possible  XXX XXX */
	bool require_strict_lof = FALSE;

	/* Count of grids in LOF, storage of LOF grids */
	u16b tmp_grids[80];

	/* Count of grids in projection path */
	int step;

	/* Remember whether and how a grid is blocked */
	int blockage[2];

	/* Assume no monsters in way */
	bool monster_in_way = FALSE;

	/* Initial grid */
	s16b g0 = GRID(y1, x1);

	s16b g;

	/* Pointer to vinfo data */
	vinfo_type *p;

	/* Handle projections of zero length */
	if ((range <= 0) || ((*y2 == y1) && (*x2 == x1))) return (0);

	/* Note that the character is the source or target of the projection */
	if ((( y1 == p_ptr->py) && ( x1 == p_ptr->px)) ||
	    ((*y2 == p_ptr->py) && (*x2 == p_ptr->px)))
	{
		/* Require strict LOF */
		require_strict_lof = TRUE;
	}

	/* Get position change (signed) */
	dy = *y2 - y1;
	dx = *x2 - x1;

	/* Get distance from start to finish */
	dist = distance(y1, x1, *y2, *x2);

	/* Must stay within the field of sight XXX XXX */
	if (dist > MAX_SIGHT)
	{
		/* Always watch your (+/-) when doing rounded integer math. */
		int round_y = (dy < 0 ? -(dist / 2) : (dist / 2));
		int round_x = (dx < 0 ? -(dist / 2) : (dist / 2));

		/* Rescale the endpoints */
		dy = ((dy * (MAX_SIGHT - 1)) + round_y) / dist;
		dx = ((dx * (MAX_SIGHT - 1)) + round_x) / dist;
		*y2 = y1 + dy;
		*x2 = x1 + dx;
	}

	/* Get the correct octant */
	if (dy < 0)
	{
		/* Up and to the left */
		if (dx < 0)
		{
			/* More upwards than to the left - octant 4 */
			if (ABS(dy) > ABS(dx)) octant = 5;

			/* At least as much left as upwards - octant 3 */
			else                   octant = 4;
		}
		else
		{
			if (ABS(dy) > ABS(dx)) octant = 6;
			else                   octant = 7;
		}
	}
	else
	{
		if (dx < 0)
		{
			if (ABS(dy) > ABS(dx)) octant = 2;
			else                   octant = 3;
		}
		else
		{
			if (ABS(dy) > ABS(dx)) octant = 1;
			else                   octant = 0;
		}
	}

	/* Determine whether the major axis is vertical or horizontal */
	if ((octant == 5) || (octant == 6) || (octant == 2) || (octant == 1))
	{
		vertical = TRUE;
	}
	else
	{
		vertical = FALSE;
	}


	/* Scan the octant, find the grid corresponding to the end point */
	for (j = 1; j < VINFO_MAX_GRIDS; j++)
	{
		s16b vy, vx;

		/* Point to this vinfo record */
		p = &vinfo[j];

		/* Extract grid value */
		g = g0 + p->grid[octant];

		/* Get axis coordinates */
		vy = GRID_Y(g);
		vx = GRID_X(g);

		/* Allow for negative values XXX XXX XXX */
		if (vy > 256 * 127) vy = vy - (256 * 256);
		if (vx > x1 + 127)
		{
			vy++;
			vx = vx - 256;
		}

		/* Require that grid be correct */
		if ((vy != *y2) || (vx != *x2)) continue;

		/* Store lines of fire */
		slope_fire1 = p->slope_fire_index1;
		slope_fire2 = p->slope_fire_index2;

		break;
	}

	/* Note failure XXX XXX */
	if (slope_fire1 == -1) return (0);

	/* Scan the octant, collect all grids having the correct line of fire */
	for (j = 1; j < VINFO_MAX_GRIDS; j++)
	{
		line_fire = FALSE;

		/* Point to this vinfo record */
		p = &vinfo[j];

		/* See if any lines of sight pass through this grid */
		if (!((bits0 & (p->bits_0)) ||
			   (bits1 & (p->bits_1)) ||
			   (bits2 & (p->bits_2)) ||
			   (bits3 & (p->bits_3))))
		{
			continue;
		}

		/*
		 * Extract grid value.  Use pointer shifting to get the
		 * correct grid offset for this octant.
		 */
		g = g0 + *((s16b*)(((byte*)(p)) + (octant * 2)));

		y = GRID_Y(g);
		x = GRID_X(g);

		/* Must be legal (this is important) */
		if (!in_bounds_fully(y, x)) continue;

		/* Check for first possible line of fire */
		i = slope_fire1;

		/* Check line(s) of fire */
		while (TRUE)
		{
			switch (i / 32)
			{
				case 3:
				{
					if (bits3 & (1L << (i % 32)))
					{
						if (p->bits_3 & (1L << (i % 32))) line_fire = TRUE;
					}
					break;
				}
				case 2:
				{
					if (bits2 & (1L << (i % 32)))
					{
						if (p->bits_2 & (1L << (i % 32))) line_fire = TRUE;
					}
					break;
				}
				case 1:
				{
					if (bits1 & (1L << (i % 32)))
					{
						if (p->bits_1 & (1L << (i % 32))) line_fire = TRUE;
					}
					break;
				}
				case 0:
				{
					if (bits0 & (1L << (i % 32)))
					{
						if (p->bits_0 & (1L << (i % 32))) line_fire = TRUE;
					}
					break;
				}
			}

			/* We're done if no second LOF exists, or when we've checked it */
			if ((!slope_fire2) || (i == slope_fire2)) break;

			/* Check second possible line of fire */
			i = slope_fire2;
		}

		/* This grid contains at least one of the lines of fire */
		if (line_fire)
		{
			/* Do not accept breaks in the series of grids  XXX XXX */
			if ((grids) && ((ABS(y - old_y) > 1) || (ABS(x - old_x) > 1)))
			{
				break;
			}

			/* Optionally, require strict line of fire */
			if ((!require_strict_lof) || (cave_info[y][x] & (CAVE_FIRE)) ||
			    ((flg & (PROJECT_INVISIPASS)) && !(cave_info[y][x] & (CAVE_MARK))) )
			{
				/* Store grid value */
				tmp_grids[grids++] = g;
			}

			/* Remember previous coordinates */
			old_y = y;
			old_x = x;
		}

		/*
		 * Handle wall (unless ignored).  Walls can be in a projection path,
		 * but the path cannot pass through them.
		 */
		if (!(flg & (PROJECT_PASS)) && (cave_info[y][x] & (CAVE_WALL)))
		{
			if (!(flg & (PROJECT_INVISIPASS)) || (cave_info[y][x] & (CAVE_MARK)))
			{				
				/* Clear any lines of sight passing through this grid */
				bits0 &= ~(p->bits_0);
				bits1 &= ~(p->bits_1);
				bits2 &= ~(p->bits_2);
				bits3 &= ~(p->bits_3);
			}
		}
        
        /*
         * Handle chasms if they are designated to block the line
         */
		if ((flg & (PROJECT_NO_CHASM)) && (cave_feat[y][x] & (FEAT_CHASM)))
		{
            /* Clear any lines of sight passing through this grid */
            bits0 &= ~(p->bits_0);
            bits1 &= ~(p->bits_1);
            bits2 &= ~(p->bits_2);
            bits3 &= ~(p->bits_3);
		}
        
	}

	/* Scan the grids along the line(s) of fire */
	for (step = 0, j = 0; j < grids;)
	{
		/* Get the coordinates of this grid */
		y_a = GRID_Y(tmp_grids[j]);
		x_a = GRID_X(tmp_grids[j]);

		/* Get the coordinates of the next grid, if legal */
		if (j < grids - 1)
		{
			y_b = GRID_Y(tmp_grids[j+1]);
			x_b = GRID_X(tmp_grids[j+1]);
		}
		else
		{
			y_b = -1;
			x_b = -1;
		}

		/*
		 * We always have at least one legal grid, and may have two.  Allow
		 * the second grid if its position differs only along the minor axis.
		 */
		if (vertical ? y_a == y_b : x_a == x_b) num = 2;
		else                                    num = 1;


		/* Scan one or both grids */
		for (i = 0; i < num; i++)
		{
			blockage[i] = 0;

			/* Get the coordinates of this grid */
			y = (i == 0 ? y_a : y_b);
			x = (i == 0 ? x_a : x_b);

			/* Determine perpendicular distance */
			k = (vertical ? ABS(x - x1) : ABS(y - y1));

			/* Hack -- Check maximum range */
			if ((i == num-1) && (step + (k >> 1)) >= range-1)
			{
				/* End of projection */
				full_stop = TRUE;
			}

			/* Sometimes stop at destination grid */
			if (!(flg & (PROJECT_THRU)))
			{
				if ((y == *y2) && (x == *x2))
				{
					/* End of projection */
					full_stop = TRUE;
				}
			}

			/* Usually stop at wall grids */
			if (!(flg & (PROJECT_PASS)) && (!(flg & (PROJECT_INVISIPASS)) || (cave_info[y][x] & (CAVE_MARK))))
			{
				if (!cave_floor_bold(y, x)) blockage[i] = 2;
			}

			/* If we don't stop at wall grids, we must explicitly check legality */
			else if (!in_bounds_fully(y, x))
			{
				/* End of projection */
				full_stop = TRUE;
				blockage[i] = 3;
			}

			/* Try to avoid monsters/players between the endpoints */
			if ((cave_m_idx[y][x] != 0) && (blockage[i] < 2))
			{
				// Hack: ignore monsters on the designated square if these flags are set
				if (!(project_path_ignore && (y == project_path_ignore_y) && (x == project_path_ignore_x)))
				{
					if      (flg & (PROJECT_STOP)) blockage[i] = 2;
					else if (flg & (PROJECT_CHCK)) blockage[i] = 1;
				}
			}
		}

		/* Pick the first grid if possible, the second if necessary */
		if ((num == 1) || (blockage[0] <= blockage[1]))
		{
			/* Store the first grid, advance */
			if (blockage[0] < 3) gp[step++] = tmp_grids[j];

			/* Blockage of 2 or greater means the projection ends */
			if (blockage[0] >= 2) break;

			/* Blockage of 1 means a monster bars the path */
			if (blockage[0] == 1)
			{
				/* Endpoints are always acceptable */
				if ((y != *y2) || (x != *x2)) monster_in_way = TRUE;
			}

			/* Handle end of projection */
			if (full_stop) break;
		}
		else
		{
			/* Store the second grid, advance */
			if (blockage[1] < 3) gp[step++] = tmp_grids[j + 1];

			/* Blockage of 2 or greater means the projection ends */
			if (blockage[1] >= 2) break;

			/* Blockage of 1 means a monster bars the path */
			if (blockage[1] == 1)
			{
				/* Endpoints are always acceptable */
				if ((y != *y2) || (x != *x2)) monster_in_way = TRUE;
			}

			/* Handle end of projection */
			if (full_stop) break;
		}

		/*
		 * Hack -- If we require orthogonal movement, but are moving
		 * diagonally, we have to plot an extra grid.  XXX XXX
		 */
		if ((flg & (PROJECT_ORTH)) && (step > 1))
		{
			/* Get grids for this projection step and the last */
			y_a = GRID_Y(gp[step-1]);
			x_a = GRID_X(gp[step-1]);

			y_b = GRID_Y(gp[step-2]);
			x_b = GRID_X(gp[step-2]);

			/* The grids differ along both axis -- we moved diagonally */
			if ((y_a != y_b) && (x_a != x_b))
			{
				/* Get locations for the connecting grids */
				int y_c = y_a;
				int x_c = x_b;
				int y_d = y_b;
				int x_d = x_a;

				/* Back up one step */
				step--;

				/* Assume both grids are available */
				blockage[0] = 0;
				blockage[1] = 0;

				/* Hack -- Check legality */
				if (!in_bounds_fully(y_c, x_c)) blockage[0] = 2;
				if (!in_bounds_fully(y_d, x_d)) blockage[1] = 2;

				/* Usually stop at wall grids */
				if (!(flg & (PROJECT_PASS)) && (!(flg & (PROJECT_INVISIPASS)) || (cave_info[y][x] & (CAVE_MARK))))
				{
					if (!cave_floor_bold(y_c, x_c)) blockage[0] = 2;
					if (!cave_floor_bold(y_d, x_d)) blockage[1] = 2;
				}

				/* Try to avoid non-initial monsters/players */
				if (cave_m_idx[y_c][x_c] != 0)
				{
					// Hack: ignore monsters on the designated square if these flags are set
					if (!(project_path_ignore && (y_c == project_path_ignore_y) && (x_c == project_path_ignore_x)))
					{
						if      (flg & (PROJECT_STOP)) blockage[0] = 2;
						else if (flg & (PROJECT_CHCK)) blockage[0] = 1;
					}
				}
				if (cave_m_idx[y_d][x_d] != 0)
				{
					// Hack: ignore monsters on the designated square if these flags are set
					if (!(project_path_ignore && (y_c == project_path_ignore_y) && (x_c == project_path_ignore_x)))
					{
						if      (flg & (PROJECT_STOP)) blockage[1] = 2;
						else if (flg & (PROJECT_CHCK)) blockage[1] = 1;
					}
				}

				/* Both grids are blocked -- we have to stop now */
				if ((blockage[0] >= 2) && (blockage[1] >= 2)) break;

				/* Accept the first grid if possible, the second if necessary */
				if (blockage[0] <= blockage[1]) gp[step++] = GRID(y_c, x_c);
				else                            gp[step++] = GRID(y_d, x_d);

				/* Re-insert the original grid, take an extra step */
				gp[step++] = GRID(y_a, x_a);

				/* Increase range to accommodate this extra step */
				range++;
			}
		}

		/* Advance to the next unexamined LOF grid */
		j += num;
	}

	/* Accept last grid as the new endpoint */
	*y2 = GRID_Y(gp[step -1]);
	*x2 = GRID_X(gp[step -1]);

	/* Return count of grids in projection path */
	if (monster_in_way) return (-step);
	else return (step);
}



/*
 * Determine if a bolt spell cast from (y1,x1) to (y2,x2) will arrive
 * at the final destination, using the "project_path()" function to check
 * the projection path.
 *
 * Accept projection flags, and pass them onto "project_path()".
 *
 * Note that no grid is ever "projectable()" from itself.
 * This function is used to determine if the player can (easily) target
 * a given grid, if a monster can target the player, and if a clear shot
 * exists from monster to player.
 */

byte projectable(int y1, int x1, int y2, int x2, u32b flg)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int y, x;

	int grid_n = 0;
	u16b grid_g[512];

	int old_y2 = y2;
	int old_x2 = x2;

	/* We do not have permission to pass through walls */
	if (!(flg & (PROJECT_WALL | PROJECT_PASS)))
	{
		/* The character is the source of the projection */
		if ((y1 == py) && (x1 == px))
		{
			/* Require that destination be in line of fire */
			if (!(cave_info[y2][x2] & (CAVE_FIRE))) return (PROJECT_NO);
		}

		/* The character is the target of the projection */
		else if ((y2 == py) && (x2 == px))
		{
			/* Require that source be in line of fire */
			if (!(cave_info[y1][x1] & (CAVE_FIRE))) return (PROJECT_NO);
		}
	}

	/* Check the projection path */
	grid_n = project_path(grid_g, MAX_RANGE, y1, x1, &y2, &x2, flg);

	/* No grid is ever projectable from itself */
	if (!grid_n) return (PROJECT_NO);

	/* Final grid.  As grid_n may be negative, use absolute value.  */
	y = GRID_Y(grid_g[ABS(grid_n) - 1]);
	x = GRID_X(grid_g[ABS(grid_n) - 1]);

	/* May not end in an unrequested grid */
	if ((y != old_y2) || (x != old_x2)) return (PROJECT_NO);

	/* May not end in a wall */
	if (!cave_floor_bold(y, x)) return (PROJECT_NO);

	/* Promise a clear bolt shot if we have verified that there is one */
	if ((flg & (PROJECT_STOP)) || (flg & (PROJECT_CHCK)))
	{
		/* Positive value for grid_n mean no obstacle was found. */
		if (grid_n > 0) return (PROJECT_CLEAR);
	}

	/* Assume projectable, but make no promises about clear shots */
	return (PROJECT_NOT_CLEAR);

}



/*
 * Standard "find me a location" function
 *
 * Obtains a legal location within the given distance of the initial
 * location, and with "los()" from the source to destination location.
 *
 * This function is often called from inside a loop which searches for
 * locations while increasing the "d" distance.
 *
 * Currently the "m" parameter is unused.
 */
void scatter(int *yp, int *xp, int y, int x, int d, int m)
{
	int nx, ny;


	/* Unused parameter */
	(void)m;

	/* Pick a location */
	while (TRUE)
	{
		/* Pick a new location */
		ny = rand_spread(y, d);
		nx = rand_spread(x, d);

		/* Ignore annoying locations */
		if (!in_bounds_fully(ny, nx)) continue;

		/* Ignore "excessively distant" locations */
		if ((d > 1) && (distance(y, x, ny, nx) > d)) continue;

		/* Require "line of sight" */
		if (los(y, x, ny, nx)) break;
	}

	/* Save the location */
	(*yp) = ny;
	(*xp) = nx;
}






/*
 * Track a new monster
 */
void health_track(int m_idx)
{
	/* Track a new guy */
	p_ptr->health_who = m_idx;

	/* Redraw (later) */
	p_ptr->redraw |= (PR_HEALTHBAR);
}



/*
 * Hack -- track the given monster race
 */
void monster_race_track(int r_idx)
{
	// don't track when hallucinating
	if (p_ptr->image) return;

    // don't track when raging
	if (p_ptr->rage) return;

	/* Save this monster ID */
	p_ptr->monster_race_idx = r_idx;

	/* Window stuff */
	p_ptr->window |= (PW_MONSTER);
}



/*
 * Hack -- track the given object kind
 */
void object_kind_track(int k_idx)
{
	/* Save this object ID */
	p_ptr->object_kind_idx = k_idx;

	/* Window stuff */
	p_ptr->window |= (PW_OBJECT);
}



/*
 * Something has happened to disturb the player.
 *
 * The first arg indicates a major disturbance, which affects stealth mode.
 *
 * The second arg is currently unused, but could induce output flush.
 *
 * All disturbance cancels repeated commands, resting, and running.
 */
void disturb(int stop_stealth, int unused_flag)
{	
	/* Unused parameter */
	(void)unused_flag;

	/* Cancel auto-commands */
	/* p_ptr->command_new = 0; */

	/* Cancel repeated commands */
	if (p_ptr->command_rep)
	{
		/* Cancel */
		p_ptr->command_rep = 0;

		/* Redraw the state (later) */
		p_ptr->redraw |= (PR_STATE);
	}

	/* Cancel Resting */
	if (p_ptr->resting)
	{
		/* Cancel */
		p_ptr->resting = 0;

		/* Redraw the state (later) */
		p_ptr->redraw |= (PR_STATE);
	}

	/* Cancel Smithing */
	if (p_ptr->smithing)
	{
		/* Cancel */
		p_ptr->smithing = 0;

		// Display a message
		msg_print("Your work is interrupted!");
		
		/* Redraw the state (later) */
		p_ptr->redraw |= (PR_STATE);
	}
	
	/* Cancel running */
	if (p_ptr->running)
	{
		/* Cancel */
		p_ptr->running = 0;

 		/* Check for new panel if appropriate */
 		if (center_player && run_avoid_center) verify_panel();
	}

	/* Cancel stealth if requested */
	if (stop_stealth && p_ptr->stealth_mode)
	{
		// signal that it will be stopped at the end of the turn
		stop_stealth_mode = TRUE;
	}

	/* Flush the input */
	flush();
}


