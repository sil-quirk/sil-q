/* File: spell/spell-teleport.h */

#ifndef INCLUDED_SPELL_TELEPORT_H
#define INCLUDED_SPELL_TELEPORT_H

void teleport_away(int m_idx, int dis);
void teleport_player(int dis);
void teleport_player_to(int ny, int nx);
void teleport_towards(int oy, int ox, int ny, int nx);
void teleport_player_level(void);

#endif /* INCLUDED_SPELL_TELEPORT_H */
