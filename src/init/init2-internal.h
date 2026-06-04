#ifndef INCLUDED_INIT2_INTERNAL_H
#define INCLUDED_INIT2_INTERNAL_H

#include "angband.h"
#include "init.h"

void init_header(header* head, int num, int len);
void display_parse_error(cptr filename, errr err, cptr buf);
errr free_info(header* head);

errr init_z_info(void);
errr init_f_info(void);
errr init_style_info(void);
errr init_partition_info(void);
errr init_k_info(void);
errr init_b_info(void);
errr init_a_info(void);
void ensure_artifact_guids(void);
void ensure_artifact_spawn_numbers(void);
errr init_e_info(void);
errr init_r_info(void);
errr init_v_info(void);
errr init_rt_info(void);
errr init_p_info(void);
errr init_c_info(void);
errr init_h_info(void);
errr init_st_info(void);
errr init_cu_info(void);
errr init_mb_info(void);
errr init_n_info(void);
errr init_flavor_info(void);
errr init_effect_info(void);
errr init_skeleton_note_info(void);
errr init_quest_info(void);
errr init_oath_info(void);

errr init_other(void);
errr init_alloc(void);

#endif /* INCLUDED_INIT2_INTERNAL_H */