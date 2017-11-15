/* File: automaton.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * This file includes code for automated play of Sil.
 *
 * The idea and some of the code from five of the low-level utility functions came from
 * the Angband Borg by Ben Harrison (with modifications by Dr Andrew White).
 *
 * The borrowed functions are: automaton_keypress, automaton_keypresses, automaton_inkey, automaton_flush, automaton_inkey_hack
 *
 * Inspiration to actually start writing an AI for Sil came from Brian Walker's article:
 * 'The Incredible Power of Djikstra Maps'.
 *
 * The Angband Borg was written with the idea that it would interact with the game as if it were a player
 * at a terminal. Thus, it would have to scan the screen to get information about what was going on and
 * then input characters to control the game, making it the full AI experience with no assistance from the game.
 * When it got assistance on certain occasions, this was seen as a crutch and a 'cheat' of sorts.
 *
 * I don't intend to be that masochistic. While it would be nice, the basic parts of the AI that are needed
 * (a *lot* of screen-scraping), are not very interesting. I'm more concerned with the AI needed to play Sil well
 * as opposed to working out what the letters on the screen mean. I'm thus prepared to let the AI have access to
 * internal variables when any sensible player would know their content, and even to modify the main program a bit
 * to add flags which make it easier for the AI to know what is going on. 
 *
 * However, I shall try to follow a rule that it shouldn't have access
 * to info that the player doesn't have (such as what items do before identified) and it shouldn't be able to perform
 * actions that the player can't. At some points it might break these
 * rules, but I'll flag that in the code as 'cheating' and try to remove it later.
 *
 *
 * The basic approach is that when someone activates the automaton, it replaces the standard function that gathers
 * user keypresses. Thus, whenever the program awaits input, the automaton can supply it. The automaton works out
 * what keypresses to send, and queues one or more of them up in the 'automaton_key_queue'. These are then
 * consumed by the standard game, causing it to run around etc. Real user keypresses abort the automaton and
 * return control to the user.
 *
 * There is a small amount of automaton-related code in other files, but the aim is to have almost all of it here.
 */

int skill_vals[S_MAX] = {100, 50, 100, 30, 50, 50, 0, 0}; // combat values
//int skill_vals[S_MAX] = {30, 0, 30, 100, 50, 50, 0, 0}; // stealth values

int light_val = 150;

#define KEY_SIZE 8192


/*
 * An array storing the automaton's additional internal info about the dungeon layout
 *
 * Currently this is just used as a boolean array for storing whether the contents of the
 * square are known (necessary because unlit floor squares can't get set with CAVE_MARK).
 *
 * It could be expanded so that each square has multiple pieces of information.
 * This could be done with bitflags (like cave_info), or even better might be an map_square
 * struct that can store numbers, indices for monsters etc. If it is too hard to get
 * a 2D array of structs to work (I couldn't do it...) then multiple 2D arrays might be best.
 */
byte (*automaton_map)[MAX_DUNGEON_WID];


/*
 * A Queue of keypresses to be sent
 */
static char *automaton_key_queue;
static s16b automaton_key_head;
static s16b automaton_key_tail;

/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * Add a keypress to the "queue" (fake event)
 */
errr automaton_keypress(char k)
{
    /* Hack -- Refuse to enqueue "nul" */
    if (!k) return (-1);
    
    /* Store the char, advance the queue */
    automaton_key_queue[automaton_key_head++] = k;
    
    /* Circular queue, handle wrap */
    if (automaton_key_head == KEY_SIZE) automaton_key_head = 0;
    
    /* Hack -- Catch overflow (forget oldest) */
    if (automaton_key_head == automaton_key_tail) return (-1);
    
    /* Hack -- Overflow may induce circular queue */
    if (automaton_key_tail == KEY_SIZE) automaton_key_tail = 0;
    
    /* Success */
    return (0);
}

/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * Add a keypress to the "queue" (fake event)
 */
errr automaton_keypresses(char *str)
{
    char *s;
    
    /* Enqueue them */
    for (s = str; *s; s++) automaton_keypress(*s);
    
    /* Success */
    return (0);
}


/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * Get the next automaton keypress
 */
char automaton_inkey(bool take)
{
    int i;
    
    /* Nothing ready */
    if (automaton_key_head == automaton_key_tail) return (0);
    
    /* Extract the keypress */
    i = automaton_key_queue[automaton_key_tail];
    
    /* Do not advance */
    if (!take) return (i);
    
    /* Advance the queue */
    automaton_key_tail++;
    
    /* Circular queue requires wrap-around */
    if (automaton_key_tail == KEY_SIZE) automaton_key_tail = 0;
    
    /* Return the key */
    return (i);
}

/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * Get the next automaton keypress
 */
void automaton_flush(void)
{
    /* Simply forget old keys */
    automaton_key_tail = automaton_key_head;
}

/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * Mega-Hack -- special "inkey_hack" hook.  XXX XXX XXX
 *
 * A special function hook (see "util.c") which allows the automaton to take
 * control of the "inkey()" function, and substitute in fake keypresses.
 */
extern char (*inkey_hack)(int flush_first);


/*
 * Stop the automaton.
 */
void stop_automaton(void)
{
    // set the flag to show the automaton is off
    p_ptr->automaton = FALSE;

    /* Remove hook */
    inkey_hack = NULL;
    
    /* Flush keys */
    automaton_flush();
    
    // free the "keypress queue"
    FREE(automaton_key_queue);
}


/*
 * Updates an array the size of the map with information about how long the automaton
 * thinks it will take the player to get to the given centre square from any map square.
 * 
 * The code is heavily based on update_flow() from cave.c, so see there for full comments.
 *
 * This is separated in an attempt to keep as much automaton stuff as possible out of the
 * main game files (and in the hope that the automaton flow code can be tailored in future).
 */
void update_automaton_flow(int cy, int cx)
{
    int which_flow = FLOW_AUTOMATON;
    
	int cost;
    
	int i, d, d2;
	byte y, x, y2, x2, y3, x3;
	int last_index;
	int grid_count = 0;
    
	/* Note where we get information from, and where we overwrite */
	int this_cycle = 0;
	int next_cycle = 1;
    
	byte flow_table[2][2][8 * FLOW_MAX_DIST];
    
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
					
                    // skip unknown grids
                    if (!((cave_info[y2][x2] & (CAVE_MARK)) || automaton_map[y2][x2])) continue;
                    
                    // skip walls
                    if (cave_wall_bold(y2, x2)) continue;
                    
                    // skip chasms
                    if (cave_feat[y2][x2] == FEAT_CHASM) continue;
                    
                    // penalise traps
                    if (cave_trap_bold(y2, x2) && !(cave_info[y2][x2] & (CAVE_HIDDEN))) extra_cost += 3;
                    
                    // penalise visible molds
                    if (cave_m_idx[y2][x2] > 0)
                    {
                        monster_type *n_ptr = &mon_list[cave_m_idx[y2][x2]];
                        monster_race *q_ptr = &r_info[n_ptr->r_idx];
                        
                        if (n_ptr->ml && q_ptr->flags1 & (RF1_NEVER_MOVE)) extra_cost += 10;
                    }
                    
                    // penalise squares next to visible molds
                    for (d2 = 0; d2 < 8; d2++)
                    {
                        /* Grand-child location */
                        y3 = y2 + ddy_ddd[d2];
                        x3 = x2 + ddx_ddd[d2];
                        
                        if (cave_m_idx[y3][x3] > 0)
                        {
                            monster_type *n_ptr = &mon_list[cave_m_idx[y3][x3]];
                            monster_race *q_ptr = &r_info[n_ptr->r_idx];
                            
                            if (n_ptr->ml && q_ptr->flags1 & (RF1_NEVER_MOVE)) extra_cost += 2;
                        }
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
 * The automaton keeps an internal map to remind it of various things.
 *
 * This function gets it to remember squares that were seen at some point.
 */
void add_seen_squares_to_map(void)
{
    int y, x;
    
    // look at every unmarked square of the map
    for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_info[y][x] & (CAVE_SEEN))
            {
                automaton_map[y][x] = TRUE;
            }
        }
    }
    
    // add own square to map too (it doesn't count as SEEN)
    automaton_map[p_ptr->py][p_ptr->px] = TRUE;
}


bool find_enemy_to_kill(int *ty, int *tx)
{
    int i;
    int dist;
    int best_dist = 4; // target to beat
    
    bool can_fire = FALSE;

    monster_type *m_ptr;
    monster_race *r_ptr;
    
    for (i = 1; i < mon_max; i++)
    {
        m_ptr = &mon_list[i];
        r_ptr = &r_info[m_ptr->r_idx];
        
        // Skip dead monsters
        if (!m_ptr->r_idx) continue;
        
        // Skip unseen monsters
        if (!m_ptr->ml) continue;
        
        // Skip unalert monsters
        if (m_ptr->alertness < ALERTNESS_ALERT) continue;

        // Skip unmoving monsters
        if (r_ptr->flags1 & (RF1_NEVER_MOVE)) continue;
        
        dist = flow_dist(FLOW_AUTOMATON, m_ptr->fy, m_ptr->fx);
        
        if (dist < best_dist)
        {
            best_dist = dist;
            can_fire = (dist > 1) && (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_FIRE));
            
            // wait for melee opponents to come to you
            if ((best_dist == 2) && (r_ptr->freq_ranged == 0))
            {
                *ty = p_ptr->py;
                *tx = p_ptr->px;
            }
            
            // charge at the others
            else
            {
                *ty = m_ptr->fy;
                *tx = m_ptr->fx;
            }
        }
    }
    
    if (can_fire && inventory[INVEN_BOW].tval && ((inventory[INVEN_QUIVER1].number >= 1) || (inventory[INVEN_QUIVER2].number >= 1)))
    {
        // clear the target
        target_set_monster(0);
        
        // queue the commands
        automaton_keypresses("ff");  // bug: occasionally shoots at a mold instead of the mobile target...
        
        return (TRUE);
    }
    
    return (FALSE);
}

int evaluate_weapon(object_type *o_ptr)
{
    int value = 0;
    int ds = strength_modified_ds(o_ptr, 0);
    int max_dam = o_ptr->dd * ds;
    int min_dam = (o_ptr->ds > 0) ? o_ptr->dd : 0;
	u32b f1, f2, f3;
    
    // 'nothings' are worthless
    if (!o_ptr->k_idx) return (0);
    
    // extract the flags for the object
    // need to be careful using these as they could involve hidden information
    object_flags(o_ptr, &f1, &f2, &f3);
    
    // value skill points as per normal
    value += (o_ptr->att * skill_vals[S_MEL]) + (o_ptr->evn * skill_vals[S_EVN]);

    // value each point of average damage as 100
    value += (min_dam + max_dam) * 100 / 2;
    
    // value {special} items as 100 (could easily be improved!)
    if (o_ptr->name1 || o_ptr->name2)   value += 100;

    // value lightness
    value = value * 100 / (70 + o_ptr->weight);
    
    // value one-handedness
    if (f3 & (TR3_TWO_HANDED)) value = value * 8 / 10;
    
    return (value);
}

int evaluate_bow(object_type *o_ptr)
{
    int value = 0;
    int ds = total_ads(o_ptr, FALSE);
    int max_dam = o_ptr->dd * ds;
    int min_dam = (o_ptr->ds > 0) ? o_ptr->dd : 0;
    
    // 'nothings' are worthless
    if (!o_ptr->k_idx) return (0);

    // value skill points as per normal
    value += (o_ptr->att * skill_vals[S_ARC]) + (o_ptr->evn * skill_vals[S_EVN]);
    
    // value each point of average damage as 100
    value += (min_dam + max_dam) * 100 / 2;
    
    // value {special} items as 100 (could easily be improved!)
    if (o_ptr->name1 || o_ptr->name2)   value += 100;
    
    // value lightness
    value = value * 100 / (70 + o_ptr->weight);
    
    return (value);
}

int evaluate_arrow(object_type *o_ptr)
{
    int value = 100 + (o_ptr->att * skill_vals[S_ARC]);
    
    // 'nothings' are worthless
    if (!o_ptr->k_idx) return (0);
    
    // value {special} items as 300 (could easily be improved!)
    if (o_ptr->name2)   value += 300;
    
    return (value);
}

int evaluate_armour(object_type *o_ptr)
{
    int value = 0;
    int max_prt = o_ptr->pd * o_ptr->ps;
    int min_prt = (o_ptr->ps > 0) ? o_ptr->pd : 0;
    
    // 'nothings' are worthless
    if (!o_ptr->k_idx) return (0);
    
    // value skill points as per normal
    value += (o_ptr->att * skill_vals[S_MEL]) + (o_ptr->evn * skill_vals[S_EVN]);

    // value each point of average protection as 100
    value += (min_prt + max_prt) * 100 / 2;
    
    // value {special} items as 100 (could easily be improved!)
    if (o_ptr->name1 || o_ptr->name2)   value += 100;

    // value lightness a tiny bit
    value -= o_ptr->weight / 2;
    
    return (value);
}

int evaluate_light(object_type *o_ptr)
{
    int value = 0;
    
    // 'nothings' are worthless
    if (!o_ptr->k_idx) return (0);
    
    // paranoia
    if (o_ptr->tval != TV_LIGHT) return (0);

    switch (o_ptr->sval)
    {
        case SV_LIGHT_TORCH:
            value = RADIUS_TORCH * light_val;
            value -= (FUEL_TORCH - o_ptr->timeout) * light_val / FUEL_TORCH;
            break;
        case SV_LIGHT_LANTERN:
            value = RADIUS_LANTERN * light_val;
            value -= (FUEL_LAMP - o_ptr->timeout) * light_val / FUEL_LAMP;
            break;
        case SV_LIGHT_LESSER_JEWEL:
            value = RADIUS_LESSER_JEWEL * light_val;
            break;
        case SV_LIGHT_FEANORIAN:
            value = RADIUS_FEANORIAN * light_val;
            break;
        case SV_LIGHT_SILMARIL:
            value = RADIUS_SILMARIL * light_val;
            break;
    }
    
    // value {special} items as 100 (could easily be improved!)
    if (o_ptr->name1 || o_ptr->name2)   value += 100;
    
    return (value);
}

int evaluate_object(object_type *o_ptr)
{
    int slot = wield_slot(o_ptr);
    int value = 0;
    object_type *o_ptr2;

    if (slot >= 0)
    {
        o_ptr2 = &inventory[slot];
        
        switch (slot)
        {
            case INVEN_WIELD:
                value = evaluate_weapon(o_ptr) - evaluate_weapon(o_ptr2);
                break;
            case INVEN_BOW:
                value = evaluate_bow(o_ptr) - evaluate_bow(o_ptr2);
                break;
            //case INVEN_LEFT:
            //case INVEN_RIGHT:
            //case INVEN_NECK:
            case INVEN_LITE:
                value = evaluate_light(o_ptr) - evaluate_light(o_ptr2);
                break;
            case INVEN_BODY:
            case INVEN_OUTER:
            case INVEN_ARM:
            case INVEN_HEAD:
            case INVEN_HANDS:
            case INVEN_FEET:
                value = evaluate_armour(o_ptr) - evaluate_armour(o_ptr2);
                break;
            case INVEN_QUIVER1:
            case INVEN_QUIVER2:
                // hack: the +1 means we will accept ties
                // in the future it is better to do arrows differently
                // by counting those in pack and considering how much we care about archery
                value = evaluate_arrow(o_ptr) - evaluate_arrow(o_ptr2) + 1;
                break;
        }
    }
    else
    {
        value = 0;
    }
    
    return (value);
}

void find_object(int *ty, int *tx)
{
    int i;
    int y, x;
    int dist;
    int best_dist = FLOW_MAX_DIST - 1; // target to beat
    int value;
    
	/* Scan objects */
	for (i = 1; i < o_max; i++)
	{
		object_type *o_ptr = &o_list[i];
        
		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;
        
		/* Skip held objects */
		if (o_ptr->held_m_idx) continue;
        
		/* Location */
		y = o_ptr->iy;
		x = o_ptr->ix;
        
		// skip items whose location is unknown
		if (!o_ptr->marked) continue;

        // skip items in the player's square
		if ((y == p_ptr->py) && (x == p_ptr->px)) continue;

        dist = flow_dist(FLOW_AUTOMATON, y, x);
        
        value = evaluate_object(o_ptr) - (dist / 20);
        
        // don't seek boring items
        if (value <= 0) continue;
        
        if (dist < best_dist)
        {
            best_dist = dist;
            
            *ty = y;
            *tx = x;
        }
	}
    
    if (ty != 0) target_set_location(*ty, *tx);

}

bool pickup_object(void)
{
    object_type *o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    char commands[80];
    int value = evaluate_object(o_ptr);
    
    if (value > 0)
    {
        int slot = wield_slot(o_ptr);
     
        // if it is a non-wieldable item
        if (slot == -1)
        {
            // create the commands
            strnfmt(commands, sizeof(commands), "g-");
            
            // queue the commands
            automaton_keypresses(commands);
        }
        
        // special rules for arrows
        else if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
        {
            // if it is considered equal in value to existing arrows, then just get it to allow auto-merging
            if (value == 1)
            {
                // create the commands
                strnfmt(commands, sizeof(commands), "g-");
                
                // queue the commands
                automaton_keypresses(commands);
            }
            
            // otherwise it is considered better than the existing arrows so wield it...
            else
            {
                // create the commands
                strnfmt(commands, sizeof(commands), "w-");
                
                // queue the commands
                automaton_keypresses(commands);
                
                // if both slots are full, we need to tell the game to replace the inferior one
                if (inventory[INVEN_QUIVER1].k_idx && inventory[INVEN_QUIVER2].k_idx)
                {
                    if (evaluate_arrow(&inventory[INVEN_QUIVER1]) > evaluate_arrow(&inventory[INVEN_QUIVER2]))
                    {
                        automaton_keypress(index_to_label(INVEN_QUIVER2));
                    }
                    else
                    {
                        automaton_keypress(index_to_label(INVEN_QUIVER1));
                    }
                }
            }
        }
        
        // default for wieldable items
        else
        {
            // create the commands
            strnfmt(commands, sizeof(commands), "w-");
            
            // queue the commands
            automaton_keypresses(commands);
        }
        

        return (TRUE);
    }
    
    return (FALSE);
}


bool renew_light(void)
{
    int i;
    bool found = FALSE;
    object_type *o_ptr = &inventory[INVEN_LITE];
    char commands[80];
    
    // if we need a new light source
    if (((o_ptr->sval == SV_LIGHT_TORCH) || (o_ptr->sval == SV_LIGHT_LANTERN)) && (o_ptr->timeout < 100))
    {
        // special case when using a lantern
        if (o_ptr->sval == SV_LIGHT_LANTERN)
        {
            // find a lantern or flask in the pack
            for (i = 0; i < INVEN_PACK; i++)
            {
                o_ptr = &inventory[i];
                
                if ((o_ptr->tval == TV_FLASK) || ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)))
                {
                    found = TRUE;
                    break;
                }
            }
        }
        
        // if we haven't yet found anything
        if (!found)
        {
            // find one or more light sources in the pack
            for (i = 0; i < INVEN_PACK; i++)
            {
                o_ptr = &inventory[i];
                
                if (o_ptr->tval == TV_LIGHT)
                {
                    if (((o_ptr->sval != SV_LIGHT_TORCH) && (o_ptr->sval != SV_LIGHT_LANTERN)) || (o_ptr->timeout > 100))
                    {
                        found = TRUE;
                        break;
                    }
                }
            }
        }
        
        // desperation!
        if (!found && (inventory[INVEN_LITE].timeout == 0))
        {
            // find one or more light sources in the pack
            for (i = 0; i < INVEN_PACK; i++)
            {
                o_ptr = &inventory[i];
                
                if (o_ptr->tval == TV_LIGHT)
                {
                    if (o_ptr->timeout > 0)
                    {
                        found = TRUE;
                        break;
                    }
                }
            }
        }
        
        // use the new light if found
        if (found)
        {
            // create the commands
            strnfmt(commands, sizeof(commands), "u%c", 'a' + i);
            
            // queue the commands
            automaton_keypresses(commands);
            
            return (TRUE);
        }
    }
    
    return (FALSE);
}

void rest(int *ty, int *tx)
{
    if ((p_ptr->chp * 100 / p_ptr->mhp < 75) ||
        (p_ptr->stun) || (p_ptr->confused) || (p_ptr->afraid) || (p_ptr->poisoned) ||
        (p_ptr->blind) || (p_ptr->image) || (p_ptr->slow) || (p_ptr->cut))
    {
        *ty = p_ptr->py;
        *tx = p_ptr->px;
    }
}



void find_unexplored(int *ty, int *tx)
{
    int y, x, yy, xx;
    int i;
    int dist;
    int best_dist = FLOW_MAX_DIST - 1;
    int count = 0;
    
    // first, if you are in a suspicious dead-end corridor, possibly search a bit
    
    // count adjacent walls
    for (i = 7; i >= 0; i--)
    {
        // get the adjacent location
        y = p_ptr->py + ddy_ddd[i];
        x = p_ptr->px + ddx_ddd[i];
        
        if (cave_wall_bold(y,x) && (cave_feat[y][x] != FEAT_RUBBLE)) count++;
    }
    
    // if it looks like there must be a secret door...
    if ((count == 7) && percent_chance(90))
    {
        *ty = p_ptr->py;
        *tx = p_ptr->px;
    }
    
    // if you are just in an everyday location...
    else
    {
        // look at every unmarked square of the map
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        {
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
            {
                if (!((cave_info[y][x] & (CAVE_MARK)) || automaton_map[y][x]))
                {
                    int best_local_dist = FLOW_MAX_DIST - 1;
                    
                    // ignore your own square
                    if ((y == p_ptr->py) && (x == p_ptr->px)) continue;
                    
                    // try all adjacent locations
                    for (i = 7; i >= 0; i--)
                    {
                        // get the adjacent location
                        yy = y + ddy_ddd[i];
                        xx = x + ddx_ddd[i];
                        
                        dist = flow_dist(FLOW_AUTOMATON, yy, xx);
                        
                        // keep track of the best square adjacent to the unmarked location that is reachable from the player
                        if (dist < best_local_dist) best_local_dist = dist;
                    }
                    
                    // keep track of the closest unmarked location
                    // (the second line breaks ties in a way that makes it go around corridor corners properly)
                    if ((best_local_dist < best_dist) ||
                        ((best_local_dist == best_dist) && (best_local_dist == 1) && ((y == p_ptr->py) || (x == p_ptr->px)) && (cave_info[y][x] & (CAVE_VIEW))))

                    {
                        best_dist = best_local_dist;
                        *ty = y;
                        *tx = x;
                    }
                }
            }
        }
    }
    
    if (ty != 0) target_set_location(*ty, *tx);

}

void find_secret_door(int *ty, int *tx)
{
    int y, x, yy, xx;
    int i;
    int dist;
    int best_dist = FLOW_MAX_DIST - 1;
    
    for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_floorlike_bold(y,x))
            {
                int count = 0;
                
                // count adjacent walls
                for (i = 7; i >= 0; i--)
                {
                    // get the adjacent location
                    yy = y + ddy_ddd[i];
                    xx = x + ddx_ddd[i];
                    
                    if (cave_wall_bold(yy,xx) && (cave_feat[yy][xx] != FEAT_RUBBLE)) count++;
                }
                
                // if it looks like there must be a secret door...
                if (count == 7)
                {
                    dist = flow_dist(FLOW_AUTOMATON, y, x);
                    
                    // keep track of the closest likely secret door location
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        *ty = y;
                        *tx = x;
                    }
                }
            }
        }
    }
    
    if (ty != 0) target_set_location(*ty, *tx);
}


bool leave_level(void)
{
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE) || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT))
    {
        automaton_keypress('>');
        return (TRUE);
    }
    
    return (FALSE);
}

void find_level_exit(int *ty, int *tx)
{
    int y, x;
    int dist;
    int best_dist = FLOW_MAX_DIST - 1;
    
    for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            // can't see unmarked things...
            if (!(cave_info[y][x] & (CAVE_MARK))) continue;
            
            if ((cave_feat[y][x] == FEAT_MORE) || (cave_feat[y][x] == FEAT_MORE_SHAFT))
            {
                dist = flow_dist(FLOW_AUTOMATON, y, x);
                
                if (dist < best_dist)
                {
                    best_dist = dist;
                    *ty = y;
                    *tx = x;
                }
            }
        }
    }
    
    if (ty != 0) target_set_location(*ty, *tx);
}

bool allocate_experience(void)
{
    char commands[80];
    int i;
    int val;
    int best_val = 0;
    int best_skill = S_MEL;
    
    // find the most important skill to raise
    for (i = 0; i < S_MAX; i++)
    {
        val = skill_vals[i] * 10 / (p_ptr->skill_base[i] + 1);
        
        if (val > best_val)
        {
            best_val = val;
            best_skill = i;
        }
    }
    
    if (p_ptr->new_exp >= (p_ptr->skill_base[best_skill] + 1) * 100)
    {
        // create the first command (getting to the increase skills screen)
        strnfmt(commands, sizeof(commands), "@i");
        
        // queue it
        automaton_keypresses(commands);

        // move to the appropriate skill
        for (i = 0; i < best_skill; i++)
        {
            automaton_keypress('2');
        }

        // create the second command (increasing the skill and exiting)
        strnfmt(commands, sizeof(commands), "6\r\033");  // \033 is ESCAPE
        
        // queue it
        automaton_keypresses(commands);

        return (TRUE);
    }
    return (FALSE);
}


/*
 * AI to-do list:
 *
 * Exploring
 *  - be less completist with levels (don't walk 50 squares to explore one more room...)
 *  - be willing to use up stairs (or chasms/shafts) sometimes
 *
 * Stealth
 *  - stay away from nearby unalert monsters
 *  - stay near walls
 *
 * Combat
 *  - learn to rest properly (away from archers/molds)
 *  - fight from sensible locations
 *  - deal with 'Afraid' status
 *  - wield new arrows from pack when a quiver is empty
 *
 * Experience
 *  - gain abilities
 *
 * Darkness
 *  - ignores monsters in darkness (e.g. archers & shadow molds)
 *  - often dies while ignoring shadow molds
 *
 * Objects
 *  - can't deal with rings and amulets
 *  - can't deal with non-wieldable items
 *  - never drops items
 *
 * Loops
 *  - pillared rooms and archers which run can cause it to walk back and forth
 *  - stuck searching for a secret door to get to the down stairs
 *
 *
 * In general, I wrote most of the basic routine as an AI with no internal state.
 * I've added state in the form of its internal map and more state should be added
 * (for example so it can work out if it lost health since last turn, or to set its
 * own internal mode).
 */


/*
 * Take an AI controlled turn.
 */
void automaton_turn(void)
{
    int y, x;
    int i;
    int ty = 0;
    int tx = 0;
    int dist;
    int best_dist = FLOW_MAX_DIST - 1; // default to an easy-to-beat value
    int best_dir = 5;                   // default to not moving
    bool found_direction = FALSE;
    
    char base_command = ';';
    char commands[80];
    
    add_seen_squares_to_map();

    // generate a flow map from the player
    update_automaton_flow(p_ptr->py, p_ptr->px);
    
    // allocate experience
    if (ty == 0)    if (allocate_experience())  return;
    
    // otherwise: fight any nearby alert monsters
    if (ty == 0)    if (find_enemy_to_kill(&ty, &tx))   return;
    
    // get item if standing on a good one
    if (ty == 0)    if (pickup_object())  return;
    
    // otherwise: light a torch if needed
    if (ty == 0)    if (renew_light())  return;

    // otherwise: rest if less than 75% health or with negative timed effects
    //if (ty == 0)    rest(&ty, &tx);
    
    // otherwise: find an object worth taking
    if (ty == 0)    find_object(&ty, &tx);
    
    // otherwise: find the closest unexplored location that is next to an explored one
    if (ty == 0)    find_unexplored(&ty, &tx);
    
    // otherwise: go downstairs if standing on them
    if (ty == 0)    if (leave_level())  return;
    
    // otherwise: head for the down stairs
    if (ty == 0)    find_level_exit(&ty, &tx);
    
    // otherwise: find a plausible location for a secret door
    if (ty == 0)    find_secret_door(&ty, &tx);
    
    // exit automaton mode if no known target is found
    if (ty == 0)
    {
        msg_print("Could not find anything to do.");
        stop_automaton();
        return;
    }
    
    // find direction to the target: easy if you are already there!
    if ((ty == p_ptr->py) && (tx == p_ptr->px))
    {
        found_direction = TRUE;
    }
    
    // find direction to target
    else
    {
        // generate a flow map towards this target
        update_automaton_flow(ty, tx);
        
        // work out the adjacent square closest to the target (with preference for orthogonals)
        for (i = 7; i >= 0; i--)
        {
            // get the location
            y = p_ptr->py + ddy_ddd[i];
            x = p_ptr->px + ddx_ddd[i];
            
            // make sure it is in bounds
            if (!in_bounds(y, x)) continue;
            
            // determine how far it is from the target
            dist = flow_dist(FLOW_AUTOMATON, y, x);
            
            // if it is at least as good as anything so far, remember it
            if (dist <= best_dist)
            {
                found_direction = TRUE;
                best_dist = dist;
                best_dir = ddd[i];
            }
        }
    }
    
    // exit if no known target is found
    if (!found_direction)
    {
        msg_print("Could not work out which way to proceed.");
        stop_automaton();
        return;
    }

    // choose this best direction
    y = p_ptr->py + ddy[best_dir];
    x = p_ptr->px + ddx[best_dir];
    
    // sometimes bash doors
    if (cave_known_closed_door_bold(y, x))
    {
        if (one_in_(5)) base_command = '/';
    }
    
    // create the commands
    strnfmt(commands, sizeof(commands), "%c%c", base_command, '0' + best_dir);
    
    // queue the commands
    automaton_keypresses(commands);

    // if searching, we know that adjacent unmarked squares must be passable
    if ((base_command == ';') && (best_dir == 5))
    {
        // work out the adjacent square closest to the target (with preference for orthogonals)
        for (i = 7; i >= 0; i--)
        {
            // get the location
            y = p_ptr->py + ddy_ddd[i];
            x = p_ptr->px + ddx_ddd[i];
            
            automaton_map[y][x] = TRUE;
        }
    }
    
    // say 'yes' to visible traps
    if (cave_trap_bold(y,x) && !(cave_info[y][x] & (CAVE_HIDDEN)))
    {
        automaton_keypress('y');
    }
}




/*
 * (From the Angband Borg by Ben Harrison & Dr Andrew White)
 *
 * This function lets the automaton "steal" control from the user.
 *
 * The "z-term.c" file provides a special hook which we use to
 * bypass the standard "Term_flush()" and "Term_inkey()" functions
 * and replace them with the function below.
 *
 * The only way that the automaton can be stopped once it is started,
 * unless it dies or encounters an error, is to press any key.
 * This function checks for user input on a regular basic, and
 * when any is encountered, it relinquishes control gracefully.
 *
 * Note that this function hook automatically removes itself when
 * it realizes that it should no longer be active.  Note that this
 * may take place after the game has asked for the next keypress,
 * but the various "keypress" routines should be able to handle this.
 *
 * XXX XXX XXX We do not correctly handle the "take" flag
 */
char automaton_inkey_hack(int flush_first)
{
	int i;
    char ch;
    
	// paranoia
	if (!p_ptr->automaton)
	{
        stop_automaton();
        
		/* Nothing ready */
		return (ESCAPE);
	}
    
    // flush key buffer if requested
    if (flush_first)
    {
        // only flush if needed
        if (automaton_inkey(FALSE) != 0)
        {
            // Flush keys
            ////automaton_flush();   currently not actually doing this as it stops us queuing a 'y' for stepping on traps
        }
    }
    
	// check for manual user abort
	(void)Term_inkey(&ch, FALSE, TRUE);
    
    // if a key is hit, stop the automaton
    if (ch > 0)
    {
        stop_automaton();
        return (ESCAPE);
    }
    
	// check for a previously queued key, without taking it from the queue
	i = automaton_inkey(FALSE);
    
	// if it was empty and we need more keys
	if (!i)
	{
        // handle waiting for commands separately
        if (waiting_for_command)
        {
            // takes its turn by choosing some keys representing commands and queuing them
            automaton_turn();
            
            // pause for a moment so the user can see what is happening
            Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
        }
        
        else
        {
            /* Hack -- Process events (do not wait) */
            (void)Term_xtra(TERM_XTRA_EVENT, FALSE);
            
            stop_automaton();
            return (ESCAPE);
        }
	}
    
	// check for a previously queued key, taking it from the queue
	i = automaton_inkey(TRUE);
    
	// deal with empty queue
	if (!i)
	{
		// exit
		return('\0');
	}
    
	// return the key chosen
	return (i);
}



/*
 * Turn the automaton on.
 */
void do_cmd_automaton(void)
{
    int y, x;
    
    // set the flag to show the automaton is on
    p_ptr->automaton = TRUE;

    // empty the "keypress queue"
    automaton_flush();
    
    // allocate the "keypress queue"
    C_MAKE(automaton_key_queue, KEY_SIZE, char);

    // allocate automaton map
    C_MAKE(automaton_map, MAX_DUNGEON_HGT, byte_wid);
    
    // initialize automaton map
    for (y = 0; y < MAX_DUNGEON_HGT; y++)
        for (x = 0; x < MAX_DUNGEON_WID; x++)
        {
            automaton_map[y][x] = FALSE;
        }
    
    // activate the key stealer
    inkey_hack = automaton_inkey_hack;
}



