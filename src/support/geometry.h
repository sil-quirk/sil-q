#ifndef INCLUDED_SUPPORT_GEOMETRY_H
#define INCLUDED_SUPPORT_GEOMETRY_H

#include "h-basic.h"

extern byte get_angle_to_grid[41][41];
int get_angle_to_target(int y0, int x0, int y1, int x1, int dir);
void get_grid_using_angle(int angle, int y0, int x0, int* ty, int* tx);

#endif /* INCLUDED_SUPPORT_GEOMETRY_H */
