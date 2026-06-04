/* File: spell/spell-projection.h */

#ifndef INCLUDED_SPELL_PROJECTION_H
#define INCLUDED_SPELL_PROJECTION_H

#include "../h-basic.h"

bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, int degrees, bool uniform);
bool project_bolt(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg);
bool project_beam(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg);
bool project_ball(int who, int rad, int y0, int x0, int y1, int x1, int dd,
    int ds, int dif, int typ, u32b flg, bool uniform);
bool explosion(
    int who, int rad, int y0, int x0, int dd, int ds, int dif, int typ);
bool project_arc(int who, int rad, int y0, int x0, int y1, int x1,
    int dd, int ds, int dif, int typ, u32b flg, int degrees);
bool project_los_not_player(
    int y1, int x1, int dd, int ds, int dif, int typ);
bool project_los(int typ, int dd, int ds, int dif, bool silent);
bool project_los_grids(int typ, int dd, int ds, int dif);
bool fire_bolt_beam_special(
    int typ, int dir, int dd, int ds, int dif, int rad, u32b flg);
bool fire_ball(int typ, int dir, int dd, int ds, int dif, int rad);
bool fire_arc(
    int typ, int dir, int dd, int ds, int dif, int rad, int degrees);
bool fire_bolt(int typ, int dir, int dd, int ds, int dif);
bool fire_beam(int typ, int dir, int dd, int ds, int dif);
bool fire_bolt_or_beam(
    int prob, int typ, int dir, int dd, int ds, int dif);

#endif /* INCLUDED_SPELL_PROJECTION_H */
