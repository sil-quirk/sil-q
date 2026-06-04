/* File: spell/spell-terrain.h */

#ifndef INCLUDED_SPELL_TERRAIN_H
#define INCLUDED_SPELL_TERRAIN_H

#include "../h-basic.h"

bool lock_door(int y, int x, int power);
bool lock_doors_radius(int y0, int x0, int radius, int power);
void stair_creation(void);
void clear_temp_array(void);
void cave_temp_mark(int y, int x, bool room);
void spread_cave_temp(int y1, int x1, int range, bool room);
bool destroy_traps(int power);
bool open_doors(int power);
bool lock_doors(int power);
void destroy_area(int y1, int x1, int r, bool full);
void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who);
bool close_chasm(int y, int x, int power);
bool close_chasms(int power);
void light_room(int y1, int x1);
void darken_room(int y1, int x1);
bool light_area(int dd, int ds, int rad);
bool darken_area(int dd, int ds, int rad);
bool light_line(int dir);
bool destroy_door(int dir);
bool disarm_trap(int dir);

#endif /* INCLUDED_SPELL_TERRAIN_H */
