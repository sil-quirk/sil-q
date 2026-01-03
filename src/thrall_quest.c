/*
 * File: thrall_quest.c
 * Purpose: Thrall quest system - alert thralls can request items from the player
 *          and provide rewards (item upgrades or artifact knowledge)
 */

#include "angband.h"
#include "externs.h"
#include "mem/alloc.h"
#include "supplies.h"
#include "thrall_quest.h"

/*
 * Probability weights for each item type by thrall race
 * Human thralls prefer practical items, elven thralls prefer finer items
 */
static const int human_thrall_weights[THRALL_QUEST_MAX] = {
    0,   /* NONE - not used */
    30,  /* SHOVEL - humans need to dig */
    25,  /* LANTERN - humans want better light */
    20,  /* HERB_HEALING - basic healing */
    10,  /* MALLORN - less common request */
    15   /* POTION_HEALING - more valuable healing */
};

static const int elf_thrall_weights[THRALL_QUEST_MAX] = {
    0,   /* NONE - not used */
    15,  /* SHOVEL - elves less interested */
    20,  /* LANTERN - practical light */
    25,  /* HERB_HEALING - elves like herbs */
    25,  /* MALLORN - elves love mallorn wood */
    15   /* POTION_HEALING - healing is valued */
};

/*
 * Probability of each reward type
 * 60% chance to upgrade a broken item (if player has one)
 * 40% chance to reveal artifact knowledge
 */
/*
 * Probability of each reward type
 * 60% chance to upgrade a broken item (if player has one)
 * 40% chance to reveal artifact knowledge
 */
#define REWARD_UPGRADE_CHANCE 60

/*
 * Tolkienistic texts for thrall interactions
 */
static const char* human_request_text = 
    "The shadows here are deep, and hope is a rare guest in these halls. Yet, I see a glimmer of light in your eyes, stranger.\n"
    "\n"
    "I am bound to this darkness, but perhaps you are not. If you possess a heart that still knows pity, I beg a small boon of you.\n"
    "\n"
    "I have need of %s. It is a small thing to one such as you, but to me, it would be a treasure beyond price.\n"
    "\n"
    "Grant me this, and I shall share what little aid I can offer.";

static const char* elf_request_text = 
    "Ai! A star in the darkness! Do my eyes deceive me, or do I see one of the Free Peoples walking these accursed paths?\n"
    "\n"
    "I am weary, kinsman, weary beyond the counting of years. The shadow lies heavy upon my fea.\n"
    "\n"
    "Yet, if you would show mercy to one who has lost all, bring me %s. In return, I shall speak to you of ancient works and secrets long forgotten by the world above.";

static const char* human_pre_give_text = 
    "You return... and I see you bear the burden I spoke of. Can it be true? Have you brought %s to this wretched place?\n"
    "\n"
    "If you would part with it, my gratitude would be boundless.";

static const char* elf_pre_give_text = 
    "You have returned, and the light of the stars seems to follow you. And... yes, I sense you carry %s.\n"
    "\n"
    "Is it for me? Will you grant this kindness to a fading spirit?";

static const char* human_thanks_text = 
    "You... you have brought it? I had not dared to hope.\n"
    "\n"
    "My thanks, stranger. You have done a kinder deed than you know. May your courage not fail you in the trials to come.\n"
    "\n"
    "Take this, and may it serve you better than it served me.";

static const char* elf_thanks_text = 
    "Elen sila lumenn' omentielvo! You have my deepest thanks.\n"
    "\n"
    "Long have I lacked such kindness in this forsaken place. May the stars shine upon your road, and may your hand be swift and your heart steadfast.\n"
    "\n"
    "Listen now, and I shall share with you the lore of old.";

static void show_thrall_dialog(monster_type* m_ptr, const char* fmt_text)
{
    char text_buf[1024];
    cptr texts[1];
    cptr title;
    byte title_color;
    
    cptr item_name = get_thrall_quest_item_name(m_ptr->thrall_quest_item);
    
    /* Format the text with item name */
    strnfmt(text_buf, sizeof(text_buf), fmt_text, item_name);
    
    texts[0] = text_buf;
    
    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
    {
        title = "An Elven Thrall";
        title_color = TERM_L_BLUE;
    }
    else
    {
        title = "A Human Thrall";
        title_color = TERM_YELLOW;
    }
    
    quest_typewriter_menu(title, texts, 1, title_color, TERM_WHITE);
}

static void thrall_request_dialog(monster_type* m_ptr)
{
    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        show_thrall_dialog(m_ptr, elf_request_text);
    else
        show_thrall_dialog(m_ptr, human_request_text);
}

static void thrall_thanks_dialog(monster_type* m_ptr)
{
    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        show_thrall_dialog(m_ptr, elf_thanks_text);
    else
        show_thrall_dialog(m_ptr, human_thanks_text);
}

/*
 * Check if a monster is an alert thrall
 */
bool is_alert_thrall(monster_type* m_ptr)
{
    return (m_ptr->r_idx == R_IDX_ALERT_HUMAN_THRALL ||
            m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL);
}

/*
 * Select a random quest item based on weights
 */
static byte select_quest_item(const int* weights)
{
    int total_weight = 0;
    int i, roll;
    
    /* Calculate total weight */
    for (i = 1; i < THRALL_QUEST_MAX; i++)
    {
        total_weight += weights[i];
    }
    
    /* Roll */
    roll = rand_int(total_weight);
    
    /* Find the selected item */
    for (i = 1; i < THRALL_QUEST_MAX; i++)
    {
        roll -= weights[i];
        if (roll < 0) return (byte)i;
    }
    
    /* Fallback */
    return THRALL_QUEST_SHOVEL;
}

/*
 * Initialize a thrall's quest (assign what item they want)
 */
void init_thrall_quest(monster_type* m_ptr)
{
    if (!is_alert_thrall(m_ptr)) return;
    
    /* Select quest item based on thrall type */
    if (m_ptr->r_idx == R_IDX_ALERT_HUMAN_THRALL)
    {
        m_ptr->thrall_quest_item = select_quest_item(human_thrall_weights);
    }
    else
    {
        m_ptr->thrall_quest_item = select_quest_item(elf_thrall_weights);
    }
    
    m_ptr->thrall_quest_completed = 0;
}

/*
 * Get the name of the item the thrall wants
 */
cptr get_thrall_quest_item_name(byte quest_item)
{
    switch (quest_item)
    {
        case THRALL_QUEST_SHOVEL:         return "a shovel";
        case THRALL_QUEST_LANTERN:        return "a brass lantern";
        case THRALL_QUEST_HERB_HEALING:   return "an identified herb of healing";
        case THRALL_QUEST_MALLORN:        return "a mallorn torch";
        case THRALL_QUEST_POTION_HEALING: return "a potion of healing";
        default:                          return "something";
    }
}

/*
 * Check if a specific item matches the quest requirement
 */
static bool item_matches_quest(object_type* o_ptr, byte quest_item)
{
    switch (quest_item)
    {
        case THRALL_QUEST_SHOVEL:
            return (o_ptr->tval == TV_DIGGING && o_ptr->sval == SV_SHOVEL);
            
        case THRALL_QUEST_LANTERN:
            return (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN);
            
        case THRALL_QUEST_HERB_HEALING:
            /* Must be identified herb of healing */
            return (o_ptr->tval == TV_FOOD && 
                    o_ptr->sval == SV_FOOD_HEALING &&
                    object_aware_p(o_ptr));
            
        case THRALL_QUEST_MALLORN:
            return (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_MALLORN);
            
        case THRALL_QUEST_POTION_HEALING:
            return (o_ptr->tval == TV_POTION && o_ptr->sval == SV_POTION_HEALING);
            
        default:
            return false;
    }
}

/*
 * Check if player has the requested item in inventory
 * Returns the inventory slot if found, -1 otherwise
 */
int player_has_thrall_quest_item(byte quest_item)
{
    int i;
    
    /* Search inventory */
    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        
        /* Skip empty slots */
        if (!o_ptr->k_idx) continue;
        
        if (item_matches_quest(o_ptr, quest_item))
        {
            return i;
        }
    }

    /* Search supplies (herbs/potions/gems cache) */
    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        if (item_matches_quest(o_ptr, quest_item))
            return SUPPLIES_INDEX + i;
    }
    
    return -1;
}

/*
 * Find a broken item in player's inventory/equipment that can be upgraded
 * Returns the slot if found, -1 otherwise
 */
int find_broken_item_to_upgrade(void)
{
    int i;
    
    /* Search inventory and equipment */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        object_kind* k_ptr;
        
        /* Skip empty slots */
        if (!o_ptr->k_idx) continue;
        
        k_ptr = &k_info[o_ptr->k_idx];
        
        /* Check if it's a damaged item */
        if (k_ptr->flags3 & TR3_DAMAGED)
        {
            return i;
        }
    }
    
    return -1;
}

/*
 * Get the upgrade target for a damaged item
 * Returns the k_idx of the replacement item, or 0 if no upgrade available
 */
static s16b get_upgrade_kind(object_type* o_ptr)
{
    /* Rusty Helm -> Helm */
    if (o_ptr->tval == TV_HELM && o_ptr->sval == SV_RUSTY_HELM)
    {
        return lookup_kind(TV_HELM, SV_HELM);
    }
    
    /* Shabby Boots -> Leather Boots */
    if (o_ptr->tval == TV_BOOTS && o_ptr->sval == SV_PAIR_OF_SHABBY_BOOTS)
    {
        return lookup_kind(TV_BOOTS, SV_PAIR_OF_LEATHER_BOOTS);
    }
    
    /* Broken Shield -> Round Shield */
    if (o_ptr->tval == TV_SHIELD && o_ptr->sval == SV_BROKEN_SHIELD)
    {
        return lookup_kind(TV_SHIELD, SV_ROUND_SHIELD);
    }
    
    return 0;
}

static bool item_tester_hook_broken_item(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return (k_info[o_ptr->k_idx].flags3 & TR3_DAMAGED) ? true : false;
}

static bool choose_broken_item_to_upgrade(int* out_slot)
{
    int slot = -1;

    if (!out_slot)
        return false;

    item_tester_hook = item_tester_hook_broken_item;

    if (!get_item(&slot, "Repair which item? ",
            "You have nothing broken to mend.", (USE_EQUIP | USE_INVEN)))
    {
        item_tester_hook = NULL;
        return false;
    }

    item_tester_hook = NULL;

    if (slot < 0)
        return false;

    *out_slot = slot;
    return true;
}

/*
 * Upgrade a broken item to its normal version, keeping special properties
 */
bool upgrade_broken_item(int slot)
{
    object_type* o_ptr = &inventory[slot];
    object_kind* old_k_ptr;
    object_kind* new_k_ptr;
    s16b new_k_idx;
    char old_name[80];
    char new_name[80];
    int dd, ds, pd, ps;
    int dd_delta, ds_delta, pd_delta, ps_delta;
    
    /* Paranoia */
    if (!o_ptr->k_idx) return false;
    
    old_k_ptr = &k_info[o_ptr->k_idx];
    
    /* Must be damaged */
    if (!(old_k_ptr->flags3 & TR3_DAMAGED)) return false;
    
    /* Get upgrade target */
    new_k_idx = get_upgrade_kind(o_ptr);
    if (!new_k_idx) return false;
    
    new_k_ptr = &k_info[new_k_idx];

    /* Keep current values, then add base delta after changing kind */
    dd = o_ptr->dd;
    ds = o_ptr->ds;
    pd = o_ptr->pd;
    ps = o_ptr->ps;

    dd_delta = (int)new_k_ptr->dd - (int)old_k_ptr->dd;
    ds_delta = (int)new_k_ptr->ds - (int)old_k_ptr->ds;
    pd_delta = (int)new_k_ptr->pd - (int)old_k_ptr->pd;
    ps_delta = (int)new_k_ptr->ps - (int)old_k_ptr->ps;
    
    /* Remember old name */
    object_desc(old_name, sizeof(old_name), o_ptr, true, 0);
    
    /* Upgrade the item - keep most properties, change kind */
    o_ptr->k_idx = new_k_idx;
    o_ptr->tval = new_k_ptr->tval;
    o_ptr->sval = new_k_ptr->sval;
    
    /* Increase basic stats by the base delta between broken and real items */
    o_ptr->dd = (byte)MIN(255, MAX(0, dd + dd_delta));
    o_ptr->ds = (byte)MIN(255, MAX(0, ds + ds_delta));
    o_ptr->pd = (byte)MIN(255, MAX(0, pd + pd_delta));
    o_ptr->ps = (byte)MIN(255, MAX(0, ps + ps_delta));
    
    /* Update weight */
    o_ptr->weight = new_k_ptr->weight;
    
    /* Mark as known */
    object_known(o_ptr);
    
    /* Get new name */
    object_desc(new_name, sizeof(new_name), o_ptr, true, 0);
    
    /* Message */
    char dialog_text[1024];
    strnfmt(dialog_text, sizeof(dialog_text),
        "The thrall takes your %s and works upon it with surprising skill, murmuring words of power.\n\n"
        "A light shines from the object...\n\n"
        "It transforms into %s!", old_name, new_name);
        
    cptr texts[1] = { dialog_text };
    quest_typewriter_menu("Restoration", texts, 1, TERM_YELLOW, TERM_WHITE);
    
    /* Update display */
    p_ptr->update |= (PU_BONUS);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
    
    return true;
}

/*
 * Reveal a random unknown artifact's description
 * Returns true if an artifact was revealed
 */
bool reveal_random_artifact(void)
{
    int i, count;
    int* candidates;
    int selected;
    artefact_type* a_ptr;
    object_type temp_obj;
    char o_name[120];
    
    /* Count unrevealed artifacts */
    count = 0;
    for (i = 1; i < z_info->art_max; i++)
    {
        a_ptr = &a_info[i];
        
        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;
        
        /* Skip already found artefacts */
        if (a_ptr->found_num > 0)
            continue;

        /* Skip already revealed artefacts */
        if (a_ptr->seen & ART_SEEN_REVEALED)
            continue;

        /* Skip the later versions of the Iron Crown */
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1) || (i == ART_MORGOTH_2))
            continue;

        /* Skip the special smithing template artefacts */
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;
        
        count++;
    }
    
    /* No unknown artifacts */
    if (count == 0)
    {
        msg_print("The thrall has nothing more to tell you of artefacts.");
        return false;
    }
    
    /* Build list of candidates */
    candidates = mem_alloc_array(count, int);
    count = 0;
    for (i = 1; i < z_info->art_max; i++)
    {
        a_ptr = &a_info[i];
        
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;
        if (a_ptr->found_num > 0)
            continue;
        if (a_ptr->seen & ART_SEEN_REVEALED)
            continue;
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1) || (i == ART_MORGOTH_2))
            continue;
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;
        
        candidates[count++] = i;
    }
    
    /* Select random artifact */
    selected = candidates[rand_int(count)];
    mem_free(candidates);
    
    a_ptr = &a_info[selected];

    /* Reveal in the knowledge menu */
    a_ptr->seen |= ART_SEEN_REVEALED;
    
    /* Create a temporary object to get full name */
    object_wipe(&temp_obj);
    {
        s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
        if (k_idx > 0)
        {
            object_prep(&temp_obj, k_idx);
            temp_obj.name1 = selected;
            object_into_artefact(&temp_obj, a_ptr);
            object_known(&temp_obj);
            temp_obj.ident |= IDENT_SPOIL;
        }
    }
    
    /* Get artifact name */
    object_desc(o_name, sizeof(o_name), &temp_obj, true, 0);
    
    /* Display the revelation */
    char dialog_text[2048];
    char quote_text[1024] = "";
    
    /* Quote some of the artefact description, if it has one */
    if (a_ptr->text)
    {
        const char* text = a_text + a_ptr->text;
        int lines = 0;

        while (*text && lines < 3)
        {
            char line[160];
            size_t len = 0;

            while (text[len] && text[len] != '\n')
                len++;

            if (len > 0)
            {
                size_t copy_len = len;
                if (copy_len > sizeof(line) - 3)
                    copy_len = sizeof(line) - 3;

                line[0] = '"';
                memcpy(line + 1, text, copy_len);
                line[1 + copy_len] = '"';
                line[2 + copy_len] = '\0';
                
                SDL_strlcat(quote_text, line, sizeof(quote_text));
                SDL_strlcat(quote_text, "\n", sizeof(quote_text));
                
                lines++;
            }

            if (!text[len])
                break;
            text += len + 1;
        }
    }
    
    strnfmt(dialog_text, sizeof(dialog_text), 
        "The thrall whispers ancient knowledge to you...\n\n"
        "You learn of %s!\n\n"
        "%s", o_name, quote_text);
        
    cptr texts[1] = { dialog_text };
    quest_typewriter_menu("Ancient Knowledge", texts, 1, TERM_L_BLUE, TERM_WHITE);

    if (get_check("Study this lore now? "))
    {
        desc_art_fake(selected);
    }
    
    return true;
}

/*
 * Complete the thrall's quest - consume the item and give reward
 */
void complete_thrall_quest(monster_type* m_ptr, int item_slot)
{
    object_type* o_ptr;
    char o_name[80];
    char m_name[80];
    int broken_slot;
    bool do_upgrade;
    bool from_supplies = false;
    int supply_idx = -1;

    if (item_slot >= SUPPLIES_INDEX)
    {
        from_supplies = true;
        supply_idx = item_slot - SUPPLIES_INDEX;
        o_ptr = supplies_entry_at(supply_idx);
    }
    else
    {
        o_ptr = &inventory[item_slot];
    }

    /* Paranoia */
    if (!o_ptr || !o_ptr->k_idx)
        return;
    
    /* Get names */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 0);
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
    
    /* Thank the player */
    thrall_thanks_dialog(m_ptr);
    
    /* Consume one item */
    if (from_supplies)
    {
        supplies_consume_quantity(supply_idx, 1);
    }
    else
    {
        inven_item_increase(item_slot, -1);
        inven_item_optimize(item_slot);
    }
    
    /* Mark quest as completed */
    m_ptr->thrall_quest_completed = 1;
    
    /* Determine reward */
    broken_slot = find_broken_item_to_upgrade();
    
    /* If player has a broken item, high chance to upgrade it */
    if (broken_slot >= 0 && rand_int(100) < REWARD_UPGRADE_CHANCE)
    {
        do_upgrade = true;
    }
    else
    {
        do_upgrade = false;
    }
    
    if (do_upgrade)
    {
        char dialog_text[1024];
        strnfmt(dialog_text, sizeof(dialog_text), 
            "%^s examines your gear with a knowing eye.\n\n"
            "\"I see broken steel that remembers its former glory. Shall I attempt to restore it?\"", m_name);
        cptr texts[1] = { dialog_text };
        quest_typewriter_menu("Offer of Restoration", texts, 1, TERM_YELLOW, TERM_WHITE);

        if (choose_broken_item_to_upgrade(&broken_slot))
        {
            if (!upgrade_broken_item(broken_slot))
            {
                /* Fallback to artifact knowledge if upgrade fails */
                reveal_random_artifact();
            }
        }
        else
        {
            reveal_random_artifact();
        }
    }
    else
    {
        reveal_random_artifact();
    }
    
    /* Update windows */
    p_ptr->window |= (PW_INVEN);
}

/*
 * Handle interaction when player bumps into an alert thrall
 * Returns true if an action was taken
 */
bool handle_thrall_interaction(monster_type* m_ptr)
{
    char m_name[80];
    int item_slot;
    
    /* Only handle alert thralls */
    if (!is_alert_thrall(m_ptr)) return false;
    
    /* Get monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
    
    /* If quest not initialized, initialize it */
    if (m_ptr->thrall_quest_item == THRALL_QUEST_NONE)
    {
        init_thrall_quest(m_ptr);
    }
    
    /* Quest already completed */
    if (m_ptr->thrall_quest_completed)
    {
        msg_format("%^s looks upon you with gratitude.", m_name);
        return true;
    }
    
    /* Check if player has the item */
    item_slot = player_has_thrall_quest_item(m_ptr->thrall_quest_item);
    
    if (item_slot >= 0)
    {
        /* Player has the item - offer to give it */
        
        /* Show Pre-Give Dialog */
        if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
            show_thrall_dialog(m_ptr, elf_pre_give_text);
        else
            show_thrall_dialog(m_ptr, human_pre_give_text);

        char prompt[200];
        strnfmt(prompt, sizeof(prompt), 
            "Give %s? ",
            get_thrall_quest_item_name(m_ptr->thrall_quest_item));
        
        if (get_check(prompt))
        {
            complete_thrall_quest(m_ptr, item_slot);
        }
        else
        {
            msg_format("%^s looks disappointed.", m_name);
        }
    }
    else
    {
        /* Player doesn't have the item - make the request */
        thrall_request_dialog(m_ptr);
    }
    
    return true;
}
