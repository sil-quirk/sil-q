/* File: monster-lore.c */

#include "monster-internal.h"

/*
 * Pronoun arrays, by gender.
 */
cptr wd_he[3] = { "it", "he", "she" };
cptr wd_his[3] = { "its", "his", "her" };
cptr wd_him[3] = { "it", "him", "her" };

#define PLAYER_GHOST_TRIES_MAX 30

#define MANY_MANY_KILLS 10000

/*
 * Determine if the "armour" is known
 * One kill is needed.
 */
static bool know_armour(s32b kills)
{
    /* Normal monsters */
    if (kills > 0)
        return (true);

    /* Assume false */
    return (false);
}

/*
 * Determine if the "damage" of the given attack is known.
 * One attack is needed.
 */
bool know_damage(const monster_lore* l_ptr, int i)
{
    /* Unique monsters */
    if (l_ptr->blows[i])
        return (true);

    /* Assume false */
    return (false);
}

static bool lore_knows_contest_stats(const monster_lore* l_ptr)
{
    return ((l_ptr->song_lore_flags & MONSTER_LORE_SONG_CONTEST) != 0);
}

bool lore_knows_lament_stats(const monster_lore* l_ptr)
{
    return ((l_ptr->song_lore_flags & MONSTER_LORE_SONG_LAMENT) != 0);
}

static int monster_lore_live_index(const monster_type* m_ptr)
{
    if (!m_ptr)
        return 0;

    for (int i = 1; i < mon_max; i++)
    {
        if (&mon_list[i] == m_ptr)
            return i;
    }

    return 0;
}

static bool monster_lore_is_current_duel_target(
    const monster_type* m_ptr, int song)
{
    int m_idx = monster_lore_live_index(m_ptr);

    return (m_idx > 0) && (p_ptr->song_target_idx == m_idx)
        && (p_ptr->song_target_song == song);
}

static bool monster_lore_has_damage_dice_reduction(const monster_type* m_ptr)
{
    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        if (m_ptr->blow_dd_reduction[i] > 0)
            return true;
    }

    return false;
}

static void monster_lore_learn_live_song_duels(
    monster_lore* l_ptr, const monster_type* m_ptr)
{
    if (!m_ptr)
        return;

    if (monster_lore_is_current_duel_target(m_ptr, SNG_CONTEST)
        || (m_ptr->song_contest_stacks > 0) || m_ptr->song_contest_completed
        || (m_ptr->song_stealth_penalty > 0) || (m_ptr->song_evasion_penalty > 0)
        || (m_ptr->song_armor_dice_penalty > 0))
    {
        l_ptr->song_lore_flags |= MONSTER_LORE_SONG_CONTEST;
    }

    if (monster_lore_is_current_duel_target(m_ptr, SNG_LAMENT)
        || (m_ptr->song_lament_stacks > 0) || m_ptr->song_lament_completed
        || (monster_song_hp_loss(m_ptr) > 0)
        || monster_lore_has_damage_dice_reduction(m_ptr))
    {
        l_ptr->song_lore_flags |= MONSTER_LORE_SONG_LAMENT;
    }
}

static void describe_monster_desc(int r_idx)
{
    const monster_race* r_ptr = &r_info[r_idx];
    char buf[2048];

    /* Simple method */
    SDL_strlcpy(buf, r_text + r_ptr->text, sizeof(buf));

    /* Dump it */
    text_out(buf);
    text_out("\n");
}

static void describe_monster_kills(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int msex = 0;

    bool out = true;

    int real_tkills;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    // determine the real number of ancestor kills for printing purposes
    if (l_ptr->tkills < MANY_MANY_KILLS)
        real_tkills = l_ptr->tkills;
    else
        real_tkills = l_ptr->tkills - MANY_MANY_KILLS;

    /* Treat uniques differently */
    if (l_ptr->flags1 & RF1_UNIQUE)
    {
        /* Determine if the unique is "dead" */
        bool dead = (r_ptr->max_num == 0) ? true : false;

        /* We've been killed... */
        if (l_ptr->deaths)
        {
            /* Killed predecessors */
            text_out(format("%^s has slain %d of your predecessors",
                wd_he[msex], l_ptr->deaths));

            /* But we've also killed it */
            if (dead)
            {
                text_out(", but you have taken revenge!  ");
            }

            /* Unavenged (ever) */
            else
            {
                text_out(format(", who %s unavenged.  ",
                    plural(l_ptr->deaths, "remains", "remain")));
            }
        }

        /* Dead unique who never hurt us */
        else if (dead)
        {
            text_out("You have slain this foe.  ");
        }

        if (!dead)
        {
            if (l_ptr->psights)
            {
                text_out("You have encountered this foe.  ");
            }
            else
            {
                text_out("You are yet to encounter this foe.  ");
            }
        }
    }

    /* Not unique, but killed us */
    else if (l_ptr->deaths)
    {
        /* Dead predecessors */
        text_out(
            format("%d of your predecessors %s been killed by this creature, ",
                l_ptr->deaths, plural(l_ptr->deaths, "has", "have")));

        /* Some kills this life */
        if (l_ptr->pkills)
        {
            text_out(format(
                "and you have slain %d of the %d you have encountered.  ",
                l_ptr->pkills, l_ptr->psights));
        }

        /* Some kills past lives */
        else if (real_tkills)
        {
            text_out(format("and %s have slain %d in return.  ",
                "your predecessors", real_tkills));

            if (l_ptr->psights)
            {
                text_out(format("You have encountered %d.  ", l_ptr->psights));
            }
            else
            {
                text_out("You are yet to encounter one.  ");
            }
        }

        /* No kills */
        else
        {
            text_out_c(TERM_RED,
                format("and it is not known to have been defeated.  "));

            if (l_ptr->psights)
            {
                text_out(format("You have encountered %d.  ", l_ptr->psights));
            }
            else
            {
                text_out("You are yet to encounter one.  ");
            }
        }
    }

    /* Normal monsters */
    else
    {
        /* Encountered some this life */
        if (l_ptr->psights && !l_ptr->pkills)
        {
            text_out(format("You have encountered %d of these creatures, ",
                l_ptr->psights));

            /* Killed some last life */
            if (real_tkills)
            {
                text_out(format(
                    "and your predecessors have slain %d.  ", real_tkills));
            }

            /* Killed none */
            else
            {
                text_out("but no battles to the death are recalled.  ");
            }
        }

        /* Killed some this life */
        else if (l_ptr->pkills)
        {
            text_out(
                format("You have slain %d of the %d you have encountered.  ",
                    l_ptr->pkills, l_ptr->psights));
        }

        /* Encountered some this life */
        else
        {
            text_out(format("You have encountered none of these creatures, ",
                l_ptr->psights));

            /* Killed some this life -- should be impossible to reach here */
            if (l_ptr->pkills)
            {
                text_out(format("but slain %d.  ", l_ptr->pkills));
            }

            /* Killed some last life */
            else if (real_tkills)
            {
                text_out(format(
                    "but your predecessors have slain %d.  ", real_tkills));
            }

            /* Killed none */
            else
            {
                text_out("and no battles to the death are recalled.  ");
            }
        }
    }

    /* Separate */
    if (out)
        text_out("\n");
}

static void describe_monster_toughness(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    bool knows_full_toughness = ((know_armour(l_ptr->tkills))
        || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR));
    bool knows_hp = knows_full_toughness || lore_knows_lament_stats(l_ptr);
    bool knows_defence = knows_full_toughness || lore_knows_contest_stats(l_ptr);

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Describe monster "toughness" */
    if (knows_hp || knows_defence)
    {
        int evn = r_ptr->evn;
        int prot_dice = r_ptr->pd;
        int prot_sides = r_ptr->ps;

        /* Health */
        char hp_text[32];
        if (r_ptr->hdice > 0 && r_ptr->hside > 0)
            strnfmt(hp_text, sizeof(hp_text), "%dd%d", r_ptr->hdice, r_ptr->hside);
        else if (r_ptr->hdice > 0)
            strnfmt(hp_text, sizeof(hp_text), "%d", r_ptr->hdice);
        else
            strnfmt(hp_text, sizeof(hp_text), "%d", r_ptr->hside);

        if (knows_hp)
        {
            text_out(format("%^s has ", wd_he[msex]));
            text_out_c(TERM_GREEN, hp_text);
            if (m_ptr)
            {
                int hp_loss = monster_song_hp_loss(m_ptr);
                if (hp_loss > 0)
                    text_out_c(TERM_L_RED, format("-%d", hp_loss));
            }
            text_out(" hp");
        }

        /* Defence */
        if (knows_defence && m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            evn = total_monster_evasion(live, false);

            int base_dice = r_ptr->pd - m_ptr->song_armor_dice_penalty;
            if (base_dice < 0)
                base_dice = 0;
            prot_dice = base_dice + curse_flag_delta_cur(CUR_MON_ARM_DICE);
            if (prot_dice < 0)
                prot_dice = 0;

            int base_sides = monster_base_armour_sides(live);
            base_sides += curse_flag_delta_cur(CUR_MON_ARM_SIDE);
            if (base_sides < 0)
                base_sides = 0;
            prot_sides = base_sides;

            if (prot_dice > 0 && prot_sides < 1)
                prot_sides = 1;
        }

        if (knows_hp && knows_defence)
            text_out(" and a defence of ");
        else if (knows_defence)
            text_out(format("%^s has a defence of ", wd_he[msex]));

        if (knows_defence && (prot_dice > 0) && (prot_sides > 0))
        {
            text_out_c(
                TERM_SLATE, format("[%+d, %dd%d]", evn, prot_dice, prot_sides));
        }
        else if (knows_defence)
        {
            text_out_c(TERM_SLATE, format("[%+d]", evn));
        }

        text_out(".  ");
    }
}

static void describe_monster_skills(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int msex = 0;
    cptr act;
    bool knows_normal_skills;
    bool knows_will;
    bool knows_stealth;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    knows_normal_skills = ((l_ptr->ranged == MAX_UCHAR)
        || ((l_ptr->tsights > 1)
            && (10 - l_ptr->tsights < p_ptr->skill_use[S_PER])));
    knows_will = knows_normal_skills || lore_knows_contest_stats(l_ptr)
        || lore_knows_lament_stats(l_ptr);
    knows_stealth = (knows_normal_skills
        && p_ptr->active_ability[S_PER][PER_LISTEN])
        || lore_knows_contest_stats(l_ptr);

    /* Describe experience if known */
    if (knows_will || knows_stealth || knows_normal_skills)
    {
        int will = r_ptr->wil;
        int stealth = r_ptr->stl;
        int perception = r_ptr->per;
        bool printed = false;

        if (m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            will = monster_skill(live, S_WIL);
            stealth = monster_skill(live, S_STL);
            perception = monster_skill(live, S_PER);
        }

        text_out(format("%^s has ", wd_he[msex]));
        if (knows_will)
        {
            text_out_c(TERM_L_GREEN, format("%d", will));
            text_out(" Will");
            printed = true;
        }
        if (knows_stealth)
        {
            if (printed)
                text_out(", ");
            text_out_c(TERM_L_GREEN, format("%d", stealth));
            text_out(" Stealth");
            printed = true;
        }
        if (knows_normal_skills)
        {
            if (printed)
                text_out(", ");
            text_out_c(TERM_L_GREEN, format("%d", perception));
            text_out(" Perception");
        }

        if (r_ptr->sleep > 20) // 21 +
        {
            act = "is usually found asleep";
        }
        else if (r_ptr->sleep > 15) // 16 to 20
        {
            act = "is often found asleep";
        }
        else if (r_ptr->sleep > 10) // 11 to 15
        {
            act = "is sometimes found asleep";
        }
        else if (r_ptr->sleep > 5) // 6 to 10
        {
            act = "is never found asleep";
        }
        else if (r_ptr->sleep > 1) // 2 to 5
        {
            act = "is quick to notice intruders";
        }
        else if (r_ptr->sleep > 0) // 1
        {
            act = "is very quick to notice intruders";
        }
        else // 0
        {
            act = "is ever vigilant";
        }

        if (!knows_normal_skills || (r_ptr->flags2 & (RF2_MINDLESS)))
        {
            text_out(".  ");
        }
        else
        {
            text_out(format(", and %s.  ", act));
        }
    }
}

static void describe_monster_song_duel_progress(
    const monster_lore* l_ptr, const monster_type* m_ptr)
{
    bool printed = false;

    if (!m_ptr)
        return;

    if (lore_knows_contest_stats(l_ptr) && !m_ptr->song_contest_completed)
    {
        text_out_c(TERM_YELLOW,
            format("Song of Contest pressure: %d/%d",
                m_ptr->song_contest_stacks, SONG_DUEL_STACK_LIMIT));
        printed = true;
    }

    if (lore_knows_lament_stats(l_ptr) && !m_ptr->song_lament_completed)
    {
        if (printed)
            text_out("; ");
        text_out_c(TERM_YELLOW,
            format("Song of Lament pressure: %d/%d",
                m_ptr->song_lament_stacks, SONG_DUEL_STACK_LIMIT));
        printed = true;
    }

    if (printed)
        text_out(".  ");
}

static void describe_monster_exp(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    long i;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Describe experience if known */
    if (l_ptr->tkills || l_ptr->tsights)
    {
        /* Introduction for Encounters */
        if (l_ptr->psights)
        {
            if (l_ptr->flags1 & RF1_UNIQUE)
            {
                text_out(format("Encountering %s was worth", wd_him[msex]));
            }
            else
                text_out("Encountering another would be worth");
        }
        else
        {
            if (l_ptr->flags1 & RF1_UNIQUE)
                text_out(
                    format("Encountering %s would be worth", wd_him[msex]));
            else
                text_out("Encountering one would be worth");
        }

        /* calculate the integer exp part */
        i = adjusted_mon_exp(r_ptr, false);

        /* Mention the experience */
        text_out(format(" %ld experience.  ", (long)i));

        /* Introduction for Kills */
        if (!(r_ptr->flags1 & (RF1_PEACEFUL)))
        {
            if (l_ptr->pkills)
            {
                if (l_ptr->flags1 & RF1_UNIQUE)
                    text_out(format("Killing %s was worth", wd_him[msex]));
                else
                    text_out("Killing another would be worth");
            }
            else
            {
                if (l_ptr->flags1 & RF1_UNIQUE)
                    text_out(format("Killing %s would be worth", wd_him[msex]));
                else
                    text_out("Killing one would be worth");
            }

            /* calculate the integer exp part */
            i = adjusted_mon_exp(r_ptr, true);

            /* Mention the experience */
            text_out(format(" %ld.  ", (long)i));
        }
    }
}

static void describe_monster_movement(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    bool old = false;

    int display_speed = r_ptr->speed;
    bool is_hasted = false;
    bool is_slowed = false;

    if (m_ptr)
    {
        if (m_ptr->mspeed)
            display_speed = m_ptr->mspeed;
        is_hasted = (m_ptr->hasted > 0);
        is_slowed = (m_ptr->slowed > 0);
    }

    text_out("This");

    if (l_ptr->flags3 & RF3_UNDEAD)
        text_out_c(TERM_L_BLUE, " undead");

    if (l_ptr->flags3 & RF3_DRAGON)
        text_out_c(TERM_L_BLUE, " dragon");
    else if (l_ptr->flags3 & RF3_SERPENT)
        text_out_c(TERM_L_BLUE, " serpent");
    else if (l_ptr->flags3 & RF3_VAMPIRE)
        text_out_c(TERM_L_BLUE, " vampire");
    else if (l_ptr->flags3 & RF3_RAUKO)
        text_out_c(TERM_L_BLUE, " rauko");
    else if (l_ptr->flags3 & RF3_TROLL)
        text_out_c(TERM_L_BLUE, " troll");
    else if (l_ptr->flags3 & RF3_ORC)
        text_out_c(TERM_L_BLUE, " orc");
    else if (l_ptr->flags3 & RF3_GIANT)
        text_out_c(TERM_L_BLUE, " giant");
    else if (l_ptr->flags3 & RF3_WOLF)
        text_out_c(TERM_L_BLUE, " wolf");
    else if (l_ptr->flags3 & RF3_SPIDER)
        text_out_c(TERM_L_BLUE, " spider");
    else if (l_ptr->flags3 & RF3_CAT)
        text_out_c(TERM_L_BLUE, " cat");
    else if (l_ptr->flags3 & RF3_HORROR)
        text_out_c(TERM_L_BLUE, " horror");
    else if (l_ptr->flags3 & RF3_MAN)
        text_out_c(TERM_L_BLUE, " man");
    else if (l_ptr->flags3 & RF3_ELF)
        text_out_c(TERM_L_BLUE, " elf");
    else
        text_out(" creature");

    /* Describe location */
    if (r_ptr->level == 0)
    {
        text_out_c(TERM_YELLOW, " dwells at the gates of Angband");
        old = true;
    }
    else
    {
        if (l_ptr->flags1 & RF1_FORCE_DEPTH)
            text_out(" is found ");
        else
            text_out(" is normally found ");

        if (r_idx == R_IDX_CARCHAROTH)
        {
            text_out_c(TERM_YELLOW, "guarding the gates of Angband");
        }
        else if (r_ptr->level < MORGOTH_DEPTH)
        {
            text_out("at depths of ");
            text_out_c(TERM_YELLOW, format("%d feet", r_ptr->level * 50));
        }
        else
        {
            text_out("at depths of ");
            text_out_c(TERM_YELLOW, format("%d feet", MORGOTH_DEPTH * 50));
        }
        old = true;
    }

    /* Deal with non-moving creatures */
    if (l_ptr->flags1 & RF1_NEVER_MOVE)
    {
        if (old)
            text_out(", and");
        text_out(" cannot move");
    }

    /* Deal with non-moving creatures */
    else if (l_ptr->flags1 & RF1_HIDDEN_MOVE)
    {
        if (old)
            text_out(", and");
        text_out(" never moves when you are looking");
    }

    /* most other creatures display their speed */
    else if ((display_speed != 2) || (l_ptr->flags1 & RF1_RAND_50)
        || (l_ptr->flags1 & RF1_RAND_25))
    {
        if (old)
            text_out(", and");
        text_out(" moves");

        /* Random-ness */
        if ((l_ptr->flags1 & RF1_RAND_50) || (l_ptr->flags1 & RF1_RAND_25))
        {
            /* Adverb */
            if ((l_ptr->flags1 & RF1_RAND_50) && (l_ptr->flags1 & RF1_RAND_25))
            {
                text_out(" extremely");
            }
            else if (l_ptr->flags1 & RF1_RAND_50)
            {
                text_out(" somewhat");
            }
            else if (l_ptr->flags1 & RF1_RAND_25)
            {
                text_out(" a bit");
            }

            /* Adjective */
            text_out(" erratically");

            /* Hack -- Occasional conjunction */
            if (display_speed != 2)
                text_out(", and");
        }

        /* Speed */
        if (display_speed > 2)
        {
            if (display_speed > 5)
                text_out_c(TERM_L_GREEN, " incredibly");
            else if (display_speed > 4)
                text_out_c(TERM_L_GREEN, " extremely");
            else if (display_speed > 3)
                text_out_c(TERM_L_GREEN, " very");
            text_out_c(TERM_L_GREEN, " quickly");
        }
        else if (display_speed < 2)
        {
            text_out_c(TERM_L_UMBER, " slowly");
        }

        if (m_ptr)
        {
            text_out_c(
                TERM_L_GREEN, format(" (speed %d)", display_speed));
            if (is_hasted && is_slowed)
                text_out(" while hasted and slowed");
            else if (is_hasted)
                text_out(" while hasted");
            else if (is_slowed)
                text_out(" while slowed");
        }
    }

    /* End this sentence */
    text_out(".  ");

    /*note if this monster does not pursue you*/
    if (l_ptr->flags2 & (RF2_TERRITORIAL))
    {
        int msex = 0;

        /* Extract a gender (if applicable) */
        if (r_ptr->flags1 & RF1_FEMALE)
            msex = 2;
        else if (r_ptr->flags1 & RF1_MALE)
            msex = 1;

        text_out(format("%^s does not deign to pursue you.  ", wd_he[msex]));
    }
}

/*
 * Learn everything about a monster (by cheating)
 */
static void cheat_monster_lore(int r_idx, monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int i;

    /* Hack -- Maximal predecessors kills */
    if (l_ptr->tkills < MANY_MANY_KILLS)
        l_ptr->tkills += MANY_MANY_KILLS;

    /* Hack -- Maximal info */
    l_ptr->notice = l_ptr->ignore = MAX_UCHAR;

    /* Observe "maximal" attacks */
    for (i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        /* Examine "actual" blows */
        if (r_ptr->blow[i].effect || r_ptr->blow[i].method)
        {
            /* Hack -- maximal observations */
            l_ptr->blows[i] = MAX_UCHAR;
        }
    }

    /* Hack -- maximal drops */
    l_ptr->drop_item = (((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0)
        + ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0)
        + ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0)
        + ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0)
        + ((r_ptr->flags1 & RF1_DROP_100) ? 1 : 0)
        + ((r_ptr->flags1 & RF1_DROP_33) ? 1 : 0)
        + ((r_ptr->flags3 & RF3_DROP_1D3) ? 3 : 0));

    /* DROP_ARTEFACT always yields at least one drop slot at runtime (see
     * monster_death), so reflect that here even when the monster carries no
     * count flag of its own (e.g. Ancalagon, Gorthaur). */
    if ((r_ptr->flags3 & RF3_DROP_ARTEFACT) && l_ptr->drop_item < 1)
        l_ptr->drop_item = 1;

    /* Hack -- observe many spells */
    l_ptr->ranged = MAX_UCHAR;

    /* Hack -- know all the flags */
    l_ptr->flags1 = r_ptr->flags1;
    l_ptr->flags2 = r_ptr->flags2;
    l_ptr->flags3 = r_ptr->flags3;
    l_ptr->flags4 = r_ptr->flags4;
}

/*
 * Hack -- display monster information using "roff()"
 *
 * Note that there is now a compiler option to only read the monster
 * descriptions from the raw file when they are actually needed, which
 * saves about 60K of memory at the cost of disk access during monster
 * recall, which is optional to the user.
 *
 * This function should only be called with the cursor placed at the
 * left edge of the screen, on a cleared line, in which the recall is
 * to take place.  One extra blank line is left after the recall.
 */
bool monster_recall_blocked_by_hallucination(void)
{
    return (p_ptr && p_ptr->image);
}

void monster_recall_show_hallucination_block(void)
{
    msg_print("Your vision is too distorted to study monsters carefully.");
    window_stuff();
    Term_fresh();
}

void describe_monster(int r_idx, bool spoilers, const monster_type* m_ptr)
{
    monster_lore lore;

    monster_lore save_mem;

    /* Get the race and lore */
    const monster_race* r_ptr = &r_info[r_idx];
    monster_lore* l_ptr = &l_list[r_idx];

    monster_lore_learn_live_song_duels(l_ptr, m_ptr);

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info)
    {
        /* XXX XXX XXX */

        /* Hack -- save memory */
        memcpy(&save_mem, l_ptr, sizeof(monster_lore));
    }

    /* Hack -- create a copy of the monster-memory */
    memcpy(&lore, l_ptr, sizeof(monster_lore));

    /* Assume some "obvious" flags */
    lore.flags1 |= (r_ptr->flags1 & RF1_OBVIOUS_MASK);

    /* Killing a monster reveals some properties */
    if (lore.tkills)
    {
        /* Know "race" flags */
        lore.flags3 |= (r_ptr->flags3 & RF3_RACE_MASK);

        /* Know "forced" flags */
        lore.flags1 |= (r_ptr->flags1 & (RF1_FORCE_DEPTH));
    }

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info || spoilers)
    {
        cheat_monster_lore(r_idx, &lore);
    }

    /* Show kills of monster vs. player(s) */
    if (!spoilers)
        describe_monster_kills(r_idx, &lore);

    /* Describe experience */
    if (!spoilers)
        describe_monster_exp(r_idx, &lore);

    /* Monster description */
    describe_monster_desc(r_idx);

    /* Describe the movement and level of the monster */
    describe_monster_movement(r_idx, &lore, m_ptr);

    /* Describe spells and innate attacks */
    describe_monster_spells(r_idx, &lore);

    /* Describe the abilities of the monster */
    describe_monster_abilities(r_idx, &lore);

    /* Describe the known attacks */
    describe_monster_attack(r_idx, &lore, m_ptr);

    /* Describe monster "toughness" */
    describe_monster_toughness(r_idx, &lore, m_ptr);

    /* Describe the known skills */
    describe_monster_skills(r_idx, &lore, m_ptr);

    /* Describe duel-song pressure on this individual monster */
    describe_monster_song_duel_progress(&lore, m_ptr);

    /* Describe the monster drop */
    describe_monster_drop(r_idx, &lore);

    /* All done */
    text_out("\n");

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info)
    {
        /* Hack -- restore memory */
        memcpy(l_ptr, &save_mem, sizeof(monster_lore));
    }
}
