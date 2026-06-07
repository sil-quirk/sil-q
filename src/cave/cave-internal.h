/* File: cave-internal.h */

#ifndef INCLUDED_CAVE_INTERNAL_H
#define INCLUDED_CAVE_INTERNAL_H

#include "../angband.h"
#include "../externs.h"
#include "../log/log.h"
#include "cave.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128
#endif

#ifndef COLOR_STYLE_SLOT_MAX
#define COLOR_STYLE_SLOT_MAX 64
#endif

#ifndef COLOR_STYLE_FLAG_FIRSTVAR
#define COLOR_STYLE_FLAG_FIRSTVAR COLOR_STYLE_SLOT_MAX
#endif

#ifndef DEPTH_BASED_WALLS
#define DEPTH_BASED_WALLS 1
#endif

#define VINFO_MAX_GRIDS 161
#define VINFO_MAX_SLOPES 126
#define VINFO_BITS_3 0x3FFFFFFF
#define VINFO_BITS_2 0xFFFFFFFF
#define VINFO_BITS_1 0xFFFFFFFF
#define VINFO_BITS_0 0xFFFFFFFF
#define SCALE 100000L

typedef struct vinfo_type vinfo_type;

struct vinfo_type
{
    s16b grid[8];

    u32b bits_3;
    u32b bits_2;
    u32b bits_1;
    u32b bits_0;

    byte slope_fire_index1;
    byte slope_fire_index2;

    vinfo_type* next_0;
    vinfo_type* next_1;

    byte y;
    byte x;
    byte d;
    byte r;
};

extern vinfo_type vinfo[VINFO_MAX_GRIDS];

byte cave_style_floor_choice(int sidx);
byte cave_style_door_choice(int sidx);
bool cave_style_index_is_valid(int sidx);
int cave_style_index_for_color(byte color_value);
bool cave_style_color_force_first_variant(byte color_value);
int cave_hallucination_style_for_display(int sidx);
byte cave_get_active_style_color(void);
int cave_style_primary_for_grid(int y, int x);
int cave_style_ascii_attr(int sidx);

void cave_feature_visual(const feature_type* f_ptr, byte* a, char* c);
void cave_monster_visual(const monster_race* r_ptr, byte* a, char* c);

#endif /* INCLUDED_CAVE_INTERNAL_H */
