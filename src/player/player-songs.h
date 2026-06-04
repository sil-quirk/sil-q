/* File: player/player-songs.h */

#ifndef INCLUDED_PLAYER_SONGS_H
#define INCLUDED_PLAYER_SONGS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

void song_of_binding(monster_type* m_ptr);
void song_of_piercing(monster_type* m_ptr);
void song_of_oaths(monster_type* m_ptr);
void hatch_spider(monster_type* m_ptr);
void change_song(int song);
bool singing(int song);
cptr song_voice_cost_desc(int song);
void sing(void);
void song_disguise_new_player_turn(void);
void song_disguise_handle_monster_removed(int m_idx);
void song_disguise_note_monster_attack(int m_idx);
void song_disguise_note_player_attack(int m_idx);
bool song_disguise_monster_is_fooled(const monster_type* m_ptr);
bool song_revealing_overlay(int m_idx, byte* a, char* c);
void song_duels_new_player_turn(void);
void song_duels_handle_monster_removed(int m_idx);
void sing_song_of_shattering(int score);
void shatter_in_arc(int dir, int score);

#endif /* INCLUDED_PLAYER_SONGS_H */
