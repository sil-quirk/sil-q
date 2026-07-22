#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include <math.h>

extern void ident(object_type* o_ptr)
{
    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Apply an autoinscription, if necessary */
    apply_autoinscription(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    return;
}

extern void ident_on_wield(object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    u32b orig_f1;

    bool notice = false;

    char o_full_name[80];

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Get the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    orig_f1 = f1;

    // Ignore previously identified items
    if (object_known_p(o_ptr))
    {
        return;
    }

    // identify the special item types that do nothing much
    // (since they have no hidden abilities, they must already be obvious)
    if (object_has_ego(o_ptr))
    {
        bool all_trivial = true;
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        if (ego_pfx)
        {
            ego_item_type* e_ptr = &e_info[ego_pfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (ego_sfx)
        {
            ego_item_type* e_ptr = &e_info[ego_sfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (all_trivial)
            notice = true;
    }

    // identify true sight if it cures blindness
    if (p_ptr->blind && (f2 & (TR2_SEE_INVIS)))
    {
        notice = true;
    }

    // Currently tunneling is an unambiguous ego on mattocks, so auto-ID
    if (f1 & TR1_TUNNEL)
    {
        notice = true;
    }

    if (f3 & TR3_ACCURATE)
    {
        notice = true;
    }

    if (f3 & TR3_CUMBERSOME)
    {
        notice = true;
    }

    if (o_ptr->name1 || object_has_ego(o_ptr))
    {
        // For special items and artefacts, we need to ignore the flags that are
        // basic to the object type and focus on the special/artefact ones. We
        // can do this by subtracting out the basic flags

        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);

        /*
         * If a special/artefact modifies pval on a base that already has a pval
         * flag (e.g. Shadow Cloak has STEALTH), stripping base flags would hide
         * the effect and prevent auto-identification on wear.
         */
        {
            u32b base_pval_flags = (orig_f1 & k_ptr->flags1);

            if ((base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES))
                && (o_ptr->pval != k_ptr->pval))
            {
                f1 |= (base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES));
            }

            if ((base_pval_flags & (TR1_STR | TR1_NEG_STR))
                && (o_ptr->stat_bonus[A_STR] != k_ptr->stat_bonus[A_STR]))
                f1 |= (base_pval_flags & (TR1_STR | TR1_NEG_STR));
            if ((base_pval_flags & (TR1_DEX | TR1_NEG_DEX))
                && (o_ptr->stat_bonus[A_DEX] != k_ptr->stat_bonus[A_DEX]))
                f1 |= (base_pval_flags & (TR1_DEX | TR1_NEG_DEX));
            if ((base_pval_flags & (TR1_CON | TR1_NEG_CON))
                && (o_ptr->stat_bonus[A_CON] != k_ptr->stat_bonus[A_CON]))
                f1 |= (base_pval_flags & (TR1_CON | TR1_NEG_CON));
            if ((base_pval_flags & (TR1_GRA | TR1_NEG_GRA))
                && (o_ptr->stat_bonus[A_GRA] != k_ptr->stat_bonus[A_GRA]))
                f1 |= (base_pval_flags & (TR1_GRA | TR1_NEG_GRA));

            if ((base_pval_flags & TR1_MEL)
                && (o_ptr->skill_bonus[S_MEL] != k_ptr->skill_bonus[S_MEL]))
                f1 |= (base_pval_flags & TR1_MEL);
            if ((base_pval_flags & TR1_ARC)
                && (o_ptr->skill_bonus[S_ARC] != k_ptr->skill_bonus[S_ARC]))
                f1 |= (base_pval_flags & TR1_ARC);
            if ((base_pval_flags & TR1_STL)
                && (o_ptr->skill_bonus[S_STL] != k_ptr->skill_bonus[S_STL]))
                f1 |= (base_pval_flags & TR1_STL);
            if ((base_pval_flags & TR1_PER)
                && (o_ptr->skill_bonus[S_PER] != k_ptr->skill_bonus[S_PER]))
                f1 |= (base_pval_flags & TR1_PER);
            if ((base_pval_flags & TR1_WIL)
                && (o_ptr->skill_bonus[S_WIL] != k_ptr->skill_bonus[S_WIL]))
                f1 |= (base_pval_flags & TR1_WIL);
            if ((base_pval_flags & TR1_SMT)
                && (o_ptr->skill_bonus[S_SMT] != k_ptr->skill_bonus[S_SMT]))
                f1 |= (base_pval_flags & TR1_SMT);
            if ((base_pval_flags & TR1_SNG)
                && (o_ptr->skill_bonus[S_SNG] != k_ptr->skill_bonus[S_SNG]))
                f1 |= (base_pval_flags & TR1_SNG);
        }
    }

    if (f2 & (TR2_DARKNESS))
    {
        notice = true;
        msg_print("It reduces your light radius, but concentrates the light that remains.");
    }
    else if (f4 & (TR4_UNLIGHT))
    {
        notice = true;
        msg_print("It reduces your light radius without concentrating the light that remains.");
    }
    else if (f2 & (TR2_LIGHT))
    {
        if (o_ptr->tval != TV_LIGHT)
        {
            notice = true;
            msg_print("It glows with a wondrous light.");
        }
        else if ((o_ptr->sval == SV_LIGHT_FEANORIAN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL) || (o_ptr->timeout > 0))
        {
            notice = true;
            msg_print("It glows very brightly.");
        }
    }
    else if (f2 & (TR2_SLOWNESS))
    {
        notice = true;
        msg_print("It slows your movement.");
    }
    else if (f2 & (TR2_SPEED))
    {
        notice = true;
        msg_print("It speeds your movement.");
    }

    else if (f1 & (TR1_DAMAGE_SIDES))
    {
        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (o_ptr->pval > 0)
        {
            notice = true;
            msg_print("You feel more forceful in melee.");
        }
        else if (o_ptr->pval < 0)
        {
            notice = true;
            msg_print("You feel less forceful in melee.");
        }
    }
    else if ((f1 & (TR1_STR)) || (f1 & (TR1_NEG_STR)))
    {
        int bonus = o_ptr->stat_bonus[A_STR];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel stronger.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less strong.");
        }
    }
    else if ((f1 & (TR1_DEX)) || (f1 & (TR1_NEG_DEX)))
    {
        int bonus = o_ptr->stat_bonus[A_DEX];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more agile.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less agile.");
        }
    }
    else if ((f1 & (TR1_CON)) || (f1 & (TR1_NEG_CON)))
    {
        int bonus = o_ptr->stat_bonus[A_CON];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more resilient.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less resilient.");
        }
    }
    else if ((f1 & (TR1_GRA)) || (f1 & (TR1_NEG_GRA)))
    {
        int bonus = o_ptr->stat_bonus[A_GRA];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more attuned to the world.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less attuned to the world.");
        }
    }
    else if (f1 & (TR1_MEL))
    {
        int bonus = o_ptr->skill_bonus[S_MEL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more in control of your weapon.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less in control of your weapon.");
        }
    }
    else if (f1 & (TR1_ARC))
    {
        int bonus = o_ptr->skill_bonus[S_ARC];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more accurate at archery.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less accurate at archery.");
        }
    }
    else if (f1 & (TR1_STL))
    {
        int bonus = o_ptr->skill_bonus[S_STL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("Your movements become quieter.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("Your movements less quiet.");
        }
    }
    else if (f1 & (TR1_PER))
    {
        int bonus = o_ptr->skill_bonus[S_PER];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more perceptive.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less perceptive.");
        }
    }
    else if (f1 & (TR1_WIL))
    {
        int bonus = o_ptr->skill_bonus[S_WIL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more firm of will.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less firm of will.");
        }
    }
    else if (f1 & (TR1_SMT))
    {
        int bonus = o_ptr->skill_bonus[S_SMT];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel a desire to craft things with your hands.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less able to craft things.");
        }
    }
    else if (f1 & (TR1_SNG))
    {
        int bonus = o_ptr->skill_bonus[S_SNG];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You are filled with inspiration.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel a loss of inspiration.");
        }
    }

    // identify the item types that grant abilities
    else if (k_ptr->abilities > 0)
    {
        notice = true;
        msg_format("You have gained the ability '%s'.",
            b_name
                + (&b_info[ability_index(
                       k_ptr->skilltype[0], k_ptr->abilitynum[0])])
                      ->name);
    }

    // identify the special item types that grant abilities
    else if (object_has_ego(o_ptr))
    {
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        ego_item_type* e_ptr = NULL;
        if (ego_pfx && e_info[ego_pfx].abilities > 0)
            e_ptr = &e_info[ego_pfx];
        else if (ego_sfx && e_info[ego_sfx].abilities > 0)
            e_ptr = &e_info[ego_sfx];

        if (e_ptr && e_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           e_ptr->skilltype[0], e_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // identify the artefacts that grant abilities
    else if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        if (a_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           a_ptr->skilltype[0], a_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // can identify <+0> items if you already know the flavour
    else if (k_info[o_ptr->k_idx].flavor)
    {
        if (object_aware_p(o_ptr))
        {
            if (o_ptr->tval != TV_STAFF)
                notice = true;
        }
        else if (o_ptr->att > 0)
        {
            notice = true;
            msg_print("You somehow feel more accurate in combat.");
        }
        else if (o_ptr->att < 0)
        {
            notice = true;
            msg_print("You somehow feel less accurate in combat.");
        }
        else if (o_ptr->evn > 0)
        {
            notice = true;
            msg_print("You somehow feel harder to hit.");
        }
        else if (o_ptr->evn < 0)
        {
            notice = true;
            msg_print("You somehow feel more vulnerable.");
        }
        else if (o_ptr->pd > 0)
        {
            notice = true;
            msg_print("You somehow feel more protected.");
        }
    }

    if (notice)
    {
        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            /* Print the messages */
            msg_format("You recognize it as %s.", o_full_name);
        }
    }

    return;
}

extern void ident_resist(u32b flag)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;
    object_kind* k_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        k_ptr = &k_info[o_ptr->k_idx];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        {
            bool is_quiver1 = (i == INVEN_QUIVER1);
            bool is_quiver2 = (i == INVEN_QUIVER2);
            bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, f3);

            if (is_quiver1)
                continue;
            if (is_quiver2 && !is_throwing_item)
                continue;
        }

        if (o_ptr->name1 || object_has_ego(o_ptr))
        {
            // For special items and artefacts, we need to ignore the flags that
            // are basic to the object type and focus on the special/artefact
            // ones. We can do this by subtracting out the basic flags

            f1 &= ~(k_ptr->flags1);
            f2 &= ~(k_ptr->flags2);
            f3 &= ~(k_ptr->flags3);
        }

        if (!object_known_p(o_ptr))
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            if ((flag == TR2_RES_COLD) && (f2 & (TR2_RES_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the chill.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_RES_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the flame.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_RES_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the poison.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_BLEED) && (f2 & (TR2_RES_BLEED)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your bleeding is slowed by your %s.", o_short_name);
            }
            else if ((flag == TR2_RES_COLD) && (f2 & (TR2_VUL_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the chill.", o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_VUL_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the flame.", o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_VUL_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the poison.", o_short_name);
            }
            else if ((flag == TR2_RES_FEAR) && (f2 & (TR2_RES_FEAR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with courage.", o_short_name);
            }
            else if ((flag == TR2_RES_BLIND) && (f2 & (TR2_RES_BLIND)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_HALLU) && (f2 & (TR2_RES_HALLU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_CONFU) && (f2 & (TR2_RES_CONFU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_RES_STUN) && (f2 & (TR2_RES_STUN)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_FREE_ACT) && (f2 & (TR2_FREE_ACT)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s glows softly.", o_short_name);
            }
            else if ((flag == TR2_SUST_STR) && (f2 & (TR2_SUST_STR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your strength.", o_short_name);
            }
            else if ((flag == TR2_SUST_DEX) && (f2 & (TR2_SUST_DEX)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your dexterity.", o_short_name);
            }
            else if ((flag == TR2_SUST_CON) && (f2 & (TR2_SUST_CON)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your constitution.", o_short_name);
            }
            else if ((flag == TR2_SUST_GRA) && (f2 & (TR2_SUST_GRA)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your grace.", o_short_name);
            }
        }

        if (notice)
        {
            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
                msg_format("%s", effect_string);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                /* Print the messages */
                msg_format("%s", effect_string);
                msg_format("You realize that it is %s.", o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_passive(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_REGEN)) && (p_ptr->chp < p_ptr->mhp))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are recovering much faster than "
                    "usual.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_AGGRAVATE)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are enraging your enemies.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_DANGER)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are attracting more powerful enemies.",
                    sizeof(effect_string));
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("%s", effect_string);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_see_invisible(const monster_type* m_ptr)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char m_name[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_SEE_INVIS)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Get the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("You notice that you can see %s very clearly.", m_name);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_haunted(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HAUNTED)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_print("You notice that wraiths are being drawn to you.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

/*
 * Identifies a hunger or sustenance item and prints a message
 */
void ident_hunger(void)
{
    u32b f1, f2, f3;
    int i;
    bool notice = false;
    char o_full_name[80];
    char o_short_name[80];
    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HUNGER)) && (p_ptr->hunger > 0))
            {
                notice = true;
            }

            if ((f2 & (TR2_SLOW_DIGEST)) && (p_ptr->hunger < 0))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            if (f2 & (TR2_HUNGER))
                msg_print("You notice that you are growing hungry much faster "
                          "than before.");
            else if (f2 & (TR2_SLOW_DIGEST))
                msg_print("You notice that you are growing hungry slower than "
                          "before.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_f2(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f2 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

extern void ident_f3(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f3 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

void search_square(int y, int x, int dist, int searching)
{
    int score = 0;
    int difficulty = 0;
    int chest_level = 0;

    object_type* o_ptr;
    int chest_trap_present = false;

    // determine if a trap is present
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        if (!chest_trap_minigame && (o_ptr->tval == TV_CHEST)
            && (o_ptr->pval > 0)
            && object_chest_trap_flags(o_ptr) && !object_known_p(o_ptr))
        {
            chest_trap_present = true;
            chest_level = o_ptr->pval;
            break;
        }
    }

    // if searching, discover unknown adjacent squares of interest
    if (searching)
    {
        if ((dist == 1) && !(cave_info[y][x] & (CAVE_MARK)))
        {
            // mark all non-floor non-trap squares
            if (!cave_floorlike_bold(y, x))
            {
                cave_info[y][x] |= (CAVE_MARK);
            }

            // mark an object, but not the square it is in
            if (cave_o_idx[y][x] != 0)
            {
                (&o_list[cave_o_idx[y][x]])->marked = true;
            }

            /* Redraw */
            lite_spot(y, x);
        }
    }

    // if there is anything to notice...
    if ((cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
        || (cave_feat[y][x] == FEAT_SECRET) || chest_trap_present)
    {
        // give up if the square is unseen and not adjacent
        if ((dist > 1) && !(cave_info[y][x] & (CAVE_SEEN)))
            return;

        // no bonus for searching on your own square
        if (dist < 1)
        {
            dist = 1;
        }

        // Determine the base score
        score = p_ptr->skill_use[S_PER] + cave_light[y][x];

        // If using the search command give a score bonus
        if (searching)
            score += 5;

        // Determine the base difficulty
        if (chest_trap_present)
        {
            difficulty = chest_level / 2;
        }
        else
        {
            if (p_ptr->depth > 0)
            {
                difficulty = p_ptr->depth / 2;
            }
            else
            {
                difficulty = 10;
            }
        }

        // Give various penalties
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5; // can't see properly
        if (p_ptr->confused)
            difficulty += 5; // confused
        if (dist == 2)
            difficulty += 2; // distance 2
        if (dist == 3)
            difficulty += 4; // distance 3
        if (dist == 4)
            difficulty += 6; // distance 4
        if cave_trap_bold (y, x)
            difficulty += 8; // dungeon trap
        if (cave_feat[y][x] == FEAT_SECRET)
            difficulty += 10; // secret door
        if (chest_trap_present)
            difficulty += 18; // chest trap
        // if (cave_info[y][x] & (CAVE_ICKY)) difficulty
        // += 2;   // inside least/lesser/greater vaults

        // Spider bane bonus helps to find webs
        if (cave_feat[y][x] == FEAT_TRAP_WEB)
        {
            difficulty -= spider_bane_bonus();
            difficulty -= artifact_spider_bane_bonus();
        }

        /* Sometimes, notice things */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            /* Dungeon trap */
            if (cave_trap_bold(y, x))
            {
                /* Reveal the trap */
                reveal_trap(y, x);

                /* Message */
                msg_print("You have found a trap.");

                /* Disturb */
                disturb(0, 0);
            }

            /* Secret door */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Message */
                msg_print("You have found a secret door.");

                /* Pick a door */
                place_closed_door(y, x);

                /* Disturb */
                disturb(0, 0);
            }

            if (chest_trap_present)
            {
                /* Message */
                msg_print("You have discovered a trap on the chest!");

                /* Know the trap */
                object_known(o_ptr);

                /* Notice it */
                disturb(0, 0);
            }
        }
    }
}

/*
 * Search for adjacent hidden things
 */
void search(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    /* Search the adjacent grids */
    for (y = (py - 1); y <= (py + 1); y++)
    {
        for (x = (px - 1); x <= (px + 1); x++)
        {
            if ((x != px) || (y != py))
                search_square(y, x, 1, true);
        }
    }

    // also make the normal perception check
    perceive();
}

/*
 * Maybe notice hidden things nearby
 */
extern void perceive(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x, dist;

    /* Search nearby grids */
    for (y = (py - 4); y <= (py + 4); y++)
    {
        for (x = (px - 4); x <= (px + 4); x++)
        {
            if (in_bounds(y, x))
            {
                dist = distance(py, px, y, x);

                /* Search only if adjacent, player lit or permanently lit */
                if ((dist <= 1) || (p_ptr->cur_light >= dist)
                    || (cave_info[y][x] & (CAVE_GLOW)))
                {
                    /* Search only if also within four grids and in line of
                     * sight*/
                    if ((dist <= 4) && los(py, px, y, x))
                    {
                        search_square(y, x, dist, false);
                    }
                }
            }
        }
    }
}

/*
 * Check if an object is a weapon or armor that would violate the Oath of the Smith
 */
bool is_weapon_or_armor(const object_type* o_ptr)
{
    /* Check if it's a weapon */
    if (o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM || 
        o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_BOW)
        return true;
        
    /* Check if it's armor */
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL || 
        o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK || 
        o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return true;
        
    return false;
}

bool smith_oath_forbids_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return chosen_oath(OATH_SMITH) && !oath_invalid(OATH_SMITH)
        && is_weapon_or_armor(o_ptr) && !is_smithed_by_player(o_ptr);
}

bool smith_oath_confirm_break(void)
{
    char* prompt;

    if (!chosen_oath(OATH_SMITH) || oath_invalid(OATH_SMITH))
        return true;

    prompt = oath_confirmation_prompt(OATH_SMITH);
    if (!prompt || !prompt[0])
        prompt = "Are you certain you wish to break your Oath of the Smith?";

    if (!get_check_oath_multiline(prompt))
        return false;

    p_ptr->oaths_broken |= OATH_SMITH_FLAG;
    apply_oath_breaking_curse(OATH_SMITH);
    return true;
}
