#ifndef INCLUDED_MELEE_COMBAT_DISPLAY_H
#define INCLUDED_MELEE_COMBAT_DISPLAY_H

#include "../angband.h"

extern void new_combat_round(void);
extern void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll);
extern void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis);
extern void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps,
    int prot, int prt_percent, int dam_type, bool melee);
extern void update_combat_rolls2_combo(int dd, int ds, int dam, int dd2,
    int ds2, int dam2, int pd, int ps, int prot, int prt_percent,
    int dam_type, bool melee);
extern void update_combat_rolls_no_damage(void);
extern void display_combat_rolls(void);
extern void display_combat_roll_line_at(int row, int base_col_offset,
    const combat_roll* roll);
extern void add_combat_round_to_history(void);
extern void do_cmd_combat_history(void);
extern void display_combat_round_details(combat_history_round* round);

#endif /* INCLUDED_MELEE_COMBAT_DISPLAY_H */
