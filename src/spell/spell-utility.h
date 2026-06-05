/* File: spell/spell-utility.h */

#ifndef INCLUDED_SPELL_UTILITY_H
#define INCLUDED_SPELL_UTILITY_H

#include "../h-basic.h"

typedef struct object_type object_type;
typedef struct monster_type monster_type;

bool hp_player(int x, bool percent, bool message);
void warding_glyph(void);
bool do_dec_stat(int stat, monster_type* m_ptr);
bool do_res_stat(int stat, int points);
bool do_inc_stat(int stat);
void identify_pack(void);
void uncurse_object(object_type* o_ptr);
bool remove_curse(bool star_curse);
void self_knowledge(void);
void self_knowledge_defer_display_push(void);
void self_knowledge_defer_display_pop(void);
bool self_knowledge_display_pending(void);
void spells2_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);

#endif /* INCLUDED_SPELL_UTILITY_H */
