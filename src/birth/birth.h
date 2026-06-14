#ifndef INCLUDED_BIRTH_H
#define INCLUDED_BIRTH_H

#include "angband.h"

NavResult player_birth(void);
NavResult gain_skills(void);
NavResult character_creation(void);
NavResult character_creation_resume_character(void);
NavResult blitz_character_creation(void);
void gain_skills_set_initial_skill(int skill);
void player_wipe(void);

#endif /* INCLUDED_BIRTH_H */
