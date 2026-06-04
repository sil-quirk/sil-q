/* File: player/player-song-internal.h */

#ifndef INCLUDED_PLAYER_SONG_INTERNAL_H
#define INCLUDED_PLAYER_SONG_INTERNAL_H

#include "player-songs.h"

bool song_is_duel(int song);
void display_synergy_message(int song1, int song2);
void song_duel_clear_player_target(void);
monster_type* song_duel_get_target(int song);
void song_duel_reset_player_stack(void);
void song_duel_learn_target_stats(monster_type* m_ptr, int song);
void song_duel_reveal_target_stats(monster_type* m_ptr, int song);
bool song_duel_select_target(int song);
bool song_duel_process_contest(int song_skill);
bool song_duel_process_lament(int song_skill);

bool song_disguise_is_active(void);
bool song_disguise_any_monster_observes_player(void);
void song_disguise_start(void);
void song_disguise_stop(void);
void sing_song_of_disguise(int score);

void song_revealing_decay(void);
void song_revealing_clear(void);
void song_revealing_handle_monster_removed(int m_idx);
void sing_song_of_revealing(int score);

void sing_song_of_freedom(int score);
void sing_song_of_challenge(int score);
void sing_song_of_delvings(int score);
void sing_song_of_elbereth(int score);
void sing_song_of_trees(int score);
void sing_song_of_lorien(int score);

#endif /* INCLUDED_PLAYER_SONG_INTERNAL_H */
