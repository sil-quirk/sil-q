/* File: generate.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "vault.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 *
 * Sil notes:
 * 
 * I do not make any use of "solid" walls, but have left the type in.
 * The code previously used a lot of 11x11 blocks in room generation.
 * I have mostly removed references to this now.
 * The rooms are now placed at random in the dungeon.
 * The corridor generation has been simplified a lot for aesthetic purposes.
 * Note that level generation can fail (if the level is unconnected, or for other
 * reasons) and that each room and corridor generation can fail too.
 * This is not a problem as they are generated until success and often succeed.
 */

/*
 * Dungeon generation values
 */

#define DUN_DEST	1	/* 1/chance of having a destroyed level */

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN	5	/* Density of streamers */
#define DUN_STR_RNG	2	/* Width of streamers */
#define DUN_STR_QUA	4	/* Number of quartz streamers */

/*
 * Dungeon treausre allocation values
 */
#define DUN_OBJ_CHANCE_ROOM	30	/* determines number of items found in rooms */
#define DUN_OBJ_CHANCE_BOTH	5	/* determines number of items found in rooms/corridors */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR		1	/* Hallway */
#define ALLOC_SET_ROOM		2	/* Room */
#define ALLOC_SET_BOTH		3	/* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE	1	/* Rubble */
#define ALLOC_TYP_TRAP		3	/* Trap */
#define ALLOC_TYP_OBJECT	5	/* Object */


/*
 * Maximum numbers of rooms along each axis (currently 6x18)
 */

#define MAX_ROOMS_ROW	(MAX_DUNGEON_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL	(MAX_DUNGEON_WID / BLOCK_WID)

/*
 * Bounds on some arrays used in the "dun_data" structure.
 * These bounds are checked, though usually this is a formality.
 */
#define CENT_MAX	110
#define DOOR_MAX	200
#define WALL_MAX	500
#define TUNN_MAX	900

bool allow_uniques;


/*
 * Maximal number of room types
 */
#define ROOM_MAX	12
#define ROOM_MIN     2

/*
 * Simple structure to hold a map location
 */

typedef struct coord coord;

struct coord
{
	byte y;
	byte x;
};

/*
 * Simple structure to hold a map location
 */

typedef struct rectangle rectangle;

struct rectangle
{
	byte y1;
	byte x1;
	byte y2;
	byte x2;
};


/*
 * Structure to hold all "dungeon generation" data
 */

typedef struct dun_data dun_data;

struct dun_data
{
	/* Array of centers of rooms */
	int cent_n;
	coord cent[CENT_MAX];

	/* Sil: Array of room corners */
	rectangle corner[CENT_MAX];

	/* Sil: Array of what dungeon piece each room is in */
	byte piece[CENT_MAX];

	/* Array of connections between rooms */
	bool connection[DUN_ROOMS][DUN_ROOMS];
};


/*
 * Dungeon generation data -- see "cave_gen()"
 */
static dun_data *dun;

/*
 * Array[DUNGEON_HGT][DUNGEON_WID]. 
 * Each corridor square it is marked for each room that it connects.
 */
int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];


/* determines whether the player can pass through a given feature */
/* icky locations (inside vaults) are all considered passable.    */
bool player_passable(int y, int x, bool ignore_rubble)
{
	byte feature = cave_feat[y][x];
	bool icky_interior = (cave_info[y][x] & (CAVE_ICKY)) &&
						 (cave_info[y][x-1] & (CAVE_ICKY)) &&
						 (cave_info[y][x+1] & (CAVE_ICKY)) &&
						 (cave_info[y-1][x] & (CAVE_ICKY)) &&
						 (cave_info[y+1][x] & (CAVE_ICKY));

	bool passable = ((feature < FEAT_WALL_HEAD) || (feature > FEAT_WALL_TAIL) || (feature == FEAT_SECRET) || 
	                 ((feature == FEAT_RUBBLE) && ignore_rubble) || icky_interior);
					 
	return (passable);
}


/* floodfills access through the dungeon, marking all accessible squares with TRUE */
void flood_access(int y, int x, int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID], bool ignore_rubble)
{
	/* first check the map bounds */
	if ((y < 0) || (y > p_ptr->cur_map_hgt) || (x < 0) || (x > p_ptr->cur_map_wid))
		return;
	
	access_array[y][x] = TRUE;
	if (player_passable(y-1, x-1, ignore_rubble) && (access_array[y-1][x-1] == FALSE))
		flood_access(y-1, x-1, access_array, ignore_rubble);
	if (player_passable(y-1, x, ignore_rubble) && (access_array[y-1][x] == FALSE))
		flood_access(y-1, x, access_array, ignore_rubble);
	if (player_passable(y-1, x+1, ignore_rubble) && (access_array[y-1][x+1] == FALSE))
		flood_access(y-1, x+1, access_array, ignore_rubble);
	if (player_passable(y, x-1, ignore_rubble) && (access_array[y][x-1] == FALSE))
		flood_access(y, x-1, access_array, ignore_rubble);
	if (player_passable(y, x+1, ignore_rubble) && (access_array[y][x+1] == FALSE))
		flood_access(y, x+1, access_array, ignore_rubble);
	if (player_passable(y+1, x-1, ignore_rubble) && (access_array[y+1][x-1] == FALSE))
		flood_access(y+1, x-1, access_array, ignore_rubble);
	if (player_passable(y+1, x, ignore_rubble) && (access_array[y+1][x] == FALSE))
		flood_access(y+1, x, access_array, ignore_rubble);
	if (player_passable(y+1, x+1, ignore_rubble) && (access_array[y+1][x+1] == FALSE))
		flood_access(y+1, x+1, access_array, ignore_rubble);
	return;
}


void label_rooms(void)
{
	int i;

	for (i = 0; i < dun->cent_n; i++)
	{
		//cave_feat[dun->corner[i].y1][dun->corner[i].x1] = 5 + 1;
		//cave_feat[dun->corner[i].y1][dun->corner[i].x2] = 5 + 1;
		//cave_feat[dun->corner[i].y2][dun->corner[i].x1] = 5 + 1;
		//cave_feat[dun->corner[i].y2][dun->corner[i].x2] = 5 + 1;

		cave_feat[dun->cent[i].y][dun->cent[i].x] = 5 + (i%10);
		if (i > 9)
		{
			cave_feat[dun->cent[i].y][dun->cent[i].x - 1] = 5 + ((i/10)%10);
		}
		if (i > 99)
		{
			cave_feat[dun->cent[i].y][dun->cent[i].x - 1] = 5 + ((i/100)%10);
		}
	}	
}

// floodfills access through the *graph* of the dungeon
void flood_piece(int n, int piece_num)
{
	int i;
	
	dun->piece[n] = piece_num;
			
	for (i = 0; i < dun->cent_n; i++)
	{
		if (dun->connection[n][i] && (dun->piece[i] == 0))
		{
			flood_piece(i, piece_num);
		}
	}
	return;
}

int dungeon_pieces(void)
{
	int piece_num;
	int i;
	
	// first reset the pieces
	for (i = 0; i < dun->cent_n; i++)
	{
		dun->piece[i] = 0;
	}
	
	for (piece_num = 1; piece_num <= dun->cent_n; piece_num++)
	{
		// find the next room that doesn't belong to a piece
		for (i = 0; (i < dun->cent_n) && (dun->piece[i] != 0); i++);
		
		if (i == dun->cent_n)
		{
			break;
		}
		else
		{
			flood_piece(i, piece_num);
		}
	}
	
	return (piece_num - 1);
}


/*
 * Convert existing terrain type to rubble
 */
static void place_rubble(int y, int x)
{
	/* Create rubble */
	if (p_ptr->depth >= 4) cave_set_feat(y, x, FEAT_RUBBLE);
}


/*
 * Choose either an ordinary up staircase or an up shaft.
 */
static int choose_up_stairs(void)
{
	if (p_ptr->depth >= 2)
	{
		if (one_in_(2)) return (FEAT_LESS_SHAFT);
	}

	return (FEAT_LESS);
}


/*
 * Choose either an ordinary down staircase or an down shaft.
 */
static int choose_down_stairs(void)
{
	if (p_ptr->depth < MORGOTH_DEPTH - 2)
	{
		if (one_in_(2)) return (FEAT_MORE_SHAFT);
	}

	return (FEAT_MORE);
}


/*
 * Place an up/down staircase at given location
 */
void place_random_stairs(int y, int x)
{
	/* Paranoia */
	if (!cave_clean_bold(y, x)) return;

	/* Create a staircase */
	if (!p_ptr->depth)
	{
		cave_set_feat(y, x, FEAT_MORE);
	}
	else if (p_ptr->depth >= MORGOTH_DEPTH)
	{
		if (one_in_(2))	cave_set_feat(y, x, FEAT_LESS);
		else cave_set_feat(y, x, FEAT_LESS_SHAFT);
	}
	else if (one_in_(2))
	{
		if (p_ptr->depth <= 1)
			cave_set_feat(y, x, FEAT_MORE);
		else if (one_in_(2)) cave_set_feat(y, x, FEAT_MORE);
		else cave_set_feat(y, x, FEAT_MORE_SHAFT);
	}
	else
	{
		if ((one_in_(2)) || (p_ptr->depth == 1)) cave_set_feat(y, x, FEAT_LESS);
		else cave_set_feat(y, x, FEAT_LESS_SHAFT);
	}
}

/*
 * Generate the chosen item at a random spot in the dungeon.
 * If 'close' is true, it must be nearby and in line-of-sight of the player.
 */
void place_item_randomly(int tval, int sval, bool close)
{
	object_type *i_ptr;
	object_type object_type_body;
	int y, x;
	int i;
	s16b k_idx;
	
	if (close)
	{
		for (i = 0; i < 1000; i++)
		{
			y = p_ptr->py + rand_range(-5,+5);
			x = p_ptr->px + rand_range(-5,+5);
			
			if (cave_naked_bold(y, x) && los(p_ptr->py, p_ptr->px, y, x) && (cave_info[y][x] & (CAVE_ROOM)))
			{
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < 1000; i++)
		{
			y = rand_int(p_ptr->cur_map_hgt);
			x = rand_int(p_ptr->cur_map_wid);
			
			if (cave_naked_bold(y, x))
			{
				break;
			}
		}
	}
	
	/* Get local object */
	i_ptr = &object_type_body;
	
	/* Get the object_kind */
	k_idx = lookup_kind(tval, sval);
	
	/* Valid item? */
	if (!k_idx) return;

	/* Paranoia regarding having found a spot */
	if (i == 1000) return;
	
	/* Prepare the item */
	object_prep(i_ptr, k_idx);
	
	if (tval == TV_ARROW)
	{
		i_ptr->number = (byte)24;
	}
	else
	{
		i_ptr->number = (byte)1;
	}
	
	drop_near(i_ptr, 0, y, x);	
}


/*
 * Allocates some objects (using "place" and "type")
 */
static void alloc_object(int set, int typ, int num, bool out_of_sight)
{
	int y, x, k, i;

	/* Place some objects */
	for (k = 0; k < num; k++)
	{
		/* Pick a "legal" spot */
		for (i = 0; i < 10000; i++)
		{
			bool is_room;

			/* Location */
			y = rand_int(p_ptr->cur_map_hgt);
			x = rand_int(p_ptr->cur_map_wid);

			/* Require "naked" floor grid */
			if (!cave_naked_bold(y, x)) continue;

			/* Check for "room" */
			is_room = (cave_info[y][x] & (CAVE_ROOM)) ? TRUE : FALSE;

			/* Require corridor? */
			if ((set == ALLOC_SET_CORR) && is_room) continue;

			/* Require room? */
			if ((set == ALLOC_SET_ROOM) && !is_room) continue;

			/* Require out_of_sight -- actually more than MAX_SIGHT squares away */
			if (out_of_sight && (distance(p_ptr->py, p_ptr->px, y, x) <= MAX_SIGHT)) continue;

			/* Accept it */
			break;
		}

		/* No point found */
		if (i == 10000) return;

		/* Place something */
		switch (typ)
		{
			case ALLOC_TYP_RUBBLE:
			{
				place_rubble(y, x);
				break;
			}

			case ALLOC_TYP_TRAP:
			{
				place_trap(y, x);
				break;
			}

			case ALLOC_TYP_OBJECT:
			{
				place_object(y, x, FALSE, FALSE, DROP_TYPE_UNTHEMED);
				break;
			}
		}
	}
}



/*
 * Places "streamers" of quartz through dungeon
 */
static bool build_streamer(int feat)
{
	int i, tx, ty;
	int y, x, dir;
	int tries1 = 0;
    int tries2 = 0;

	/* Hack -- Choose starting point */
	y = rand_spread(p_ptr->cur_map_hgt / 2, 10);
	x = rand_spread(p_ptr->cur_map_wid / 2, 15);

	/* Choose a random compass direction */
	dir = ddd[rand_int(8)];

	/* Place streamer into dungeon */
	while (TRUE)
	{
		tries1++;

		if (tries1 > 2500) return(FALSE);

		/* One grid per density */
		for (i = 0; i < DUN_STR_DEN; i++)
		{
			int d = DUN_STR_RNG;

			/* Pick a nearby grid */
			while (TRUE)
			{
				tries2 ++;
				if (tries2 > 2500) return (FALSE);
				ty = rand_spread(y, d);
				tx = rand_spread(x, d);
				if (!in_bounds(ty, tx)) continue;
				break;
			}

            /* Only convert "granite" walls */
            if (cave_feat[ty][tx] < FEAT_WALL_EXTRA) continue;
            if (cave_feat[ty][tx] > FEAT_WALL_SOLID) continue;

			/* Clear previous contents, add proper vein type */
			cave_set_feat(ty, tx, feat);
		}

		/* Advance the streamer */
		y += ddy[dir];
		x += ddx[dir];

		/* Stop at dungeon edge */
		if (!in_bounds(y, x)) break;
	}

	return (TRUE);
}

/*
 * Places a single chasm
 */
static bool build_chasm(void)
{
	int i;
	int y, x;
    int main_dir, new_dir;
    int length;
    int floor_to_chasm;
    
    bool chasm_ok = FALSE;
    
    while (!chasm_ok)
    {
        // choose starting point
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);
        
        // choose a random cardinal direction for it to run in
        main_dir = ddd[rand_int(4)];
        
        // choose a random length for it
        length = damroll(4,8);
        
        // determine its shape
        for (i = 0; i < length; i++)
        {
            // go in a random direction half the time
            if (one_in_(2))
            {
                // choose the random cardinal direction
                new_dir = ddd[rand_int(4)];
                y += ddy[new_dir];
                x += ddx[new_dir];
            }
            
            // go straight ahead the other half
            else
            {
                y += ddy[main_dir];
                x += ddx[main_dir];
            }
            
            // stop near dungeon edge
            if ((y < 3) || (y > p_ptr->cur_map_hgt - 3) || (x < 3) || (x > p_ptr->cur_map_wid - 3)) break;

            // mark that we want to put a chasm here
            cave_info[y][x] |= (CAVE_TEMP);
        }
        
        // start by assuming it will be OK
        chasm_ok = TRUE;
        
        // count floor squares that will be turned to chasm
        floor_to_chasm = 0;
        
        // check it doesn't wreck the dungeon
        for (y = 1; y < p_ptr->cur_map_hgt-1; y++)
        {
            for (x = 1; x < p_ptr->cur_map_wid-1; x++)
            {
                // only inspect squares that are currently destined to be chasms
                if (cave_info[y][x] & (CAVE_TEMP))
                {
                    // avoid chasms in interesting rooms / vaults
                    if (cave_info[y][x] & (CAVE_ICKY))
                    {
                        chasm_ok = FALSE;
                    }
                    
                    // avoid two chasm square in a row in corridors
                    if ((cave_info[y+1][x] & (CAVE_TEMP)) &&
                        !(cave_info[y][x] & (CAVE_ROOM)) && !(cave_info[y+1][x] & (CAVE_ROOM)) &&
                        cave_floorlike_bold(y,x) && cave_floorlike_bold(y+1,x))
                    {
                        chasm_ok = FALSE;
                    }
                    if ((cave_info[y][x+1] & (CAVE_TEMP)) &&
                        !(cave_info[y][x] & (CAVE_ROOM)) && !(cave_info[y][x+1] & (CAVE_ROOM)) &&
                        cave_floorlike_bold(y,x) && cave_floorlike_bold(y,x+1))
                    {
                        chasm_ok = FALSE;
                    }
                    
                    // avoid a chasm taking out the rock next to a door
                    if (cave_any_closed_door_bold(y+1,x) || cave_any_closed_door_bold(y-1,x) ||
                        cave_any_closed_door_bold(y,x+1) || cave_any_closed_door_bold(y,x-1))
                    {
                        chasm_ok = FALSE;
                    }
                    
                    // avoid a chasm just hitting the wall of a lit room (would look odd that the light doesn't hit the wall behind)
                    if (cave_wall_bold(y,x) && (cave_info[y][x] & (CAVE_GLOW)))
                    {
                        if ((cave_wall_bold(y+1,x) && !(cave_info[y+1][x] & (CAVE_GLOW)) && !(cave_info[y+1][x] & (CAVE_TEMP))) ||
                            (cave_wall_bold(y-1,x) && !(cave_info[y-1][x] & (CAVE_GLOW)) && !(cave_info[y-1][x] & (CAVE_TEMP))) ||
                            (cave_wall_bold(y,x+1) && !(cave_info[y][x+1] & (CAVE_GLOW)) && !(cave_info[y][x+1] & (CAVE_TEMP))) ||
                            (cave_wall_bold(y,x-1) && !(cave_info[y][x-1] & (CAVE_GLOW)) && !(cave_info[y][x-1] & (CAVE_TEMP))))
                        {
                            chasm_ok = FALSE;
                        }
                    }
                        
                    // avoid a chasm having no squares in a room/corridor
                    if (cave_floor_bold(y,x))
                    {
                        floor_to_chasm++;
                    }
                }
            }
        }
        
        // the chasm must affect at least one floor square
        if (floor_to_chasm < 1) chasm_ok = FALSE;
        
        // clear the flag for failed chasm placement
        if (!chasm_ok)
        {
            for (y = 0; y < p_ptr->cur_map_hgt; y++)
            {
                for (x = 0; x < p_ptr->cur_map_wid; x++)
                {
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        cave_info[y][x] &= ~(CAVE_TEMP);
                    }
                }
            }
        }
    }

    // actually place the chasm and clear the flag
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_info[y][x] & (CAVE_TEMP))
            {
                cave_set_feat(y, x, FEAT_CHASM);
                cave_info[y][x] &= ~(CAVE_TEMP);
            }
        }
    }

	return (TRUE);
}


/*
 * Places chasms through dungeon
 */
static void build_chasms(void)
{
    int i;
    int chasms = 0;
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT) * (p_ptr->cur_map_wid / PANEL_WID_FIXED);
    
    // determine whether to add chasms, and how many
    if ((p_ptr->depth > 2) && (p_ptr->depth < MORGOTH_DEPTH - 1) && percent_chance(p_ptr->depth + 30))
    {
        // add some chasms
        chasms += damroll(1, panels / 3);

        // flip a coin, and if it is heads...
        while (one_in_(2))
        {
            // add some more chasms and flip again...
            chasms += damroll(1, panels / 3);
        }
    }
    
    // build them
    for (i = 0; i < chasms; i++)
    {
        build_chasm();
    }
    
    if (cheat_room && (chasms > 0)) msg_format("%d chasms.", chasms);
}


/*
 * Generate helper -- test a rectangle to see if it is all rock (i.e. not floor and not icky)
 */
static bool solid_rock(int y1, int x1, int y2, int x2)
{
	int y, x;

	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			if (cave_feat[y][x] == FEAT_FLOOR)
				return (FALSE);
			if (cave_info[y][x] & CAVE_ICKY)
				return (FALSE);
		}
	}
	return (TRUE);
}

/*
 * Sil
 * Generate helper -- test around a rectangle to see if there would be a doubled wall
 *
 * eg:
 *       ######
 * #######....#
 * #....##....#
 * #....#######
 * ######
 */
static bool doubled_wall(int y1, int x1, int y2, int x2)
{
	int y, x;

	/* check top wall */
	for (x = x1; x < x2; x++)
	{
		if ((cave_feat[y1-2][x] == FEAT_WALL_OUTER) && (cave_feat[y1-2][x+1] == FEAT_WALL_OUTER))
			return (TRUE);
	}

	/* check bottom wall */
	for (x = x1; x < x2; x++)
	{
		if ((cave_feat[y2+2][x] == FEAT_WALL_OUTER) && (cave_feat[y2+2][x+1] == FEAT_WALL_OUTER))
			return (TRUE);
	}

	/* check left wall */
	for (y = y1; y < y2; y++)
	{
		if ((cave_feat[y][x1-2] == FEAT_WALL_OUTER) && (cave_feat[y+1][x1-2] == FEAT_WALL_OUTER))
			return (TRUE);
	}

	/* check right wall */
	for (y = y1; y < y2; y++)
	{
		if ((cave_feat[y][x2+2] == FEAT_WALL_OUTER) && (cave_feat[y+1][x2+2] == FEAT_WALL_OUTER))
			return (TRUE);
	}

	return (FALSE);
}


/*
 * Generate helper -- create a new room with optional light
 */
static void generate_room(int y1, int x1, int y2, int x2, int light)
{
	int y, x;

	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			cave_info[y][x] |= (CAVE_ROOM);
			if (light) cave_info[y][x] |= (CAVE_GLOW);
		}
	}
}


/*
 * Generate helper -- fill a rectangle with a feature
 */
static void generate_fill(int y1, int x1, int y2, int x2, int feat)
{
	int y, x;

	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			cave_set_feat(y, x, feat);
		}
	}
}


/*
 * Generate helper -- draw a rectangle with a feature
 */
static void generate_draw(int y1, int x1, int y2, int x2, int feat)
{
	int y, x;

	for (y = y1; y <= y2; y++)
	{
		cave_set_feat(y, x1, feat);
		cave_set_feat(y, x2, feat);
	}

	for (x = x1; x <= x2; x++)
	{
		cave_set_feat(y1, x, feat);
		cave_set_feat(y2, x, feat);
	}
}


/*
 * Generate helper -- split a rectangle with a feature
 */
static void generate_plus(int y1, int x1, int y2, int x2, int feat)
{
	int y, x;
	int y0, x0;

	/* Center */
	y0 = (y1 + y2) / 2;
	x0 = (x1 + x2) / 2;

	for (y = y1; y <= y2; y++)
	{
		cave_set_feat(y, x0, feat);
	}

	for (x = x1; x <= x2; x++)
	{
		cave_set_feat(y0, x, feat);
	}
}


static bool h_tunnel_ok(int x1, int x2, int y, bool tentative, int desired_changes)
{
	int x, x_lo, x_hi, changes;
	
	x_lo = MIN(x1,x2);
	x_hi = MAX(x1,x2);
	changes = 0;
	
	/* Don't dig corridors ending at a room's outer wall (can happen at corners of L-corridors) */
	if ((cave_feat[y][x1] == FEAT_WALL_OUTER) || (cave_feat[y][x2] == FEAT_WALL_OUTER))
		return (FALSE);
	/* Don't dig L-corridors when the corner is too close to empty space */
	if (!(cave_info[y][x_lo] & (CAVE_ROOM)))
	{
		if ((cave_feat[y-1][x_lo-1] == FEAT_FLOOR) || (cave_feat[y+1][x_lo-1] == FEAT_FLOOR))
		{
			return (FALSE);
		}
	}
	if (!(cave_info[y][x_hi] & (CAVE_ROOM)))
	{
		if ((cave_feat[y-1][x_hi+1] == FEAT_FLOOR) || (cave_feat[y+1][x_hi+1] == FEAT_FLOOR))
		{
			return (FALSE);
		}
	}

	/* test each location in the corridor */
	for (x = x_lo; x <= x_hi; x++)
	{
		/* count the number of times it enters or leaves a room */
		if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) &&                           // to outside
						(cave_floor_bold(y,x-1) || (cave_feat[y][x-1] == FEAT_WALL_INNER))) // from inside
		{
			changes++;
		}
		if ((x > x_lo) && (cave_feat[y][x-1] == FEAT_WALL_OUTER) &&                      // from outside
		                 (cave_floor_bold(y,x) || (cave_feat[y][x] == FEAT_WALL_INNER))) // to inside
		{
			changes++;
		}
		
		/* abort if the tunnel would go through two adjacent squares of the outside wall of a room */
		if ((x > x_lo) && (cave_feat[y][x-1] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_WALL_OUTER))
		{
			return (FALSE);
		}

		/* abort if the tunnel would go from an outside wall to a door */
		if ((x > x_lo) && (cave_feat[y][x-1] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_DOOR_HEAD))
		{
			return (FALSE);
		}
		/* abort if the tunnel would go from a door to an outside wall */
		if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && (cave_feat[y][x-1] == FEAT_DOOR_HEAD))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would go from an outside wall into an inside wall */
		if ((x > x_lo) && (cave_feat[y][x-1] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_WALL_INNER))
		{
			return (FALSE);
		}
		if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && (cave_feat[y][x-1] == FEAT_WALL_INNER))
		{
			return (FALSE);
		}

		/* abort if the tunnel would directly enter a vault without going through a designated square */
		if ((x > x_lo) && (cave_feat[y][x-1] == FEAT_WALL_EXTRA) && 
		                 (cave_floor_bold(y,x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
		{
			return (FALSE);
		}
		if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA) && 
		                 (cave_floor_bold(y,x-1) || (cave_feat[y][x-1] == FEAT_WALL_INNER)))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would go through or adjacent to an existing door (except in vaults) */
		if (cave_known_closed_door_bold(y-1,x) && !(cave_info[y-1][x] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		if (cave_known_closed_door_bold(y,x) && !(cave_info[y][x] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		if (cave_known_closed_door_bold(y+1,x) && !(cave_info[y+1][x] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would have floor beside it at some point outside a room */
		if (((cave_feat[y+1][x] == FEAT_FLOOR) || (cave_feat[y-1][x] == FEAT_FLOOR)) && !(cave_info[y][x] & (CAVE_ROOM)))
		{
			return (FALSE);
		}
	}
	if (tentative && (changes != desired_changes))
	{
		return (FALSE);
	}
	else
	{
		return (TRUE);
	}
}

static bool v_tunnel_ok(int y1, int y2, int x, bool tentative, int desired_changes)
{
	int y, y_lo, y_hi, changes;
	
	y_lo = MIN(y1,y2);
	y_hi = MAX(y1,y2);
	changes = 0;

	/* Don't dig corridors ending at a room's outer wall (can happen at corners of L-corridors) */
	if ((cave_feat[y1][x] == FEAT_WALL_OUTER) || (cave_feat[y2][x] == FEAT_WALL_OUTER))
		return (FALSE);
	/* Don't dig L-corridors when the corner is too close to empty space */
	if (!(cave_info[y_lo][x] & (CAVE_ROOM)))
	{
		if ((cave_feat[y_lo-1][x-1] == FEAT_FLOOR) || (cave_feat[y_lo-1][x+1] == FEAT_FLOOR))
		{
			return (FALSE);
		}
	}
	if (!(cave_info[y_hi][x] & (CAVE_ROOM)))
	{
		if ((cave_feat[y_hi+1][x-1] == FEAT_FLOOR) || (cave_feat[y_hi+1][x+1] == FEAT_FLOOR))
		{
			return (FALSE);
		}
	}

	/* test each location in the corridor */	
	for (y = y_lo; y <= y_hi; y++)
	{
		/* count the number of times it enters or leaves a room */
		if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && 
		                 (cave_floor_bold(y-1,x) || (cave_feat[y-1][x] == FEAT_WALL_INNER)))
		{
			changes++;
		}
		if ((y > y_lo) && (cave_feat[y-1][x] == FEAT_WALL_OUTER) &&
						 (cave_floor_bold(y,x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
		{
			changes++;
		}
		
		/* abort if the tunnel would go through two adjacent squares of the outside wall of a room */
		if ((y > y_lo) && (cave_feat[y-1][x] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_WALL_OUTER))
		{
			return (FALSE);
		}

		/* abort if the tunnel would go from an outside wall to a door */
		if ((y > y_lo) && (cave_feat[y-1][x] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_DOOR_HEAD))
		{
			return (FALSE);
		}
		/* abort if the tunnel would go from a door to an outside wall */
		if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && (cave_feat[y-1][x] == FEAT_DOOR_HEAD))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would go from an outside wall into an inside wall */
		if ((y > y_lo) && (cave_feat[y-1][x] == FEAT_WALL_OUTER) && (cave_feat[y][x] == FEAT_WALL_INNER))
		{
			return (FALSE);
		}
		if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && (cave_feat[y-1][x] == FEAT_WALL_INNER))
		{
			return (FALSE);
		}

		/* abort if the tunnel would directly enter a vault without going through a designated square */
		if ((y > y_lo) && (cave_feat[y-1][x] == FEAT_WALL_EXTRA) && 
		                 (cave_floor_bold(y,x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
		{
			return (FALSE);
		}
		if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA) && 
		                 (cave_floor_bold(y-1,x) || (cave_feat[y-1][x] == FEAT_WALL_INNER)))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would go through, or adjacent to an existing (non-vault) door */
		if (cave_known_closed_door_bold(y,x-1) && !(cave_info[y][x-1] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		if (cave_known_closed_door_bold(y,x) && !(cave_info[y][x] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		if (cave_known_closed_door_bold(y,x+1) && !(cave_info[y][x+1] & (CAVE_ICKY)))
		{
			return (FALSE);
		}
		
		/* abort if the tunnel would have floor beside it at some point outside a room */
		if (((cave_feat[y][x+1] == FEAT_FLOOR) || (cave_feat[y][x-1] == FEAT_FLOOR)) && !(cave_info[y][x] & (CAVE_ROOM)))
		{
			return (FALSE);
		}
	}
	if (tentative && (changes != desired_changes))
	{
		return (FALSE);
	}
	else
	{
		return (TRUE);
	}
}

static void build_h_tunnel(int r1, int r2, int x1, int x2, int y)
{
	int x, x_lo, x_hi;
	
	x_lo = MIN(x1,x2);
	x_hi = MAX(x1,x2);
	
	for (x = x_lo; x <= x_hi; x++)
	{
		if (cave_feat[y][x] == FEAT_WALL_OUTER)
		{
			/* all doors get randomised later */
			cave_set_feat(y, x, FEAT_DOOR_HEAD);	
		}
		else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
		{
			cave_set_feat(y, x, FEAT_FLOOR);
			cave_corridor1[y][x] = r1;
			cave_corridor2[y][x] = r2;
		}

	}
}

static void build_v_tunnel(int r1, int r2, int y1, int y2, int x)
{
	int y, y_lo, y_hi;
	
	y_lo = MIN(y1,y2);
	y_hi = MAX(y1,y2);
	
	for (y = y_lo; y <= y_hi; y++)
	{
		if (cave_feat[y][x] == FEAT_WALL_OUTER)
		{
			/* all doors get randomised later */
			cave_set_feat(y, x, FEAT_DOOR_HEAD);
		}
		else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
		{
			cave_set_feat(y, x, FEAT_FLOOR);
			cave_corridor1[y][x] = r1;
			cave_corridor2[y][x] = r2;
		}
	}
}


static bool build_tunnel(int r1, int r2, int y1, int x1, int y2, int x2, bool tentative)
{
	/* build a vertical tunnel */
	if (x1 == x2)
	{
		if (!v_tunnel_ok(y1,y2,x1,tentative,2))
		{
			return (FALSE);
		}
		build_v_tunnel(r1,r2,y1,y2,x1);
	}
	
	/* build a horizontal tunnel */
	else if (y1 == y2)
	{
		if (!h_tunnel_ok(x1,x2,y1,tentative,2))
		{
			return (FALSE);
		}
		build_h_tunnel(r1,r2,x1,x2,y1);
	}

	/* build an L-shaped tunnel */
	else
	{
		/* build an h-v tunnel */
		if (one_in_(2))
		{
			if (!h_tunnel_ok(x1,x2,y1,tentative,1) || !v_tunnel_ok(y1,y2,x2,tentative,1))
			{
				return (FALSE);
			}
			build_h_tunnel(r1, r2, x1, x2, y1);
			build_v_tunnel(r1, r2, y1, y2, x2);
		}
		
		/* build a v-h tunnel */
		else
		{
			if (!h_tunnel_ok(x1,x2,y2,tentative,1) || !v_tunnel_ok(y1,y2,x1,tentative,1))
			{
				return (FALSE);
			}
			build_v_tunnel(r1, r2, y1, y2, x1);
			build_h_tunnel(r1, r2, x1, x2, y2);
		}
	}

	return (TRUE);
}


static bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate)
{
	int x, y;
	int r1y, r1x, r1y1, r1x1, r1y2, r1x2;
	int r2y, r2x, r2y1, r2x1, r2y2, r2x2;
	bool success;
	
	int distance_limitx = desperate ? 22 : 15;
	int distance_limity = desperate ? 16 : 10;
	
	r1y  = dun->cent[r1].y;
	r1x  = dun->cent[r1].x;
	r1y1 = dun->corner[r1].y1;
	r1x1 = dun->corner[r1].x1;
	r1y2 = dun->corner[r1].y2;
	r1x2 = dun->corner[r1].x2;

	r2y  = dun->cent[r2].y;
	r2x  = dun->cent[r2].x;
	r2y1 = dun->corner[r2].y1;
	r2x1 = dun->corner[r2].x1;
	r2y2 = dun->corner[r2].y2;
	r2x2 = dun->corner[r2].x2;
		
	/* if the rooms are too far apart, then just give up immediately */
	// look at total distance of room centres
	if ((ABS(r1y - r2y) > distance_limity * 3) || (ABS(r1x - r2x) > distance_limitx * 3))
	{
		return (FALSE);
	}
	// then look at distance of relevant room edges
	if ((r1x < r2x) && (r2x1 - r1x2 > distance_limitx))
	{
		return (FALSE);
	}
	if ((r2x < r1x) && (r1x1 - r2x2 > distance_limitx))
	{
		return (FALSE);
	}
	if ((r1y < r2y) && (r2y1 - r1y2 > distance_limity))
	{
		return (FALSE);
	}
	if ((r2y < r1y) && (r1y1 - r2y2 > distance_limity))
	{
		return (FALSE);
	}
			
	/* if we have vertical or horizontal overlap, connect a straight tunnel */
	/* at a random point where they overlap */
	
	/* if vertical overlap */
	if ((r1x1 <= r2x2) && (r2x1 <= r1x2))
	{
		/* unless careful, there will be too many vertical tunnels */
		/* since rooms are wider than they are tall                */
		if (tentative && one_in_(2))
		{
			return (FALSE);
		}
		x = rand_range(MAX(r1x1,r2x1), MIN(r1x2,r2x2));  // Sil-x: one of these two lines has somehow caused a crash: http://angband.oook.cz/ladder-show.php?id=13070

		success = build_tunnel(r1, r2, r1y, x, r2y, x, tentative);
	}
	/* if horizontal overlap */
	else if ((r1y1 <= r2y2) && (r2y1 <= r1y2))
	{
		y = rand_range(MAX(r1y1,r2y1), MIN(r1y2,r2y2));  // Sil-x: one of these two lines has somehow caused a crash
		
		success = build_tunnel(r1, r2, y, r1x, y, r2x, tentative);
	}
	
	/* otherwise, make an L shaped corridor between their centres */
	else 
	{
		// this must fail if any of the tunnels would be too long
		if (MIN(ABS(r2x - r1x1), ABS(r2x - r1x2)) > distance_limitx - 2) return (FALSE);
		if (MIN(ABS(r1x - r2x1), ABS(r1x - r2x2)) > distance_limitx - 2) return (FALSE);
		if (MIN(ABS(r2y - r1y1), ABS(r2y - r1y2)) > distance_limity - 2) return (FALSE);
		if (MIN(ABS(r1y - r2y1), ABS(r1y - r2y2)) > distance_limity - 2) return (FALSE);
		
		success = build_tunnel(r1, r2, r1y, r1x, r2y, r2x, tentative);
	}
	
	if (success)
	{
		dun->connection[r1][r2] = TRUE;
		dun->connection[r2][r1] = TRUE;	
	}
	
	return (success);
}



static bool connect_room_to_corridor(int r)
{
	int length = 10;
	int x;
	int y;
	int delta;
	int ry, rx, r1, r2;
	bool success = FALSE;
	bool done = FALSE;
		
	ry  = dun->cent[r].y;
	rx  = dun->cent[r].x;
	
	y = ry;
	x = rx;
	
	// go down/right half the time, up/left the other half
	if (one_in_(2)) delta = 1;
	else			delta = -1;
	
	// go horizontal half the time, vertical the other half
	if (one_in_(2))
	{
		while (!done)
		{
			y += delta;
			
			// abort if the tunnel leaves the map or passes through a door
			if (!in_bounds(y,x) || (ABS(y - ry) > length) || cave_any_closed_door_bold(y,x))
			{
				success = FALSE;
				done = TRUE;
			}
			
			// it has intercepted a tunnel!
			else if ((cave_feat[y][x] == FEAT_FLOOR) && !(cave_info[y][x] & (CAVE_ROOM)))
			{
				r1 = cave_corridor1[y][x];
				r2 = cave_corridor2[y][x];
				
				// make sure that the tunnel intercepts only connects rooms that aren't connected to this room
				if ((r1 < 0) || (r2 < 0) || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
				{
					if (v_tunnel_ok(ry, y-(delta*2), x, TRUE, 1))
					{
						build_v_tunnel(r, r1, ry, y, x);
						
						// mark the new room connections
						dun->connection[r][r1] = TRUE;
						dun->connection[r1][r] = TRUE;
						dun->connection[r][r2] = TRUE;
						dun->connection[r2][r] = TRUE;
						success = TRUE;
					}
				}
				
				done = TRUE;
			}
		}
	}
	
	// do the vertical case (very similar to the horizontal one!)
	else
	{
		while (!done)
		{
			x += delta;
			
			// abort if the tunnel leaves the map or passes through a door
			if (!in_bounds(y,x) || (ABS(x - rx) > length) || cave_any_closed_door_bold(y,x))
			{
				success = FALSE;
				done = TRUE;
			}
			
			// it has intercepted a tunnel!
			else if ((cave_feat[y][x] == FEAT_FLOOR) && !(cave_info[y][x] & (CAVE_ROOM)))
			{
				r1 = cave_corridor1[y][x];
				r2 = cave_corridor2[y][x];
				
				// make sure that the tunnel intercepts only connects rooms that aren't connected to this room
				if ((r1 < 0) || (r2 < 0) || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
				{
					if (h_tunnel_ok(rx, x-(delta*2), y, TRUE, 1))
					{
						build_h_tunnel(r, r1, rx, x, y);
						
						// mark the new room connections
						dun->connection[r][r1] = TRUE;
						dun->connection[r1][r] = TRUE;
						dun->connection[r][r2] = TRUE;
						dun->connection[r2][r] = TRUE;
						success = TRUE;
					}
				}
				
				done = TRUE;
			}
		}		
	}

	return (success);
}




/*
 * Places some staircases near walls
 */
static bool alloc_stairs(int feat, int num)
{
	int x;

	/*smaller levels don't need that many stairs, but there are a minimum of 4 rooms*/
	if (dun->cent_n == 4) num = 1;
	else if (num > (dun->cent_n / 2)) num = dun->cent_n / 2;

	/* Place "num" stairs */
	for (x = 0; x < num; x++)
	{
		int i;

		int yy, xx;

		for (i = 0; i <= 1000; i++)
		{
			yy = rand_int(p_ptr->cur_map_hgt);
			xx = rand_int(p_ptr->cur_map_wid);
			
			/* make sure the square is empty, in a room and has no adjacent doors*/
			if (cave_naked_bold(yy, xx) && (cave_info[yy][xx] & (CAVE_ROOM)))
				if ((cave_feat[yy-1][xx] != FEAT_DOOR_HEAD) &&
					(cave_feat[yy][xx-1] != FEAT_DOOR_HEAD) &&
					(cave_feat[yy+1][xx] != FEAT_DOOR_HEAD) &&
					(cave_feat[yy][xx+1] != FEAT_DOOR_HEAD))
				{
					break;
				}
			if (i == 1000)
			{
				return (FALSE);
			}
		}

		/* Surface -- must go down */
		if (!p_ptr->depth)
		{
			/* Clear previous contents, add down stairs */
			cave_set_feat(yy, xx, FEAT_MORE);
		}

		/* Bottom -- must go up */
		else if (p_ptr->depth >= MORGOTH_DEPTH)
		{
			/* Clear previous contents, add up stairs */
			if (x != 0) cave_set_feat(yy, xx, FEAT_LESS);
			else cave_set_feat(yy, xx, choose_up_stairs());
		}

		/* Requested type */
		else
		{
			/* Allow shafts, but guarantee the first one is an ordinary stair */
			if (x != 0)
			{
				if      (feat == FEAT_LESS) feat = choose_up_stairs();
				else if (feat == FEAT_MORE) feat = choose_down_stairs();
			}

			/* Clear previous contents, add stairs */
			cave_set_feat(yy, xx, feat);
		}
	}

	return (TRUE);
}


bool feat_within_los(int y0, int x0, int feat)
{
	int y, x;

	bool detect = FALSE;


	/* Scan the visible area */
	for (y = y0 - 15; y < y0 + 15; y++)
	{
		for (x = x0 - 15; x < x0 + 15; x++)
		{
			if (!in_bounds_fully(y, x)) continue;
			
			if (!los(y0, x0, y, x)) continue;

			/* Detect invisible traps */
			if (cave_feat[y][x] == feat)
			{
				detect = TRUE;
			}
		}
	}

	/* Result */
	return (detect);

} 


/*
 * Are there any stairs within line of sight?
 */
bool stairs_within_los(int y, int x)
{
	if (feat_within_los(y, x, FEAT_LESS)) return (TRUE);
	if (feat_within_los(y, x, FEAT_MORE)) return (TRUE);
	if (feat_within_los(y, x, FEAT_LESS_SHAFT)) return (TRUE);
	if (feat_within_los(y, x, FEAT_MORE_SHAFT)) return (TRUE);

	// else:
	
	return (FALSE);
}


/*
 * Determines the chance (out of 1000) that a trap will be placed in a given square.
 */
static int trap_placement_chance(int y, int x)
{
    int yy, xx;
    
    int chance = 0;
    
    // extra chance of having a trap for certain squares inside rooms
    if (cave_clean_bold(y,x) && (cave_info[y][x] & (CAVE_ROOM)))
    {
        chance = 1;
        
        // check the squares that neighbour (y,x)
        for (yy = y - 1; yy <= y + 1; yy++)
        {
            for (xx = x - 1; xx <= x + 1; xx++)
            {
                if (!((yy==y) && (xx==x)))
                {
                    // item
                    if (cave_o_idx[yy][xx] != 0)              chance += 10;
                    
                    // stairs
                    if (cave_stair_bold(yy,xx))               chance += 10;
                    
                    // closed doors (including secret)
                    if (cave_any_closed_door_bold(yy,xx))     chance += 10;
                }
            }
        }
        
        // opposing impassable squares (chasm or wall)
        if (cave_impassable_bold(y-1,x) && cave_impassable_bold(y+1,x)) chance += 10;
        if (cave_impassable_bold(y,x-1) && cave_impassable_bold(y,x+1)) chance += 10;
    }
    
    return (chance);
}


/*
 * Place traps randomly on the level.
 * Biased towards certain sneaky locations.
 */
static void place_traps(void)
{
    int y, x;
    
	// scan the map
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
            // randomly determine whether to place a trap based on the above
            if (dieroll(1000) <= trap_placement_chance(y, x))
            {
                place_trap(y,x);
            }
		}
	}
}


static bool place_rubble_player(void)
{
	int r;
	int y, x;
	int i, panels;
	
	/* Basic "amount" */
	
	panels = (p_ptr->cur_map_hgt / PANEL_HGT) * (p_ptr->cur_map_wid / PANEL_WID_FIXED);
	
	r = dieroll(panels / 3); 
    
	// occasionally produce much more rubble on deep levels
	if ((p_ptr->depth >= 10) && one_in_(10)) r += panels * 2;

	/* Put some rubble in corridors */
	alloc_object(ALLOC_SET_BOTH, ALLOC_TYP_RUBBLE, r, FALSE);

	/* simple way to place player */
	for (i = 0; i <= 100; i++)
	{
		y = rand_int(p_ptr->cur_map_hgt);
		x = rand_int(p_ptr->cur_map_wid);
		
		// require empty square that isn't in an interesting room or vault
		if (cave_naked_bold(y, x) && !(cave_info[y][x] & (CAVE_ICKY)))
		{
			// require a room if it is the first level
			if ((playerturn > 0) || (cave_info[y][x] & (CAVE_ROOM)))
			{
				// don't generate stairs within line of sight if player arrived using stairs
				if (!stairs_within_los(y, x) || (p_ptr->create_stair == FALSE))
				{
					player_place(y, x);
					
					break;
				}
			}
		}
		if (i == 100)   return (FALSE);
	}
	
	return (TRUE);
}


/*
 *  Make sure that the level is sufficiently connected.
 */

bool check_connectivity(void)
{
	int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
	int y, x;

	// Reset the array used for checking connectivity
	for (y = 0; y < p_ptr->cur_map_hgt; y++)	
		for (x = 0; x < p_ptr->cur_map_wid; x++)
			cave_access[y][x] = FALSE;
	
	// Make sure entire dungeon is connected (ignoring rubble)
	flood_access(p_ptr->py, p_ptr->px, cave_access, TRUE);
	for (y = 0; y < p_ptr->cur_map_hgt; y++)	
		for (x = 0; x < p_ptr->cur_map_wid; x++)
			if (player_passable(y,x,TRUE) && (cave_access[y][x] == FALSE))
			{
				return (FALSE);
			}

	// Reset the array used for checking connectivity
	for (y = 0; y < p_ptr->cur_map_hgt; y++)	
		for (x = 0; x < p_ptr->cur_map_wid; x++)
			cave_access[y][x] = FALSE;
	
	// Make sure player can reach stairs without going through rubble
	flood_access(p_ptr->py, p_ptr->px, cave_access, FALSE);
	for (y = 0; y < p_ptr->cur_map_hgt; y++)	
		for (x = 0; x < p_ptr->cur_map_wid; x++)
			if ((cave_access[y][x] == TRUE) && cave_stair_bold(y,x))
			{
				return (TRUE);
			}
	
	return (FALSE);
}


/*
 *  Check if there are two adjacent doors on the level.
 */
bool doubled_doors(void)
{
	int y, x;
	
	// Check each grid within boundary
	for (y = 0; y < p_ptr->cur_map_hgt - 1; y++)	
		for (x = 0; x < p_ptr->cur_map_wid - 1; x++)
			if (cave_known_closed_door_bold(y,x))
			{
				if (cave_known_closed_door_bold(y+1,x)) return (TRUE);
				if (cave_known_closed_door_bold(y,x+1)) return (TRUE);
			}
	
	return (FALSE);
}


static bool connect_rooms_stairs(void)
{
	int i;
	int corridor_attempts;
	int r1, r2, r_closest, d_closest, d;
	int pieces = 0;
	
    int width;
	int stairs = 0;
	
	bool joined;

	// Phase 1: 
	// connect each room to the closest room (if not already connected)
	
	for (r1 = 0; r1 < dun->cent_n; r1++)
	{
		/* find closest room */
		r_closest = 0;		/* default values that will get beaten trivially */
		d_closest = 1000;
		for (r2 = 0; r2 < dun->cent_n; r2++)
		{
			if (r2 != r1)
			{
				d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x); 
				if (d < d_closest)
				{
					d_closest = d;
					r_closest = r2;
				}
			}
		}
		
		/* connect the rooms, if not already connected */
		if (!(dun->connection[r1][r_closest]))
		{
			(void) connect_two_rooms(r1, r_closest, TRUE, FALSE);
		}
	}

	// Phase 2:
	// make some random connections between rooms so long as they don't intersect things
	
	switch (p_ptr->cur_map_hgt / PANEL_HGT)
	{
		case 3:		corridor_attempts = dun->cent_n * dun->cent_n;
		case 4:		corridor_attempts = dun->cent_n * dun->cent_n * 2;
		case 5:		corridor_attempts = dun->cent_n * dun->cent_n * 10;
		default:	corridor_attempts = dun->cent_n * dun->cent_n * 10;
	}
	
	for (i = 0; i < corridor_attempts; i++)
	{
		r1 = rand_int(dun->cent_n);
		r2 = rand_int(dun->cent_n);
		if ((r1 != r2) && !(dun->connection[r1][r2]))
		{
			(void) connect_two_rooms(r1, r2, TRUE, FALSE);
		}
	}

	// add some T-intersections in the corridors
	for (i = 0; i < corridor_attempts; i++)
	{
		r1 = rand_int(dun->cent_n);
		(void) connect_room_to_corridor(r1);
	}
	
	// Phase 3:
	// cut the dungeon up into connected pieces and try hard to make corridors
	// that connect them
	
	pieces = dungeon_pieces();
	while (pieces > 1)
	{
		joined = FALSE;
		
		for (r1 = 0; r1 < dun->cent_n; r1++)
		{ 
			for (r2 = 0; r2 < dun->cent_n; r2++)
			{
				if (!joined && (dun->piece[r1] != dun->piece[r2]))
				{
					for (i = 0; i < 10; i++)
					{
						if (!(dun->connection[r1][r2]))
						{
							joined = connect_two_rooms(r1, r2, TRUE, TRUE);
						}
					}
				}
			}
		}
		
		if (!joined) break;
		
		// cut the dungeon up into connected pieces and stop if there is only one
		pieces = dungeon_pieces();
	}

	//label_rooms();

	/* Place down stairs */
    width = (p_ptr->cur_map_wid / PANEL_WID_FIXED);
    
    if (width <= 3)         stairs = 1;
    else if (width == 4)    stairs = 2;
    else                    stairs = 4;
    
	if ((p_ptr->create_stair == FEAT_MORE) || (p_ptr->create_stair == FEAT_MORE_SHAFT))
		stairs--;
	if (!(alloc_stairs(FEAT_MORE, stairs)))
	{
		if (cheat_room) msg_format("Failed to place down stairs.");

		return(FALSE);
	}

	/* Place up stairs */

    if (width <= 3)         stairs = 1;
    else if (width == 4)    stairs = 2;
    else                    stairs = 4;
    
    if ((p_ptr->create_stair == FEAT_LESS) || (p_ptr->create_stair == FEAT_LESS_SHAFT))
		stairs--;
	if (!(alloc_stairs(FEAT_LESS, stairs)))
	{
		if (cheat_room) msg_format("Failed to place up stairs.");

		return(FALSE);
	}
	
	/* Hack -- Add some quartz streamers */
	for (i = 0; i < DUN_STR_QUA; i++)
	{
		/*if we can't build streamers, something is wrong with level*/
		if (!build_streamer(FEAT_QUARTZ)) return (FALSE);
	}

    // add any chasms if needed
    build_chasms();


	return (TRUE);

}



/*
 * Room building routines.
 *
 * Six basic room types:
 *   1 -- normal
 *   2 -- cross shaped
 *   3 -- (removed)
 *   4 -- large room with features (removed)
 *   5 -- monster nests (removed)
 *   6 -- least vaults (formerly: monster pits)
 *   7 -- lesser vaults
 *   8 -- greater vaults
 */


/*
 * Type 1 -- normal rectangular rooms
 */
static bool build_type1(int y0, int x0)
{
	int y, x;

	int y1, x1, y2, x2;

	int light = FALSE;
	
	// Occasional light - chance of darkness starts very small and
	// increases quadratically until always dark at 950 ft
	if ((p_ptr->depth < dieroll(MORGOTH_DEPTH - 1)) || (p_ptr->depth < dieroll(MORGOTH_DEPTH - 1)))
	{
		light = TRUE;
	}

	/* Pick a room size */
	y1 = y0 - dieroll(3);
	x1 = x0 - dieroll(5);
	y2 = y0 + dieroll(3);
	x2 = x0 + dieroll(4)+1;

	/* Sil: bounds checking */
	if ((y1 <= 3) || (x1 <=3) || (y2 >= p_ptr->cur_map_hgt-3) || (x2 >= p_ptr->cur_map_wid-3))
	{
		return (FALSE);
	}
	
	/* Check to see if the location is all plain rock */
	if (!solid_rock(y1-1, x1-1, y2+1, x2+1))
	{
		return (FALSE);
	}
	
	if (doubled_wall(y1, x1, y2, x2))
	{
		return (FALSE);
	}
	
	/* Save the corner locations */
	dun->corner[dun->cent_n].y1 = y1;
	dun->corner[dun->cent_n].x1 = x1;
	dun->corner[dun->cent_n].y2 = y2;
	dun->corner[dun->cent_n].x2 = x2;

	/* Save the room location */
	dun->cent[dun->cent_n].y = y0;
	dun->cent[dun->cent_n].x = x0;
	dun->cent_n++;
	
	/* Generate new room */
	generate_room(y1-1, x1-1, y2+1, x2+1, light);

	/* Generate outer walls */
	generate_draw(y1-1, x1-1, y2+1, x2+1, FEAT_WALL_OUTER);

	/* Generate inner floors */
	generate_fill(y1, x1, y2, x2, FEAT_FLOOR);

	/* Hack -- Occasional pillar room */
	if (one_in_(20) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
	{
		for (y = y1 + 1; y <= y2; y += 2)
		{
			for (x = x1 + 1; x <= x2; x += 2)
			{
				cave_set_feat(y, x, FEAT_WALL_INNER);
			}
		}
	}

	/* Hack -- Occasional pillar-lined room */
	if (one_in_(10) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
	{
		for (y = y1 + 1; y <= y2; y += 2)
		{
			for (x = x1 + 1; x <= x2; x += 2)
			{
				if ((x == x1+1) || (x == x2-1) || (y == y1+1) || (y == y2-1))
				{
					cave_set_feat(y, x, FEAT_WALL_INNER);
				}
			}
		}
	}

	return (TRUE);
}


/*
 * Type 2 -- Cross shaped rooms
 */
static bool build_type2(int y0, int x0)
{
	int y, x;

	int y1h, x1h, y2h, x2h;
	int y1v, x1v, y2v, x2v;
		
	int h_hgt, h_wid, v_hgt, v_wid;

	int light = FALSE;
	
	/* Occasional light - always at level 1 through to never at Morgoth's level */
	if (p_ptr->depth < dieroll(MORGOTH_DEPTH)) light = TRUE;

	/* Pick a room size */
	
	h_hgt = 1;               /* 3 */
	h_wid = rand_range(5,7); /* 11, 13, 15 */
	
	y1h = y0 - h_hgt;
	x1h = x0 - h_wid;
	y2h = y0 + h_hgt;
	x2h = x0 + h_wid;

	v_hgt = rand_range(3,6); /* 7, 9, 11, 13 */
	v_wid = rand_range(1,2); /* 3, 5 */

	y1v = y0 - v_hgt;
	x1v = x0 - v_wid;
	y2v = y0 + v_hgt;
	x2v = x0 + v_wid;

	/* Sil: bounds checking */
	if ((y1v <= 3) || (x1h <=3) || (y2v >= p_ptr->cur_map_hgt-3) || (x2h >= p_ptr->cur_map_wid-3))
	{
		return (FALSE);
	}
	
	/* Check to see if the location is all plain rock */
	if (!solid_rock(y1v-1, x1h-1, y2v+1, x2h+1))
	{
		return (FALSE);
	}
	
	if (doubled_wall(y1v, x1h, y2v, x2h))
	{
		return (FALSE);
	}
			
	/* Save the corner locations */
	dun->corner[dun->cent_n].y1 = y1v;
	dun->corner[dun->cent_n].x1 = x1h;
	dun->corner[dun->cent_n].y2 = y2v;
	dun->corner[dun->cent_n].x2 = x2h;

	/* Save the room location */
	dun->cent[dun->cent_n].y = y0;
	dun->cent[dun->cent_n].x = x0;
	dun->cent_n++;

	/* Generate new room */
	generate_room(y1h-1, x1h-1, y2h+1, x2h+1, light);
	generate_room(y1v-1, x1v-1, y2v+1, x2v+1, light);

	/* Generate outer walls */
	generate_draw(y1h-1, x1h-1, y2h+1, x2h+1, FEAT_WALL_OUTER);
	generate_draw(y1v-1, x1v-1, y2v+1, x2v+1, FEAT_WALL_OUTER);

	/* Generate inner floors */
	generate_fill(y1h, x1h, y2h, x2h, FEAT_FLOOR);
	generate_fill(y1v, x1v, y2v, x2v, FEAT_FLOOR);


	/* Hack -- Occasional pillar room */
	
	switch (dieroll(7))
	{
		case 1:
		{
			if ((v_wid == 2) && (v_hgt == 6))
			{
				for (y = y1v + 1; y <= y2v; y += 2)
				{
					for (x = x1v + 1; x <= x2v; x += 2)
					{
						cave_set_feat(y, x, FEAT_WALL_INNER);
					}
				}
				place_object(y0, x0, FALSE, FALSE, DROP_TYPE_CHEST);
			}
			break;
		}
		case 2:
		{
			if ((v_wid == 1) && (h_hgt == 1))
			{
				generate_plus(y0-1, x0-1, y0+1, x0+1, FEAT_WALL_INNER);
			}
			break;
		}
		case 3:
		{
			if ((v_wid == 1) && (h_hgt == 1))
			{
				cave_set_feat(y0-1, x0-1, FEAT_WALL_INNER);
				cave_set_feat(y0+1, x0-1, FEAT_WALL_INNER);
				cave_set_feat(y0-1, x0+1, FEAT_WALL_INNER);
				cave_set_feat(y0+1, x0+1, FEAT_WALL_INNER);
			}
			break;
		}
		case 4:
		{
			if ((v_wid == 1) && (h_hgt == 1))
			{
				cave_set_feat(y0, x0-1, FEAT_WALL_INNER);
				cave_set_feat(y0, x0+1, FEAT_WALL_INNER);
				cave_set_feat(y0-1, x0, FEAT_WALL_INNER);
				cave_set_feat(y0+1, x0, FEAT_WALL_INNER);
			}
			break;
		}
		default:
		{
			break;
		}
	}


	return (TRUE);
}

/*
 *  Has a very good go at placing a monster of kind represented by a flag
 *  (eg RF3_DRAGON) at (y,x). It is goverened by a maximum depth and tries
 *  100 times at this depth and each depth below it.
 */
extern void place_monster_by_flag(int y, int x, int flagset, u32b f, bool allow_unique, int max_depth)
{
	bool got_r_idx = FALSE;
	int tries = 0;
	int r_idx;
	monster_race *r_ptr;
	int depth = max_depth;
		
	while (!got_r_idx && (depth > 0))
	{		
		r_idx = get_mon_num(depth, FALSE, TRUE, TRUE);
		r_ptr = &r_info[r_idx];
        
        if (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE)))
        {
            if (((flagset == 1) && (r_ptr->flags1 & (f))) ||
                ((flagset == 2) && (r_ptr->flags2 & (f))) ||
                ((flagset == 3) && (r_ptr->flags3 & (f))) ||
                ((flagset == 4) && (r_ptr->flags4 & (f))))
            {
                got_r_idx = TRUE;
                break;
            }
        }
		
		tries++;
		if (tries >= 100)
		{
			tries = 0;
			depth--;
		}
	}
	
	// place a monster of that type if you could find one
	if (got_r_idx) place_monster_one(y, x, r_idx, TRUE, FALSE, NULL);
	
}


/*
 *  Has a very good go at placing a monster of kind represented by its racial letter
 *  (eg 'v' for vampire) at (y,x). It is goverened by a maximum depth and tries
 *  100 times at this depth and each depth below it.
 */
void place_monster_by_letter(int y, int x, char c, bool allow_unique, int max_depth)
{
	bool got_r_idx = FALSE;
	int tries = 0;
	int r_idx;
	monster_race *r_ptr;
	int depth = max_depth;
	
	while (!got_r_idx && (depth > 0))
	{		
		r_idx = get_mon_num(depth, FALSE, TRUE, TRUE);
		r_ptr = &r_info[r_idx];
		if ((r_ptr->d_char = c) && (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE))))
		{
			got_r_idx = TRUE;
			break;
		}
		
		tries++;
		if (tries >= 100)
		{
			tries = 0;
			depth--;
		}
	}
	
	// place a monster of that type if you could find one
	if (got_r_idx) place_monster_one(y, x, r_idx, TRUE, FALSE, NULL);
	
}


/*
 * Hack -- fill in "vault" rooms
 */
static bool build_vault(int y0, int x0, vault_type *v_ptr, bool flip_d)
{
    int ymax = v_ptr->hgt;
    int xmax = v_ptr->wid;
    cptr data = v_text + v_ptr->text;
	int dx, dy, x, y;
	int ax, ay;
	bool flip_v = FALSE;
	bool flip_h = FALSE;
    int multiplier;

	int original_object_level = object_level;
	int original_monster_level = monster_level;

	cptr t;

	// Check that the vault doesn't contain invalid things for its depth
	for (t = data, dy = 0; dy < ymax; dy++)
	{	
		for (dx = 0; dx < xmax; dx++, t++)
		{
			// Barrow wights can't be deeper than level 12
			if ((*t == 'W') && (p_ptr->depth > 12))
			{
				//msg_print("Skipped a barrow wight vault.");
				return (FALSE);
			}

            // chasms can't occur at 950 ft
			if ((*t == '7') && (p_ptr->depth >= MORGOTH_DEPTH - 1))
			{
				return (FALSE);
			}
		}
	}

    // reflections
    if ((p_ptr->depth > 0) && (p_ptr->depth < MORGOTH_DEPTH))
    {
        // reflect it vertically half the time
        if (one_in_(2)) flip_v = TRUE;
        
        // reflect it horizontally half the time
        if (one_in_(2)) flip_h = TRUE;
    }

	/* Place dungeon features and objects */
	for (t = data, dy = 0; dy < ymax; dy++)
	{
		if (flip_v) ay = ymax - 1 - dy;
		else ay = dy;

		for (dx = 0; dx < xmax; dx++, t++)
		{
			if (flip_h) ax = xmax - 1 - dx;
			else ax = dx;

			/* Extract the location */
			x = x0 - (xmax / 2) + ax;
			y = y0 - (ymax / 2) + ay;
            
            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;

			/* Lay down a floor */
			cave_set_feat(y, x, FEAT_FLOOR);

			/* Part of a vault */
			cave_info[y][x] |= (CAVE_ROOM | CAVE_ICKY);

			/* Analyze the grid */
			switch (*t)
			{
				/* Granite wall (outer) */
				case '$':
				{
					cave_set_feat(y, x, FEAT_WALL_OUTER);
					break;
				}

				/* Granite wall (inner) */
				case '#':
				{
					cave_set_feat(y, x, FEAT_WALL_INNER);
					break;
				}

				/* Quartz vein */
				case '%':
				{
					cave_set_feat(y, x, FEAT_QUARTZ);
					break;
				}

				/* Rubble */
				case ':':
				{
					cave_set_feat(y, x, FEAT_RUBBLE);
					break;
				}

				/* Glyph of warding */
				case ';':
				{
					cave_set_feat(y, x, FEAT_GLYPH);
					break;
				}
					
					/* Down staircase */
				case '>':
				{
					cave_set_feat(y, x, FEAT_MORE);
					break;
				}

				/* Up staircase */
				case '<':
				{
					cave_set_feat(y, x, FEAT_LESS);
					break;
				}

				/* Visible door */
				case '+':
				{
					place_closed_door(y, x);
					break;
				}

				/* Secret door */
				case 's':
				{
					place_secret_door(y, x);
					break;
				}

				/* Trap */
				case '^':
				{
					if (one_in_(2)) place_trap(y, x);
					break;
				}

				/* Forge */
				case '0':
				{
					place_forge(y, x);
					break;
				}

                /* Chasm */
				case '7':
				{
					cave_set_feat(y, x, FEAT_CHASM);
					break;
				}
                
                /* Not actually part of the vault after all */
                case ' ':
                {
                    // remove room and vault flags
                    cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
                    break;
                }
            }
		}
	}

	/* Place dungeon monsters and objects */
	for (t = data, dy = 0; dy < ymax; dy++)
	{	
		if (flip_v) ay = ymax - 1 - dy;
		else ay = dy;

		for (dx = 0; dx < xmax; dx++, t++)
		{
			if (flip_h) ax = xmax - 1 - dx;
			else ax = dx;

			/* Extract the grid */
			x = x0 - (xmax/2) + ax;
			y = y0 - (ymax/2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;

			/* Analyze the symbol */
			switch (*t)
			{
				/* A monster from 1 level deeper */
				case '1':
				{
					monster_level = p_ptr->depth + 1;
					place_monster(y, x, TRUE, TRUE, TRUE);
					monster_level = original_monster_level;
					break;
				}

				/* A monster from 2 levels deeper */
				case '2':
				{
					monster_level = p_ptr->depth + 2;
					place_monster(y, x, TRUE, TRUE, TRUE);
					monster_level = original_monster_level;
					break;
				}

				/* A monster from 3 levels deeper */
				case '3':
				{
					monster_level = p_ptr->depth + 3;
					place_monster(y, x, TRUE, TRUE, TRUE);
					monster_level = original_monster_level;
					break;
				}

				/* A monster from 4 levels deeper */
				case '4':
				{
					monster_level = p_ptr->depth + 4;
					place_monster(y, x, TRUE, TRUE, TRUE);
					monster_level = original_monster_level;
					break;
				}

				/* An object from 1-4 levels deeper */
				case '*':
				{
					object_level = p_ptr->depth + dieroll(4);
					place_object(y, x, FALSE, FALSE, DROP_TYPE_NOT_USELESS);
					object_level = original_object_level;
					break;
				}

				/* A good object from 1-4 levels deeper */
				case '&':
				{
					object_level = p_ptr->depth + dieroll(4);
					place_object(y, x, TRUE, FALSE, DROP_TYPE_NOT_USELESS);
					object_level = original_object_level;
					break;
				}

				/* A chest from 4 levels deeper */
				case '~':
				{
					if (p_ptr->depth == 0) object_level = MORGOTH_DEPTH;
					else                   object_level = p_ptr->depth + 4;;
					
					place_object(y, x, FALSE, FALSE, DROP_TYPE_CHEST);
					object_level = original_object_level;
					break;
				}

				/* A skeleton */
				case 'S':
				{
					object_type *i_ptr;
					object_type object_type_body;
					s16b k_idx;

					// make a skeleton 1/2 of the time
					if (one_in_(2))
					{
						/* Get local object */
						i_ptr = &object_type_body;
						
						/* Wipe the object */
						object_wipe(i_ptr);
						
						if (one_in_(3)) k_idx = lookup_kind(TV_USELESS, SV_USELESS_HUMAN_SKEL);
						else			k_idx = lookup_kind(TV_USELESS, SV_USELESS_ELF_SKEL);
												
						/* Prepare the item */
						object_prep(i_ptr, k_idx);
						
						/* Drop it in the dungeon */
						drop_near(i_ptr, -1, y, x);
					}
					break;
				}
					
				/* Monster and/or object from 1 level deeper */
				case '?':
				{
					int r = dieroll(3);
					
					if (r <= 2)
					{
						monster_level = p_ptr->depth + 1;
						place_monster(y, x, TRUE, TRUE, TRUE);
						monster_level = original_monster_level;
					}
					if (r >= 2)
					{
						object_level = p_ptr->depth + 1;
						place_object(y, x, FALSE, FALSE, DROP_TYPE_UNTHEMED);
						object_level = original_object_level;
					}
					break;
				}


				/* Carcharoth */
				case 'C':
				{
					place_monster_one(y, x, R_IDX_CARCHAROTH, TRUE, TRUE, NULL);
					break;
				}
				
				/* silent watcher */
				case 'H':
				{
					place_monster_one(y, x, R_IDX_SILENT_WATCHER, TRUE, FALSE, NULL);
					break;
				}

				/* easterling spy */
				case '@':
				{
					place_monster_one(y, x, R_IDX_EASTERLING_SPY, TRUE, FALSE, NULL);
					break;
				}
					
				/* orc champion */
				case 'o':
				{
					place_monster_one(y, x, R_IDX_ORC_CHAMPION, TRUE, FALSE, NULL);
					break;
				}

				/* orc captain */
				case 'O':
				{
					place_monster_one(y, x, R_IDX_ORC_CAPTAIN, TRUE, FALSE, NULL);
					break;
				}

				/* cat warrior */
				case 'f':
				{
					place_monster_one(y, x, R_IDX_CAT_WARRIOR, TRUE, FALSE, NULL);
					break;
				}

				/* cat assassin */
				case 'F':
				{
					place_monster_one(y, x, R_IDX_CAT_ASSASSIN, TRUE, FALSE, NULL);
					break;
				}
					
				/* troll guard */
				case 'T':
				{
					place_monster_one(y, x, R_IDX_TROLL_GUARD, TRUE, FALSE, NULL);
					break;
				}

				/* barrow wight */
				case 'W':
				{
					place_monster_one(y, x, R_IDX_BARROW_WIGHT, TRUE, TRUE, NULL);
					break;
				}
				
				/* dragon */
				case 'd':
				{
					place_monster_by_flag(y, x, 3, RF3_DRAGON, TRUE, p_ptr->depth + 4);
					break;
				}

				/* young cold drake */
				case 'y':
				{
					place_monster_one(y, x, R_IDX_YOUNG_COLD_DRAKE, TRUE, FALSE, NULL);
					break;
				}
					
				/* young fire drake */
				case 'Y':
				{
					place_monster_one(y, x, R_IDX_YOUNG_FIRE_DRAKE, TRUE, FALSE, NULL);
					break;
				}
					
				/* Spider */
				case 'M':
				{
					place_monster_by_flag(y, x, 3, RF3_SPIDER, TRUE, p_ptr->depth + rand_range(1,4));
					break;
				}
				
				/* Vampire */
				case 'v':
				{
					place_monster_by_letter(y, x, 'v', TRUE, p_ptr->depth + rand_range(1,4));
					break;
				}

                /* Archer */
				case 'a':
				{
					place_monster_by_flag(y, x, 4, (RF4_ARROW1 | RF4_ARROW2), TRUE, p_ptr->depth + 1);
					break;
				}

                /* Flier */
				case 'b':
				{
					place_monster_by_flag(y, x, 2, (RF2_FLYING), TRUE, p_ptr->depth + 1);
					break;
				}

				/* Wolf */
				case 'c':
				{
					place_monster_by_flag(y, x, 3, RF3_WOLF, TRUE, p_ptr->depth + rand_range(1,4));
					break;
				}
					
				/* Rauko */
				case 'r':
				{
					place_monster_by_flag(y, x, 3, RF3_RAUKO, TRUE, p_ptr->depth + rand_range(1,4));
					break;
				}
					
                /* Aldor */
				case 'A':
				{
					place_monster_one(y, x, R_IDX_ALDOR, TRUE, TRUE, NULL);
					break;
				}
                    
				/* Glaurung */
				case 'D':
				{
					place_monster_one(y, x, R_IDX_GLAURUNG, TRUE, TRUE, NULL);
					break;
				}

				/* Gothmog */
				case 'R':
				{
					place_monster_one(y, x, R_IDX_GOTHMOG, TRUE, TRUE, NULL);
					break;
				}
					
				/* Ungoliant */
				case 'U':
				{
					place_monster_one(y, x, R_IDX_UNGOLIANT, TRUE, TRUE, NULL);
					break;
				}

				/* Gorthaur */
				case 'G':
				{
					place_monster_one(y, x, R_IDX_GORTHAUR, TRUE, TRUE, NULL);
					break;
				}
					
				/* Morgoth */
				case 'V':
				{
					place_monster_one(y, x, R_IDX_MORGOTH, TRUE, TRUE, NULL);
					break;
				}
			}

		}
	}
	
	/* Place dungeon features and objects */
	for (t = data, dy = 0; dy < ymax; dy++)
	{
		if (flip_v) ay = ymax - 1 - dy;
		else ay = dy;
        
		for (dx = 0; dx < xmax; dx++, t++)
		{
			if (flip_h) ax = xmax - 1 - dx;
			else ax = dx;
            
			/* Extract the location */
			x = x0 - (xmax / 2) + ax;
			y = y0 - (ymax / 2) + ay;
            
            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }
            
			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;
            
            // some vaults are always lit
            if (v_ptr->flags & (VLT_LIGHT))
            {
                cave_info[y][x] |= (CAVE_GLOW);
            }
            
            // traps are usually 5 times as likely in vaults, but are 10 times as likely if the TRAPS flag is set
            multiplier = (v_ptr->flags & (VLT_TRAPS)) ? 10 : 5;
            
            // another chance to place traps, with 4 times the normal chance
            // so traps in interesting rooms and vaults are a total of 5 times more likely
            if (dieroll(1000) <= trap_placement_chance(y, x) * (multiplier-1))
            {
                place_trap(y,x);
            }
            
            // webbed vaults also have a large chance of receiving webs
            else if ((v_ptr->flags & (VLT_WEBS)) && cave_naked_bold(y,x) && one_in_(20))
            {
                /* Place a web trap */
                cave_set_feat(y, x, FEAT_TRAP_WEB);
                
                // Hide it half the time
                if (one_in_(2))
                {
                    cave_info[y][x] |= (CAVE_HIDDEN);
                }
            }
        }
    }

	return (TRUE);
}


/*
 * Type 6 -- least vaults (see "vault.txt")
 */
static bool build_type6(int y0, int x0)
{
	vault_type *v_ptr;
	int tries = 0;
	int y1, x1, y2, x2;
    bool flip_d;

	/* Pick an interesting room */
	while (TRUE)
	{
		tries++;

		/* Get a random vault record */
		v_ptr = &v_info[rand_int(z_info->v_max)];
		
		// if forcing a forge, then skip vaults without forges in them
		if (p_ptr->force_forge && !v_ptr->forge) continue;
        
        // unless forcing a forge, try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST)) && !p_ptr->force_forge) continue;

        /* Accept the first interesting room */
		if ((v_ptr->typ == 6) && (v_ptr->depth <= p_ptr->depth) && (one_in_(v_ptr->rarity))) break;
		
		if (tries > 20000)
		{
			if (!DEPLOYMENT || cheat_room) msg_format("Bug: Could not find a record for an Interesting Room in vault.txt");
			return (FALSE);
		}
	}
    
    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);
    
    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION)) flip_d = FALSE;

    if (flip_d)
    {
        /* determine the coordinates with hight/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }
    
    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

	/* make sure that the location is within the map bounds */
	if ((y1 <= 3) || (x1 <=3) || (y2 >= p_ptr->cur_map_hgt-3) || (x2 >= p_ptr->cur_map_wid-3))
	{
		return (FALSE);
	}
	/* make sure that the location is empty */
	if (!solid_rock(y1 - 2, x1 - 2, y2 + 2, x2 + 2))
	{
		return (FALSE);
	}

	/* Try building the vault */
	if (!build_vault(y0, x0, v_ptr, flip_d))
	{
		return (FALSE);
	}

	/* save the corner locations */
	dun->corner[dun->cent_n].y1 = y1 + 1;
	dun->corner[dun->cent_n].x1 = x1 + 1;
	dun->corner[dun->cent_n].y2 = y2 - 1;
	dun->corner[dun->cent_n].x2 = x2 - 1;
	
	/* Save the room location */
	dun->cent[dun->cent_n].y = y0;
	dun->cent[dun->cent_n].x = x0;
	dun->cent_n++;

	/* Message */
	//if (cheat_room) msg_format("IR (%s).", v_name + v_ptr->name);

	/* Cause a special feeling */
	good_item_flag = TRUE;

	return (TRUE);
}

/*
 * Type 7 -- lesser vaults (see "vault.txt")
 */
static bool build_type7(int y0, int x0)
{
	vault_type *v_ptr;
	int tries = 0;
	int y1, x1, y2, x2;
    bool flip_d;

	/* Pick a lesser vault */
	while (TRUE)
	{
		tries++;

		/* Get a random vault record */
		v_ptr = &v_info[rand_int(z_info->v_max)];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST))) continue;
        
		/* Accept the first lesser vault */
		if ((v_ptr->typ == 7) && (v_ptr->depth <= p_ptr->depth) && (one_in_(v_ptr->rarity))) break;
		
		if (tries > 2000)
		{
			msg_format("Bug: Could not find a record for a Lesser Vault in vault.txt");
			return (FALSE);
		}
	}

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);
    
    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION)) flip_d = FALSE;
    
    if (flip_d)
    {
        /* determine the coordinates with hight/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }
    
    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

	/* make sure that the location is within the map bounds */
	if ((y1 <= 3) || (x1 <=3) || (y2 >= p_ptr->cur_map_hgt-3) || (x2 >= p_ptr->cur_map_wid-3))
	{
		return (FALSE);
	}
	/* make sure that the location is empty */
	if (!solid_rock(y1 - 2, x1 - 2, y2 + 2, x2 + 2))
	{
		return (FALSE);
	}

	/* Try building the vault */
	if (!build_vault(y0, x0, v_ptr, flip_d))
	{
		return (FALSE);
	}
	
	/* save the corner locations */
	dun->corner[dun->cent_n].y1 = y1 + 1;
	dun->corner[dun->cent_n].x1 = x1 + 1;
	dun->corner[dun->cent_n].y2 = y2 - 1;
	dun->corner[dun->cent_n].x2 = x2 - 1;
	
	/* Save the room location */
	dun->cent[dun->cent_n].y = y0;
	dun->cent[dun->cent_n].x = x0;
	dun->cent_n++;

	/* Message */
	if (cheat_room) msg_format("LV (%s).", v_name + v_ptr->name);

	/* Cause a special feeling */
	good_item_flag = TRUE;
	
	return (TRUE);
}


/*
 * Mark greater vault grids with the CAVE_G_VAULT flag.
 * Returns TRUE if it succeds.
 */
static bool mark_g_vault(int y0, int x0, int ymax, int xmax)
{
  	int y1, x1, y2, x2, y, x;

  	/* Get the coordinates */
  	y1 = y0 - ymax / 2;
  	x1 = x0 - xmax / 2;
  	y2 = y1 + ymax - 1;
  	x2 = x1 + xmax - 1;

  	/* Step 1 - Mark all grids inside that perimeter with the new flag */
  	for (y = y1 + 1; y < y2; y++)
  	{
    	for (x = x1 + 1; x < x2; x++)
    	{
      		cave_info[y][x] |= (CAVE_G_VAULT);
    	}
  	}

  	return (TRUE);
}


/*
 * Type 8 -- greater vaults (see "vault.txt")
 */
static bool build_type8(int y0, int x0)
{
	vault_type *v_ptr;
	int tries = 0;
	bool found = FALSE;
	bool repeated = FALSE;
	int i;
	int y1, x1, y2, x2;
	s16b v_idx;
    bool flip_d;

	// Can only have one greater vault per level
	if (g_vault_name[0] != '\0')
	{
		return (FALSE);
	}

	/* Pick a greater vault */
	while (!found)
	{
		tries++;

		/* Get a random vault record */
		v_idx = rand_int(z_info->v_max);
		v_ptr = &v_info[v_idx];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST))) continue;
        
		/* Accept the first greater vault */
		if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth) && (one_in_(v_ptr->rarity))) 
		{
			repeated = FALSE;
			for (i = 0; i < MAX_GREATER_VAULTS; i++)
			{
				if (v_idx == p_ptr->greater_vaults[i])
				{
					repeated = TRUE;
				}
			}
			
			if (!repeated) found = TRUE;
		}
					
		if (tries > 2000)
		{
			//if (!repeated) msg_debug("Bug: Could not find a record for a Greater Vault in vault.txt");
			return (FALSE);
		}
	}

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);
    
    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION)) flip_d = FALSE;
    
    if (flip_d)
    {
        /* determine the coordinates with hight/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }
    
    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

	/* make sure that the location is within the map bounds */
	if ((y1 <= 3) || (x1 <=3) || (y2 >= p_ptr->cur_map_hgt-3) || (x2 >= p_ptr->cur_map_wid-3))
	{
		return (FALSE);
	}
	/* make sure that the location is empty */
	if (!solid_rock(y1 - 5, x1 - 5, y2 + 5, x2 + 5))
	{
		return (FALSE);
	}

	/* Try building the vault */
	if (!build_vault(y0, x0, v_ptr, flip_d))
	{
		return (FALSE);
	}
	
	/* save the corner locations */
	dun->corner[dun->cent_n].y1 = y1 + 1;
	dun->corner[dun->cent_n].x1 = x1 + 1;
	dun->corner[dun->cent_n].y2 = y2 - 1;
	dun->corner[dun->cent_n].x2 = x2 - 1;
	
	/* Save the room location */
	dun->cent[dun->cent_n].y = y0;
	dun->cent[dun->cent_n].x = x0;
	dun->cent_n++;

	// Remember this greater vault
	for (i = 0; i < MAX_GREATER_VAULTS; i++)
	{
		if (p_ptr->greater_vaults[i] == 0)
		{
			p_ptr->greater_vaults[i] = v_idx;
			break;
		}
	}

	/* Message */
	if (cheat_room) msg_format("GV (%s).", v_name + v_ptr->name);

	/* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
	if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
	{
		my_strcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
	}
	return (TRUE);
}


/*
 * Type 9 -- Morgoth's vault (see "vault.txt")
 */
static bool build_type9(int y0, int x0)
{
	vault_type *v_ptr;
	int y1, x1, y2, x2;
	int tries = 0;

	/* Pick a version of Morgoth's vault */
	while (TRUE)
	{
		/* Get a random vault record */
		v_ptr = &v_info[rand_int(z_info->v_max)];

		/* Accept the first morgoth vault */
		if (v_ptr->typ == 9) break;
		
		tries++;
		if (tries > 10000)
		{
			msg_format("Could not find a record for Morgoth's Vault in vault.txt");
			return (FALSE);
		}
	}

	/* determine the coordinates */
	y1 = y0 - (v_ptr->hgt / 2);
	x1 = x0 - (v_ptr->wid / 2);
	y2 = y1 + v_ptr->hgt - 1;
	x2 = x1 + v_ptr->wid - 1;

	/* Try building the vault */
	if (!build_vault(y0, x0, v_ptr, FALSE))
	{
		return (FALSE);
	}
	
	/* Cause a special feeling */
	good_item_flag = TRUE;
	
	/* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
	if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
	{
		my_strcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
	}

	return (TRUE);
}

/*
 * Type 10 -- The Gates of Angband (see "vault.txt")
 */
static bool build_type10(int y0, int x0)
{
	vault_type *v_ptr;
	int y1, x1, y2, x2;

	/* Get the first vault record */
	v_ptr = &v_info[1];

	/* determine the coordinates */
	y1 = y0 - (v_ptr->hgt / 2);
	x1 = x0 - (v_ptr->wid / 2);
	y2 = y1 + v_ptr->hgt - 1;
	x2 = x1 + v_ptr->wid - 1;

	/* Try building the vault */
	if (!build_vault(y0, x0, v_ptr, FALSE))
	{
		return (FALSE);
	}
	
	/* Cause a special feeling */
	good_item_flag = TRUE;
	
	/* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
	if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
	{
		my_strcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
	}

	return (TRUE);
}


/*
 * Attempt to build a room of the given type at the given co-ordinates
 */
static bool room_build(int typ)
{
	int y, x;

	if (dun->cent_n >= CENT_MAX)
	{
		return (FALSE);
	}

	y = rand_range(5, p_ptr->cur_map_hgt - 5);
	x = rand_range(5, p_ptr->cur_map_wid - 5);

	/* Build a room */
	switch (typ)
	{
		/* Build an appropriate room */
		// Greater Vault
		case 8: build_type8(y, x); break;
		// Lesser Vault
		case 7: build_type7(y, x); break;
		// Least Vault
		case 6:
		{
			if (!build_type6(y, x))
			{
				return (FALSE);
			}
			break;
		}
		// Cross Room
		case 2:
		{
			if (!build_type2(y, x))
			{
				return (FALSE);
			}
			break;
		}
		// Normal Room
		case 1: 
		{
			if (!build_type1(y, x))
			{
				return (FALSE);
			}
			break;
		}
		/* Paranoia */
		default: return (FALSE);
	}

	/* Success */
	return (TRUE);
}


static void set_perm_boundry(void)
{
	int y, x;

	/* Special boundary walls -- Top */
	for (x = 0; x < p_ptr->cur_map_wid; x++)
	{
		y = 0;

		/* Clear previous contents, add perma-wall */
		cave_set_feat(y, x, FEAT_WALL_PERM);
	}

	/* Special boundary walls -- Bottom */
	for (x = 0; x < p_ptr->cur_map_wid; x++)
	{
		y = p_ptr->cur_map_hgt-1;

		/* Clear previous contents, add perma-wall */
		cave_set_feat(y, x, FEAT_WALL_PERM);
	}

	/* Special boundary walls -- Left */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		x = 0;

		/* Clear previous contents, add perma-wall */
		cave_set_feat(y, x, FEAT_WALL_PERM);
	}

	/* Special boundary walls -- Right */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		x = p_ptr->cur_map_wid-1;

		/* Clear previous contents, add perma-wall */
		cave_set_feat(y, x, FEAT_WALL_PERM);
	}
}


/* Start new level with a map entirely of basic granite */
static void basic_granite(void)
{
	int y, x;

	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			/* Create granite wall */
			cave_set_feat(y, x, FEAT_WALL_EXTRA);
			
			// initialise the corridor id array
			cave_corridor1[y][x] = -1;
			cave_corridor2[y][x] = -1;
		}
	}
}

/*
 * Generate a new dungeon level
 *
 * Note that "dun_body" adds about 4000 bytes of memory to the stack.
 */
static bool cave_gen(void)
{
	int i;
	
	int l;
	
	int y, x;
	
	int r;

	int room_attempts = 0;

	/* Hack - variables for allocations */
	s16b mon_gen, obj_room_gen;

	dun_data dun_body;

	/* Global data */
	dun = &dun_body;

	/* Sil - determine the dungeon size */
	/* note: Panel height and width is 1/6 of max height/width*/
	
	// between 3x3 and 5x5
	l = 3 + ((p_ptr->depth + dieroll(5)) / 10);

	p_ptr->cur_map_hgt = l * (PANEL_HGT);
	p_ptr->cur_map_wid = l * (PANEL_WID_FIXED);

	room_attempts = l * l * l * l;

	/*start with basic granite*/
	basic_granite();

	/* Initialize the connection table */
	for (y = 0; y < DUN_ROOMS; y++)
	{
		for (x = 0; x < DUN_ROOMS; x++)
		{
			dun->connection[y][x] = FALSE;
		}
	}

	/* No rooms yet */
	dun->cent_n = 0;

	// guarantee a forge if one hasn't been generated in a while
	//if (cheat_room) msg_format("Forge Drought = %d.", p_ptr->forge_drought);
	if ((p_ptr->forge_drought >= rand_range(2000, 5000)) && (playerturn > 0))
	{
		if (cheat_room) msg_format("Trying to force a forge:");
		p_ptr->force_forge = TRUE;
		
		if (!room_build(6))
		{
			p_ptr->force_forge = FALSE;

			if (cheat_room) msg_format("failed.");

			return (FALSE);
		}

		if (cheat_room) msg_format("succeeded.");
		p_ptr->force_forge = FALSE;
	}
	
	/* Build some rooms */
	for (i = 0; i < room_attempts; i++)
	{
		r = dieroll(p_ptr->depth + 5);
        
        if (one_in_(5)) r += dieroll(5);
		
		// choose a room type based on the level
		if ((r < 5) || one_in_(2) || (p_ptr->depth == 1))
		{
			// standard room
			room_build(1);
		}
		else if ((r < 8))
		{
			// cross room
			room_build(2);
		}
		else if ((r < 13) || one_in_(2))
		{
			// interesting room
			room_build(6);
		}
		else if (r < 18)
		{
			// lesser vault
			room_build(7);
		}
		else
		{
			// greater vault
			room_build(8);
		}
		
		// stop if there are too many rooms
		if (dun->cent_n == DUN_ROOMS - 1)	break;
	}

	/*set the permanent walls*/
	set_perm_boundry();

	/*start over on all levels with less than two rooms due to inevitable crash*/
	if (dun->cent_n < ROOM_MIN)
	{
		if (cheat_room) msg_format("Not enough rooms.");
		return (FALSE);
	}

	/* make the tunnels */
	/* Sil - This has been changed considerably */
	if (!connect_rooms_stairs())
	{
		if (cheat_room) msg_format("Couldn't connect the rooms.");
		return (FALSE);
	}
	
	/* randomise the doors (except those in vaults) */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			if ((cave_feat[y][x] == FEAT_DOOR_HEAD) && !(cave_info[y][x] & (CAVE_ICKY)))
			{
				if(one_in_(4))
					cave_set_feat(y, x, FEAT_FLOOR);			
				else
					place_random_door(y, x);
			}
		}
	
	/* place the stairs, traps, rubble, secret doors */
	if (!place_rubble_player())
	{
		if (cheat_room) msg_format("Couldn't place, rubble, or player.");
		return (FALSE);
	}

	// check dungeon connectivity
	if (!check_connectivity())
	{
		if (cheat_room) msg_format("Failed connectivity.");
		return (FALSE);
	}
	
	if (p_ptr->depth == 1)
	{
		// smaller number of monsters at 50ft
		mon_gen = dun->cent_n / 2;
	}
	else
	{
		// pick some number of monsters (between 0.5 per room and 1 per room)
		mon_gen = (dun->cent_n + dieroll(dun->cent_n)) / 2;
	}

	/* Put some objects in rooms */
	obj_room_gen = 3 * mon_gen / 4;
	if (obj_room_gen > 0)
	{
		// currently ignoring the above...
		alloc_object(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, obj_room_gen, FALSE);
	}
	
    // place the traps
    place_traps();
    
	/* Put some monsters in the dungeon */
	for (i = mon_gen; i > 0; i--)
	{
		(void)alloc_monster(FALSE, FALSE);
	}
	
	// place Morgoth if on the run
	if (p_ptr->on_the_run && !p_ptr->morgoth_slain)
	{
		/* simple way to place Morgoth */
		for (i = 0; i <= 100; i++)
		{
			y = rand_int(p_ptr->cur_map_hgt);
			x = rand_int(p_ptr->cur_map_wid);
			
			if (cave_naked_bold(y, x) && !los(p_ptr->py, p_ptr->px, y, x) && !(cave_info[y][x] & (CAVE_ICKY)))
			{
				place_monster_one(y, x, R_IDX_MORGOTH, FALSE, TRUE, NULL);
				break;
			}
		}
	}

	// add a curved sword near the player if this is the beginning of the game
	if (playerturn == 0)
	{
		place_item_randomly(TV_SWORD, SV_CURVED_SWORD, TRUE);
	}

	return (TRUE);
}



/*
 * Create the gates to Angband level
 */
static void gates_gen(void)
{
	int y, x;
	int i;
	int py = 0, px = 0;
	bool daytime;

	/* Restrict to single-screen size */
	p_ptr->cur_map_hgt = (3 * PANEL_HGT);
	p_ptr->cur_map_wid = (2 * PANEL_WID_FIXED);

	/* Day time */
	if ((turn % (10L * GATES_DAWN)) < ((10L * GATES_DAWN) / 2))
	{
		/* Day time */
		daytime = TRUE;
	}

	/* Night time */
	else
	{
		/* Night time */
		daytime = FALSE;
	}

	/*start with basic granite*/
	basic_granite();
    
	/*set the permanent walls*/
	set_perm_boundry();
    

	build_type10(17,33);


	/* Find an up staircase */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			if (cave_feat[y][x] == FEAT_MORE)
			{
				py = y;
				px = x;
			}
		}
	}

	if ((py == 0) || (px == 0))
	{
		msg_format("Failed to find a down staircase in the gates level");
	}

	/* Delete any monster on the starting square */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *m_ptr = &mon_list[i];

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Only get the monster on the same square */
		if ((m_ptr->fy != py) || (m_ptr->fx != px)) continue;

		/* Delete the monster */
		delete_monster_idx(i);
	}
	
	/* Place the player */
	player_place(py, px);
	
}



/*
 * Create the level containing Morgoth's throne room
 */
static void throne_gen(void)
{
	int y, x;
	int i;
	int py = 0, px = 0;

	// display the throne poetry
	pause_with_text(throne_poetry, 5, 15);
	
	// set the 'truce' in action
	p_ptr->truce = TRUE;
	
	/* Restrict to single-screen size */
	p_ptr->cur_map_hgt = (3 * PANEL_HGT);
	p_ptr->cur_map_wid = (3 * PANEL_WID_FIXED);

	/*start with basic granite*/
	basic_granite();
    
	/*set the permanent walls*/
	set_perm_boundry();

	build_type9(16,38);

	/* Find an up staircase */
	for (y = 0; y < p_ptr->cur_map_hgt; y++)
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			// Sil-y: assumes the important staircase is at the centre of the level
			if ((cave_feat[y][x] == FEAT_LESS) && (x >= 30) && (x <= 45))
			{
				py = y;
				px = x;
			}
		}
	}

	if ((py == 0) || (px == 0))
	{
		msg_format("Failed to find an up staircase in the throne-room");
	}

	/* Delete any monster on the starting square */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *m_ptr = &mon_list[i];

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Only get the monster on the same square */
		if ((m_ptr->fy != py) || (m_ptr->fx != px)) continue;

		/* Delete the monster */
		delete_monster_idx(i);
	}
	
	/* Place the player */
	player_place(py, px);
}

/*
 * Dungeon generation can set some flags indicating that certain one-off
 * things have happened (artefacts, unique greater vaults, unique forge).
 * But if generation fails, we need to reset these flags.
 *
 * "You can't unring a bell." -- Tom Waits
 */
void unring_a_bell(void)
{
	object_type *o_ptr;
	int y, x, i;
		
	// look through the dungeon objects for artefacts
	for (i = 1; i < o_max; i++)
	{
		/* Get the object */
		o_ptr = &o_list[i];
		
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
		
		if (o_ptr->name1)
		{
			artefact_type *a_ptr = &a_info[o_ptr->name1];
			a_ptr->cur_num = 0;
		}
	}
	
	// Look through the map for the unique forge
	for (y = 0; y < p_ptr->cur_map_hgt; ++y)
	{
		for (x = 0; x < p_ptr->cur_map_wid; ++x)
		{
			// Reset the unique forge
			if ((cave_feat[y][x] >= FEAT_FORGE_UNIQUE_HEAD) && (cave_feat[y][x] <= FEAT_FORGE_UNIQUE_TAIL))
			{
				p_ptr->unique_forge_made = FALSE;
			}
		}
	}
	
	// If there is a greater vault...
	if (g_vault_name[0] != '\0')
	{
		// wipe vault name
		g_vault_name[0] = '\0';
		
		// look for the final greater vault entry
		for (i = 0; i < MAX_GREATER_VAULTS; i++)
		{
			// wipe the final entry
			if (i == MAX_GREATER_VAULTS-1)
			{
				p_ptr->greater_vaults[i] = 0;
				break;
			}
			else if (p_ptr->greater_vaults[i+1] == 0)
			{
				p_ptr->greater_vaults[i] = 0;
				break;
			}
		}
	}
	
}

/*
 * Generate a random dungeon level
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Note that this function resets "cave_feat" and "cave_info" directly.
 */
void generate_cave(void)
{
	int y, x, num, i;

	/* The dungeon is not ready */
	character_dungeon = FALSE;

	/* Don't know feeling yet */
	do_feeling = FALSE;

	/*allow uniques to be generated everywhere but in nests/pits*/
	allow_uniques = TRUE;

	// display the entry poetry
	if (playerturn == 0)
	{
		if (p_ptr->psex == SEX_FEMALE)	pause_with_text(female_entry_poetry, 5, 15);
		else							pause_with_text(male_entry_poetry, 5, 15);
	}
	
	// reset smithing leftover (as there is no access to the old forge)
	p_ptr->smithing_leftover = 0;
    
    // reset the forced skipping of next turn (a bit rough to miss first turn if you fell down)
    p_ptr->skip_next_turn = FALSE;

	/* Generate num is increased below*/
	for (num = 0; TRUE;)
	{
		bool okay = TRUE;

		cptr why = NULL;

		/* Reset */
		o_max = 1;
		mon_max = 1;
		feeling = 0;

		/* Start with a blank cave */
		for (y = 0; y < MAX_DUNGEON_HGT; y++)
		{
			for (x = 0; x < MAX_DUNGEON_WID; x++)
			{
				/* No flags */
				cave_info[y][x] = 0;

				/* No features */
				cave_feat[y][x] = 0;

				/* No objects */
				cave_o_idx[y][x] = 0;

				/* No monsters */
				cave_m_idx[y][x] = 0;

				for (i = 0; i < MAX_FLOWS; i++)
				{
					cave_cost[i][y][x] = FLOW_MAX_DIST;
				}

				cave_when[y][x] = 0;

			}
		}
		
		// reset the wandering monster pauses
		for(i = 0; i < MAX_FLOWS; i++)
		{
			wandering_pause[i] = 0;
		}

		/* Mega-Hack -- no player yet */
		p_ptr->px = p_ptr->py = 0;

		/* Hack -- illegal panel */
		p_ptr->wy = MAX_DUNGEON_HGT;
		p_ptr->wx = MAX_DUNGEON_WID;

		/* Reset the monster generation level */
		monster_level = p_ptr->depth;

		/* Reset the object generation level */
		object_level = p_ptr->depth;
		
		/* Nothing special here yet */
		good_item_flag = FALSE;

		/* Nothing good here yet */
		rating = 0;

		/* Build the gates to Angband */
		if (!p_ptr->depth)
		{
			gates_gen();

			/* Hack -- Clear stairs request */
			p_ptr->create_stair = 0;
		}

		/* Build Morgoth's throne room */
		else if (p_ptr->depth == MORGOTH_DEPTH)
		{
			throne_gen();

			/* Hack -- Clear stairs request */
			p_ptr->create_stair = 0;
		}

		/* Build a real level */
		else
		{
			/* Make a dungeon, or report the failure to make one*/
			if (cave_gen()) okay = TRUE;
			else okay = FALSE;

		}

		/*message*/
		if(!okay)
		{
	   		if (cheat_room || cheat_hear || cheat_peek || cheat_xtra)
					why = "defective level";
			
			// Must reset all the artefacts that were generated on the defective level
			for (i = 1; i < o_max; i++)
			{
				/* Get the object */
				object_type *o_ptr = &o_list[i];

				/* Skip dead objects */
				if (!o_ptr->k_idx) continue;

				/* If artefact. */
				if (o_ptr->name1)
				{
					/* Reset its count */
					a_info[o_ptr->name1].cur_num = 0;
					a_info[o_ptr->name1].found_num = 0;
				}

			}
			
		}

		else
		{

			/* Extract the feeling */
			if (!feeling)
			{
				if (rating > 100) feeling = 2;
				else if (rating > 80) feeling = 3;
				else if (rating > 60) feeling = 4;
				else if (rating > 40) feeling = 5;
				else if (rating > 30) feeling = 6;
				else if (rating > 20) feeling = 7;
				else if (rating > 10) feeling = 8;
				else if (rating > 0) feeling = 9;
				else feeling = 10;

				/* Hack -- Have a special feeling sometimes */
				if (good_item_flag && !(PRESERVE_MODE)) feeling = 1;

				/* Hack -- no feeling at the gates */
				if (!p_ptr->depth) feeling = 0;
			}

			/* Prevent object over-flow */
			if (o_max >= z_info->o_max)
			{
				/* Message */
				why = "too many objects";

				/* Message */
				okay = FALSE;
			}

			/* Prevent monster over-flow */
			if (mon_max >= MAX_MONSTERS)
			{
				/* Message */
				why = "too many monsters";

				/* Message */
				okay = FALSE;
			}

		}

		/* Accept */
		if (okay) break;

		/* Message */
		if (why) msg_format("Generation failed (%s)", why);

		// Undo unique things!
		unring_a_bell();

		/* Wipe the objects */
		wipe_o_list();

		/* Wipe the monsters */
		wipe_mon_list();
	}

	/* The dungeon is ready */
	character_dungeon = TRUE;
        
	/* Reset the number of traps on the level. */
	num_trap_on_level = 0;

	/* Note any forges generated -- have to do this here in case generation fails earlier */
	for (y = 0; y < MAX_DUNGEON_HGT; y++)
	{
		for (x = 0; x < MAX_DUNGEON_WID; x++)
		{
			if (cave_forge_bold(y,x))
			{
				// reset the time since the last forge when an interesting room with a forge is generated
				p_ptr->forge_drought = 0;
				p_ptr->forge_count++;
			}
		}
	}
	

}




