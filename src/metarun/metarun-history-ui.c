#include "angband.h"
#include "metarun-internal.h"

static int compare_metarun_indices(const void *a, const void *b)
{
    const s16b ia = *(const s16b *)a;
    const s16b ib = *(const s16b *)b;

    if (!metaruns) return 0;

    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->score != mb->score)
        return (ma->score < mb->score) ? 1 : -1;
    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id < mb->id) return -1;
    if (ma->id > mb->id) return 1;
    return 0;
}

void list_metaruns(void)
{
    screen_save();
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    int term_h = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int footer_row = term_h - 1;

    if (steamdeck) {
        /* Steam Deck UI: A=ok */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
    }
    Term_clear();
    c_prt(TERM_L_GREEN, "Meta-run history", 1, 2);
    c_put_str(TERM_L_DARK,
              " *ID      Score     Sil  Dth  Res  Last played", 3, 2);

    refresh_current_metar_score();

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    s16b *order = NULL;
    if (metarun_max > 0 && metaruns) {
        order = mem_alloc_array(metarun_max, s16b);
        for (s16b i = 0; i < metarun_max; i++) order[i] = i;
        qsort(order, metarun_max, sizeof(s16b), compare_metarun_indices);
    }

    int row = 4;
    for (s16b i = 0; i < metarun_max; i++) {
        s16b idx = order ? order[i] : i;
        const metarun *m = &metaruns[idx];

        /* Get dynamic win/loss conditions for this metarun type */
        int win_goal = WINCON_SILMARILS;
        int death_limit = LOSECON_DEATHS; /* hardcoded death limit for all runtypes */

        if (runtype_info && m->type < z_info->rt_max)
        {
            win_goal = runtype_info[m->type].win_con ? runtype_info[m->type].win_con : WINCON_SILMARILS;
        }

        char res = (m->silmarils >= win_goal) ? 'W' :
                   (m->deaths >= death_limit) ? 'L' : ' ';
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d",
                 localtime((time_t*)&m->last_played));

        byte attr = (idx == current_run) ? TERM_YELLOW : TERM_WHITE;
        char marker = (idx == current_run) ? '*' : ' ';

        c_put_str(attr,
                  format("%c%08u %8lu   %2d   %2d   %c   %s",
                         marker,
                         (unsigned)m->id,
                         (unsigned long)m->score,
                         m->silmarils, m->deaths, res, date),
                  row++, 2);

        if (row >= footer_row && i+1 < metarun_max) {   /* page break */
            if (steamdeck) {
                char hint_buf[64];
                strnfmt(hint_buf, sizeof(hint_buf), "[more - press %s]", accept_label);
                c_put_str(TERM_L_DARK, hint_buf, footer_row, 2);
            } else if (sdl_touch_only_device_active()) {
                c_put_str(TERM_L_DARK, "[more - tap to continue]", footer_row, 2);
            } else {
                c_put_str(TERM_L_DARK, "[more - any key]", footer_row, 2);
            }
            /* Register a full-screen tap target so touch-only users (whose
             * prompt no longer says "any key") can advance the page. */
            ui_menu_click_begin();
            for (int r = 0; r < (Term ? Term->hgt : 24); r++)
                ui_menu_click_add_full_row('\r', r);
            metarun_wait_hidden();  Term_clear();
            ui_menu_click_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " *ID      Score     Sil  Dth  Res  Last played", 3, 2);
        }
    }

    order = mem_free(order);
    if (steamdeck) {
        char hint_buf[64];
        strnfmt(hint_buf, sizeof(hint_buf), "Press %s to return.", accept_label);
        c_put_str(TERM_L_DARK, hint_buf, MIN(row + 1, footer_row), 2);
    } else if (sdl_touch_only_device_active()) {
        c_put_str(TERM_L_DARK, "Tap to return",
            MIN(row + 1, footer_row), 2);
    } else {
        c_put_str(TERM_L_DARK, "Press any key to return.",
            MIN(row + 1, footer_row), 2);
    }
    /* Register a full-screen tap target so touch-only users can dismiss. */
    ui_menu_click_begin();
    for (int r = 0; r < (Term ? Term->hgt : 24); r++)
        ui_menu_click_add_full_row('\r', r);
    metarun_wait_hidden();
    ui_menu_click_clear();
    screen_load();
}
