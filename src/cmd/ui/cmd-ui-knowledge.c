#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

static void desc_obj_fake(int k_idx);
static cptr equipment_slot_text(int slot);
static int equipment_menu_compare_slots(int selected_slot, int slots[],
    int max_slots);
static object_type* equipment_menu_floor_item_for_object(
    const object_type* ref);

int g_knowledge_last_page = KNOWLEDGE_PAGE_ARTEFACTS;

static bool floor_entry_perform_action(int floor_idx,
    supply_floor_action floor_action, int selected_slot)
{
    object_type* o_ptr;

    if (floor_idx <= 0 || floor_idx >= o_max)
        return false;

    o_ptr = &o_list[floor_idx];
    if (!o_ptr->k_idx)
        return false;

    switch (floor_action)
    {
    case SUPPLY_FLOOR_ACTION_USE:
        do_cmd_use_item_by_index(0 - floor_idx);
        return true;

    case SUPPLY_FLOOR_ACTION_WIELD:
        if (selected_slot >= INVEN_WIELD && selected_slot < INVEN_TOTAL)
            do_cmd_wield_to_slot(o_ptr, 0 - floor_idx, selected_slot);
        else
            do_cmd_wield(o_ptr, 0 - floor_idx);
        return true;

    case SUPPLY_FLOOR_ACTION_DEFAULT:
    default:
        py_pickup_aux(floor_idx);
        return true;
    }
}

static bool supplies_menu_use_entry(supply_list_entry* entry,
    supply_floor_action floor_action)
{
    if (!entry)
        return false;

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
        return floor_entry_perform_action(entry->floor_idx, floor_action, -1);

    if (entry->preset_idx >= 0)
        return do_cmd_jewelry_preset_apply(entry->preset_idx);

    if (entry->supply_idx < 0)
    {
        if (entry->equipped && entry->equip_idx == INVEN_LITE)
        {
            msg_print("That light source is already equipped.");
        }
        return false;
    }

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    supplies_begin_action(entry->supply_idx);

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        do_cmd_eat_food(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_POTION:
        do_cmd_quaff_potion(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_STAFF:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_GEM:
    {
        bool defer_self_knowledge =
            (o_ptr->sval == SV_GEM_SELF_KNOWLEDGE);

        if (defer_self_knowledge)
            self_knowledge_defer_display_push();
        do_cmd_use_gem(o_ptr, SUPPLIES_INDEX);
        if (defer_self_knowledge)
            self_knowledge_defer_display_pop();
        break;
    }
    case TV_FLASK:
        do_cmd_refuel_lamp(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_LIGHT:
        do_cmd_wield(o_ptr, SUPPLIES_INDEX);
        break;
    default:
        supplies_end_action();
        bell("Cannot use that item here!");
        msg_print("Cannot use that item here.");
        return false;
    }

    supplies_end_action();
    return true;
}

static bool supplies_menu_drop_entry(supply_list_entry* entry)
{
    if (!entry)
        return false;

    if (entry->preset_idx >= 0)
    {
        bell("Nothing to drop here.");
        msg_print("Use c to clear a jewelry set.");
        return false;
    }

    if (entry->supply_idx < 0)
    {
        if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        {
            do_cmd_drop_item_by_index(entry->equip_idx);
            return true;
        }
        return false;
    }

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    int max_amt = o_ptr->number;
    if (max_amt <= 0)
        return false;

    int actual_amt = get_quantity(NULL, max_amt);
    if (actual_amt <= 0)
        return false;
    supplies_begin_action(entry->supply_idx);
    bool dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
    supplies_end_action();

    if (dropped)
        handle_stuff();

    return dropped;
}

/*
 * Show an object's description, comparing it against whatever is already
 * equipped in the slot(s) it would occupy ("the item underneath"). Mirrors the
 * comparison the Equipped/Inventory panes do via
 * equipment_menu_show_entry_description, so the Supplies 'x' preview lines a
 * candidate light/jewelry up against the currently worn item.
 */
static bool supply_object_show_with_compare(object_type* o_ptr,
    int selected_slot, bool overlay)
{
    int compare_slots[2];
    int compare_count;
    const object_type* objects[3];
    const char* headings[3];
    char heading_texts[3][32];
    int count = 0;
    object_type* floor_ptr;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    compare_count = equipment_menu_compare_slots(selected_slot, compare_slots,
        2);

    strnfmt(heading_texts[count], sizeof(heading_texts[count]),
        "Selected item");
    headings[count] = heading_texts[count];
    objects[count++] = o_ptr;

    for (int i = 0; i < compare_count && count < 3; i++)
    {
        int slot = compare_slots[i];

        /* The selected entry is itself the equipped item; nothing to
         * compare against. */
        if (&inventory[slot] == o_ptr)
            continue;

        strnfmt(heading_texts[count], sizeof(heading_texts[count]),
            "%s", equipment_slot_text(slot));
        headings[count] = heading_texts[count];
        objects[count++] = &inventory[slot];
    }

    /* Shortcut: if standing on a comparable item, line it up too. */
    floor_ptr = equipment_menu_floor_item_for_object(o_ptr);
    if (floor_ptr && count < 3)
    {
        strnfmt(heading_texts[count], sizeof(heading_texts[count]),
            "On floor");
        headings[count] = heading_texts[count];
        objects[count++] = floor_ptr;
    }

    if (count <= 1)
    {
        const object_type* single[1] = { o_ptr };

        if (overlay)
            return object_info_overlay_show_multi(single, NULL, 1);
        object_info_screen(o_ptr);
        return true;
    }

    if (overlay)
        return object_info_overlay_show_multi(objects, headings, count);
    object_info_screen_multi(objects, headings, count);
    return true;
}

static bool supplies_menu_show_entry_description(supply_list_entry* entry,
    bool overlay)
{
    if (!entry)
        return false;

    if (entry->preset_idx >= 0)
    {
        const object_type* objects[JEWELRY_PRESET_SLOT_MAX];
        const char* headings[JEWELRY_PRESET_SLOT_MAX] = {
            "Left ring", "Right ring", "Amulet"
        };
        int count = 0;

        if (!jewelry_preset_is_set(entry->preset_idx))
        {
            if (!overlay)
            {
                bell("That jewelry set is empty.");
                msg_print("That jewelry set is empty.");
            }
            return false;
        }

        for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
        {
            const object_type* o_ptr =
                jewelry_preset_object(entry->preset_idx, slot);

            if (!o_ptr || !o_ptr->k_idx)
                continue;

            objects[count] = o_ptr;
            headings[count] = headings[slot];
            count++;
        }

        if (count > 0)
        {
            if (overlay)
                return object_info_overlay_show_multi(objects, headings, count);
            object_info_screen_multi(objects, headings, count);
            return true;
        }

        if (!overlay)
        {
            bell("That jewelry set is empty.");
            msg_print("That jewelry set is empty.");
        }
        return false;
    }

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        object_type* o_ptr = &inventory[entry->equip_idx];
        if (!overlay)
            (void)player_try_identify_smithing_object_on_examine(o_ptr, true);
        if (overlay)
        {
            const object_type* objects[1] = { o_ptr };
            return object_info_overlay_show_multi(objects, NULL, 1);
        }
        object_info_screen(o_ptr);
        return true;
    }

    if (entry->supply_idx >= 0)
    {
        object_type* o_ptr = supplies_entry_at(entry->supply_idx);
        if (o_ptr)
            return supply_object_show_with_compare(o_ptr, wield_slot(o_ptr),
                overlay);
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        object_type* o_ptr = &inventory[entry->item_idx];
        if (!overlay)
            (void)player_try_identify_smithing_object_on_examine(o_ptr, false);
        return supply_object_show_with_compare(o_ptr, wield_slot(o_ptr),
            overlay);
    }

    if (entry->k_idx >= 0)
    {
        object_kind* k_ptr = &k_info[entry->k_idx];
        if (k_ptr->aware)
        {
            if (overlay)
            {
                object_type object_type_body;
                object_type* i_ptr = &object_type_body;

                object_wipe(i_ptr);
                object_prep(i_ptr, entry->k_idx);
                apply_magic_fake(i_ptr);
                i_ptr->ident |= IDENT_KNOWN;
                return supply_object_show_with_compare(i_ptr,
                    wield_slot(i_ptr), true);
            }
            desc_obj_fake(entry->k_idx);
            return true;
        }

        if (!overlay)
        {
            bell("You have not identified that yet.");
            msg_print("You have not identified that yet.");
        }
        return false;
    }

    if (!overlay)
    {
        bell("Nothing to recall.");
        msg_print("Nothing to recall.");
    }
    return false;
}

static bool supplies_menu_overlay_entry(supply_list_entry* entry)
{
    return supplies_menu_show_entry_description(entry, true);
}

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Food",
    "Potions",
    "Gems",
    "Lights/Oil",
    "Jewelry Sets",
    "Supply",
    NULL
};

void do_cmd_note(char* note, int what_depth)
{
    char buf[120];
    char turn_string[16];

    int length, length_info;
    char info_note[40];
    char depths[10];

    /* Default */
    SDL_strlcpy(buf, "", sizeof(buf));

    /* If a note is passed, use that, otherwise accept user input. */
    if (streq(note, ""))
    {
        if (!term_get_string("Note: ", buf, 57))
            return;
    }
    else
    {
        SDL_strlcpy(buf, note, sizeof(buf));
    }

    /* Ignore empty notes */
    if (!buf[0] || (buf[0] == ' '))
        return;

    /* write it to the notes file */

    /*Artefacts use depth artefact created.  All others use player depth.*/

    /*get depth for recording\
     */
    if (what_depth == 0)
    {
        SDL_strlcpy(depths, "   Gates", sizeof(depths));
    }
    else if (what_depth == CHEST_LEVEL)
    {
        SDL_strlcpy(depths, "   Chest", sizeof(depths));
    }
    else if (what_depth == SKELETON_LEVEL)
    {
        SDL_strlcpy(depths, "   Skeleton", sizeof(depths));
    }
    else
    {
        comma_number(depths, what_depth * 50);
        strnfmt(depths, sizeof(depths), "%5s ft", depths);
    }

    comma_number(turn_string, playerturn);

    /* Make preliminary part of note */
    strnfmt(info_note, sizeof(info_note), "%7s  %s   ", turn_string, depths);

    /*write the info note*/
    SDL_strlcat(notes_buffer, info_note, sizeof(notes_buffer));

    /*get the length of the notes*/
    length_info = strlen(info_note);
    length = strlen(buf);

    /*break up long notes*/
    if ((length + length_info) > LINEWRAP)
    {
        bool keep_going = true;
        int startpoint = 0;
        int endpoint, n;

        while (keep_going)
        {
            /*don't print more than the set linewrap amount*/
            endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;

            /*find a breaking point*/
            while (true)
            {
                /*are we at the end of the line?*/
                if (endpoint >= length)
                {
                    /*print to the end*/
                    endpoint = length;
                    keep_going = false;
                    break;
                }

                /* Mark the most recent space or dash in the string */
                else if ((buf[endpoint] == ' ') || (buf[endpoint] == '-'))
                    break;

                /*no spaces in the line, so break in the middle of text*/
                else if (endpoint == startpoint)
                {
                    endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;
                    break;
                }

                /* check previous char */
                endpoint--;
            }

            /*make a continued note if applicable*/
            if (startpoint)
                SDL_strlcat(
                    notes_buffer, "                    ", sizeof(notes_buffer));

            /* Write that line to file */
            for (n = startpoint; n <= endpoint; n++)
            {
                char ch;

                /* Ensure the character is printable */
                ch = (isprint(buf[n]) ? buf[n] : ' ');

                /* Write out the character */
                SDL_strlcat(notes_buffer, format("%c", ch), sizeof(notes_buffer));
            }

            /*break the line*/
            SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

            /*prepare for the next line*/
            startpoint = endpoint + 1;
        }
    }

    /* Add note to buffer */
    else
    {
        SDL_strlcat(notes_buffer, format("%s\n", buf), sizeof(notes_buffer));
    }
}

/*
 * Mention the current version
 */
void do_cmd_version(void)
{
    /* Silly message - use msg_print so message is shown immediately */
    char verbuf[128];
    strnfmt(verbuf, sizeof(verbuf), "You are playing %s %s.  Type '?' for more info.",
        VERSION_NAME, VERSION_STRING);
    msg_print(verbuf);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_feeling_text[LEV_THEME_HEAD]
    = { "Looks like any other level.",
          "You feel there is something special about this level.",
          "You have a superb feeling about this level.",
          "You have an excellent feeling...", "You have a very good feeling...",
          "You have a good feeling...", "You feel strangely lucky...",
          "You feel your luck is turning...",
          "You like the look of this place...",
          "This level can't be all bad...", "What a boring place..." };

/*
 * Note that "feeling" is set to zero unless some time has passed.
 * Note that this is done when the level is GENERATED, not entered.
 */
void do_cmd_feeling(void)
{
    /* No useful feeling on the surface */
    if (!p_ptr->depth)
    {
        msg_print("You stand once again upon the surface. Freedom awaits.");
        return;
    }

    /* No useful feelings until enough time has passed */
    if (!do_feeling)
    {
        msg_print("You are still uncertain about this level...");
        return;
    }

    /* Display the feeling */
    else
        msg_print(do_cmd_feeling_text[feeling]);
}

/*
 * Array of feeling strings
 */
static cptr do_cmd_challenge_text[14]
    = { "challenges you from beyond the grave!",
          "thunders 'Prove worthy of your traditions - or die ashamed!'.",
          "desires to test your mettle!",
          "has risen from the dead to test you!",
          "roars 'Fight, or know yourself for a coward!'.",
          "summons you to a duel of life and death!",
          "desires you to know that you face a mighty champion of yore!",
          "demands that you prove your worthiness in combat!",
          "calls you unworthy of your ancestors!",
          "challenges you to a deathmatch!", "walks Middle-Earth once more!",
          "challenges you to demonstrate your prowess!",
          "demands you prove yourself here and now!",
          "asks 'Can ye face the best of those who came before?'." };

/*
 * Personalize, randomize, and announce the challenge of a player ghost. -LM-
 */
void ghost_challenge(void)
{
    monster_race* r_ptr = &r_info[r_ghost];

    /*paranoia*/
    /* Check there is a name/ghost first */
    if (ghost_name[0] == '\0')
    {
        /*there wasn't a ghost*/
        bones_selector = 0;
        return;
    }

    msg_format("%^s, the %^s %s", ghost_name, r_name + r_ptr->name,
        do_cmd_challenge_text[rand_int(14)]);

    message_flush();
}

/*display the notes file*/
void do_cmd_knowledge_notes(void) { show_buffer(notes_buffer, 0); }

/*
 * Display oath status information
 */
void do_cmd_knowledge_oaths(void)
{
    SDL_IOStream* fff;
    char file_name[1024];

    /* Temporary file */
    if (!path_temp(file_name, sizeof(file_name)))
        return;

    /* Open a new file */
    fff = sdl_fopen(file_name, "w");

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Scan the oaths */
    SDL_IOprintf(fff, "Oath Status\n\n");

    /* Check current character oath */
    if (p_ptr->have_ability[S_SPC][SPC_OATH_MERCY])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_MERCY])
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Mercy (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SILENCE])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE])
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Silence (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_IRON])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_IRON])
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Iron (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SMITH])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SMITH])
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of the Smith (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_VALOROUS])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS])
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Active)\n\n");
        else
            SDL_IOprintf(fff, "Current Oath: Oath of Valorous Heart (Broken)\n\n");
    }
    else
    {
        SDL_IOprintf(fff, "Current Oath: None\n\n");
    }

    /* Display metarun oath status */
    SDL_IOprintf(fff, "Metarun Oath Status:\n");

    /* Check unlocked oaths */
    bool has_unlocked = false;
    if (oath_unlocked(OATH_MERCY))
    {
        SDL_IOprintf(fff, "  Oath of Mercy: Unlocked");
        if (oath_banned(OATH_MERCY))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }

    if (oath_unlocked(OATH_SILENCE))
    {
        SDL_IOprintf(fff, "  Oath of Silence: Unlocked");
        if (oath_banned(OATH_SILENCE))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }

    if (oath_unlocked(OATH_IRON))
    {
        SDL_IOprintf(fff, "  Oath of Iron: Unlocked");
        if (oath_banned(OATH_IRON))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }

    if (oath_unlocked(OATH_SMITH))
    {
        SDL_IOprintf(fff, "  Oath of the Smith: Unlocked");
        if (oath_banned(OATH_SMITH))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }

    if (oath_unlocked(OATH_VALOROUS))
    {
        SDL_IOprintf(fff, "  Oath of Valorous Heart: Unlocked");
        if (oath_banned(OATH_VALOROUS))
            SDL_IOprintf(fff, " (Banned this run)");
        SDL_IOprintf(fff, "\n");
        has_unlocked = true;
    }

    if (!has_unlocked)
    {
        SDL_IOprintf(fff, "  No oaths unlocked yet.\n");
        SDL_IOprintf(fff, "  Complete Valar quests to unlock new oaths.\n");
    }

    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Oath Status", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Description of each object group.
 */
static cptr object_group_text[]
    = { "Herbs", "Potions", "Rings", "Amulets", "Staves", "Horns", "Swords",
          "Axes & Polearms", "Blunt Weapons", "Diggers", "Bows",
          //	"Arrows",
          "Light Sources", "Soft Armour", "Mail", "Shields", "Cloaks", "Gloves",
          "Helms", "Crowns", "Boots", "Chests", NULL };

/*
 * TVALs of items in each group
 */
static byte object_group_tval[] = { TV_FOOD, TV_POTION, TV_RING, TV_AMULET,
    TV_STAFF, TV_HORN, TV_SWORD, TV_POLEARM, TV_HAFTED, TV_DIGGING, TV_BOW,
    //	TV_ARROW,
    TV_LIGHT, TV_SOFT_ARMOR, TV_MAIL, TV_SHIELD, TV_CLOAK, TV_GLOVES, TV_HELM,
    TV_CROWN, TV_BOOTS, TV_CHEST, 0 };

/*
 * Build a list of objects indexes in the given group. Return the number
 * of objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 *  (Incorporates some code from jdh)
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
    int i, j, k, object_cnt = 0;
    int max_sval = -1;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /* Check every object */
    for (i = 0; i < z_info->k_max; i++)
    {
        /* Access the object type */
        object_kind* k_ptr = &k_info[i];

        /*used to check for allocation*/
        k = 0;

        /* Skip empty objects */
        if (!k_ptr->name)
            continue;

        /* Skip items with no distribution (including special artefacts) */
        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /*add the rarity, if there is one*/
            k += k_ptr->chance[j];
        }
        /*not in allocation table*/
        if (!(k))
            continue;

        /* Require objects ever seen*/
        // if (!(k_ptr->aware && k_ptr->everseen)) continue;
        if (!(k_ptr->everseen))
            continue;

        /* Check for object in the group */
        if (k_ptr->tval == group_tval)
        {
            /* Save the highest sval in the group for later */
            if (k_ptr->sval > max_sval)
            {
                max_sval = k_ptr->sval;
            }

            /* Add the object type */
            if (object_idx)
            {
                object_idx[object_cnt].type = OBJ_NORMAL;
                object_idx[object_cnt].idx = i;
            }

            object_cnt++;
        }
    }

    /* Add special items to the list */
    /* Skip this part if we don't know any normal items */
    for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
    {
        /* Access the object type */
        ego_item_type* e_ptr = &e_info[i];

        /* Skip empty objects */
        if (!e_ptr->name)
            continue;

        /* Require objects ever seen*/
        if (!(e_ptr->everseen))
            continue;

        /* Check for object in the group */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            if (e_ptr->tval[j] == group_tval)
            {
                if (object_idx)
                {
                    object_idx[object_cnt].type = OBJ_SPECIAL;
                    object_idx[object_cnt].idx = -1;
                    object_idx[object_cnt].e_idx = i;
                    object_idx[object_cnt].tval = group_tval;
                    object_idx[object_cnt].sval = -1;
                }
                object_cnt++;

                break;
            }
        }
    }

    /* Terminate the list */
    if (object_idx)
        object_idx[object_cnt].type = OBJ_NONE;

    /* Return the number of object types */
    return object_cnt;
}

/*
 * Build a list of artefact indexes in the given group. Return the number
 * of eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
    int i, object_cnt = 0;
    bool* okay;
    bool know_all = cheat_know;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /*make a list of artefacts not found*/
    /* Allocate the "object_idx" array */
    okay = mem_alloc_array(z_info->art_max, bool);

    /* Default first,  */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        bool revealed = (a_ptr->seen & ART_SEEN_REVEALED) != 0;

        /*start with false*/
        okay[i] = false;

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Skip "unfound" artefacts, unless in wizard mode, cheating,
         * or revealed via quests/lore. */
        if (!know_all && !p_ptr->wizard && !a_ptr->found_num && !revealed)
            continue;

        /* Skip "ungenerated" artefacts, unless cheating or quest-revealed. */
        if (!know_all && !revealed && !a_ptr->cur_num)
            continue;

        /* Skip the later versions of the Iron Crown */
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1)
            || (i == ART_MORGOTH_2))
            continue;

        /* Skip the special smithing template artefacts */
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;

        /*assume all created artefacts are good at this point*/
        okay[i] = true;
    }

    /* Finally, go through the list of artefacts and categorize the good ones */
    for (i = 0; i < z_info->art_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip empty artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Require artefacts ever seen*/
        if (okay[i] == false)
            continue;

        /* Check for race in the group */
        if (a_ptr->tval == group_tval)
        {
            /* Add the race */
            object_idx[object_cnt++] = i;
        }
    }

    /* Terminate the list */
    object_idx[object_cnt] = 0;

    /*clear the array*/
    mem_free_null(okay);

    /* Return the number of races */
    return object_cnt;
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    return supplies_group_matches_kind(group, tval, sval);
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    return supplies_group_matches_object(group, o_ptr);
}

static int supply_group_uniform_weight(int group_idx)
{
    int weight = -1;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (weight < 0)
            weight = k_ptr->weight;
        else if (weight != k_ptr->weight)
            return -1;
    }

    return weight;
}

static void describe_supply_group_status(int group_idx, int term_wid,
    char* buf, size_t len)
{
    int weight;

    if (!buf || len == 0)
        return;

    buf[0] = '\0';

    switch (group_idx)
    {
    case SUPPLY_GROUP_HERBS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All herbs weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_FOOD:
        SDL_strlcpy(buf, "Food weight varies; each row shows per-item weight.",
            len);
        break;
    case SUPPLY_GROUP_POTIONS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All potions weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_GEMS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All gems weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_LIGHTS:
        SDL_strlcpy(buf,
            "Oil slots: lamp 2, flask 1 (max 4).",
            len);
        break;
    case SUPPLY_GROUP_SUPPLY:
        SDL_strlcpy(buf,
            "All supply-cache entries. Press a listed letter to use that item.",
            len);
        break;
    case SUPPLY_GROUP_JEWELRY_PRESETS:
        if (term_wid <= 42)
            SDL_strlcpy(buf, "s save, u equip, c clear, Alt+1-5", len);
        else if (term_wid <= 62)
            SDL_strlcpy(buf, "s save, u equip, c clear, Alt+1-5", len);
        else
            SDL_strlcpy(buf,
                "Use s to save current jewelry, u or Alt+1-5 to equip, c to clear.",
                len);
        break;
    default:
        break;
    }
}

static void build_supply_weight_summary(char* buf, size_t buflen, int term_wid,
    int used_weight, int max_weight, int light_weight, int light_item_weight,
    int light_oil_weight, int lamp_oil, int lamp_capacity, int oil_slots,
    int oil_slot_capacity)
{
    char temp[128];

    if (!buf || buflen == 0)
        return;

    if (term_wid < 1)
        term_wid = 80;

    strnfmt(temp, sizeof(temp),
        "Supply: %d.%1d/%d.%1d lb  Light: %d.%1d lb (%d.%1d items + %d.%1d oil)  Oil: %d/%d  Slots: %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp),
        "Sup %d.%1d/%d.%1d lb  Lgt %d.%1d lb (%d.%1d itm + %d.%1d oil)  Oil %d/%d  Slots %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp),
        "Sup %d.%1d/%d.%1d  Lgt %d.%1d (%d.%1d+%d.%1d)  Oil %d/%d  Sl %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        light_item_weight / 10, light_item_weight % 10,
        light_oil_weight / 10, light_oil_weight % 10,
        lamp_oil, lamp_capacity, oil_slots, oil_slot_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp), "Sup %d.%1d/%d.%1d  Lgt %d.%1d  Oil %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        lamp_oil, lamp_capacity);
    if ((int)strlen(temp) <= term_wid)
    {
        SDL_strlcpy(buf, temp, buflen);
        return;
    }

    strnfmt(temp, sizeof(temp), "S %d.%1d/%d.%1d  L %d.%1d  O %d/%d",
        used_weight / 10, used_weight % 10,
        max_weight / 10, max_weight % 10,
        light_weight / 10, light_weight % 10,
        lamp_oil, lamp_capacity);
    SDL_strlcpy(buf, temp, buflen);
}

static void strip_supply_light_turns_suffix(char* name)
{
    char* suffix;

    if (!name)
        return;

    suffix = strstr(name, " (");
    if (suffix && strstr(suffix, " turns)"))
        *suffix = '\0';
}

static object_type* supply_entry_display_object(const supply_list_entry* entry,
    bool aware, object_type* fake)
{
    object_type* o_ptr = NULL;

    if (!entry)
        return NULL;

    if (entry->supply_idx >= 0)
    {
        o_ptr = supplies_entry_at(entry->supply_idx);
    }
    else if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        o_ptr = &inventory[entry->equip_idx];
    }
    else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        o_ptr = &inventory[entry->item_idx];
    }
    else if (entry->floor_idx > 0 && entry->floor_idx < o_max)
    {
        o_ptr = &o_list[entry->floor_idx];
    }
    else if (fake)
    {
        object_wipe(fake);
        object_prep(fake, entry->k_idx);
        if (aware)
            fake->ident |= IDENT_KNOWN;
        fake->number = (entry->total > 0) ? entry->total : 1;
        o_ptr = fake;
    }

    if (entry->single_item_display && o_ptr && fake && o_ptr->k_idx
        && o_ptr->number > 1)
    {
        object_copy(fake, o_ptr);
        fake->number = 1;
        o_ptr = fake;
    }

    return o_ptr;
}

static void supply_strip_leading_name_unit(char* buf)
{
    static cptr prefixes[] = {
        "Fragment of ",
        "Fragments of ",
        "Piece of ",
        "Pieces of ",
        "Strip of ",
        "Strips of ",
        NULL
    };
    int i;

    if (!buf || !buf[0])
        return;

    for (i = 0; prefixes[i]; i++)
    {
        size_t len = strlen(prefixes[i]);

        if (strncmp(buf, prefixes[i], len) == 0)
        {
            memmove(buf, buf + len, strlen(buf + len) + 1);
            return;
        }
    }
}

static void supply_strip_compact_kind_word(char* buf, size_t buflen)
{
    static cptr prefixes[] = {
        "Easter Egg of ",
        "Easter Eggs of ",
        "Herb of ",
        "Herbs of ",
        "Potion of ",
        "Potions of ",
        "Gem of ",
        "Gems of ",
        NULL
    };
    static cptr generic_names[] = {
        "Easter Egg",
        "Easter Eggs",
        "Herb",
        "Herbs",
        "Potion",
        "Potions",
        "Gem",
        "Gems",
        NULL
    };
    int i;

    if (!buf || buflen == 0 || !buf[0])
        return;

    for (i = 0; prefixes[i]; i++)
    {
        size_t len = strlen(prefixes[i]);

        if (strncmp(buf, prefixes[i], len) == 0)
        {
            memmove(buf, buf + len, strlen(buf + len) + 1);
            if (!buf[0])
                SDL_strlcpy(buf, "?", buflen);
            return;
        }
    }

    for (i = 0; generic_names[i]; i++)
    {
        if (strcmp(buf, generic_names[i]) == 0)
        {
            SDL_strlcpy(buf, "?", buflen);
            return;
        }
    }
}

static bool supply_entry_compact_flavorless_name(char* buf, size_t buflen,
    const object_type* o_ptr)
{
    object_kind* k_ptr;

    if (!buf || buflen == 0 || !o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->k_idx < 0 || o_ptr->k_idx >= z_info->k_max)
        return false;

    k_ptr = &k_info[o_ptr->k_idx];
    if (!k_ptr->flavor)
        return false;

    if (object_aware_p(o_ptr))
    {
        object_desc_spoil(buf, buflen, o_ptr, false, 0);
        supply_strip_leading_name_unit(buf);
        supply_strip_compact_kind_word(buf, buflen);
        return true;
    }

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        if (o_ptr->sval >= SV_FOOD_MIN_FOOD)
            return false;
        SDL_strlcpy(buf, easter_time() ? "Easter Egg" : "Herb", buflen);
        break;
    case TV_POTION:
        SDL_strlcpy(buf, "Potion", buflen);
        break;
    case TV_GEM:
        SDL_strlcpy(buf, "Gem", buflen);
        break;
    default:
        return false;
    }

    supply_strip_compact_kind_word(buf, buflen);

    if (object_tried_p(o_ptr))
        SDL_strlcat(buf, " {tried}", buflen);

    return true;
}

static void supply_entry_display_name(char* buf, size_t buflen,
    const supply_list_entry* entry, const object_type* o_ptr, int current_group,
    bool compact_names)
{
    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!entry || !o_ptr)
        return;

    if (compact_names
        && supply_entry_compact_flavorless_name(buf, buflen, o_ptr))
    {
        return;
    }

    object_desc(buf, buflen, o_ptr, false, 0);
    supply_strip_leading_name_unit(buf);

    if (current_group == SUPPLY_GROUP_LIGHTS)
    {
        strip_supply_light_turns_suffix(buf);
        if (entry->equipped)
            SDL_strlcat(buf, " [equipped]", buflen);
    }
}

static const object_type* jewelry_preset_first_object(int preset)
{
    for (int slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
    {
        const object_type* o_ptr = jewelry_preset_object(preset, slot);
        if (o_ptr && o_ptr->k_idx)
            return o_ptr;
    }

    return NULL;
}

static int jewelry_preset_icon_kind(int preset)
{
    const object_type* o_ptr = jewelry_preset_first_object(preset);

    if (o_ptr && o_ptr->k_idx)
        return o_ptr->k_idx;

    return lookup_kind(TV_RING, SV_RING_ACCURACY);
}

static void jewelry_preset_append_object_name(char* buf, size_t buflen,
    cptr label, const object_type* o_ptr)
{
    char name[80];

    if (!buf || buflen == 0)
        return;

    if (buf[0])
        SDL_strlcat(buf, " | ", buflen);

    SDL_strlcat(buf, label, buflen);
    SDL_strlcat(buf, ": ", buflen);

    if (!o_ptr || !o_ptr->k_idx)
    {
        SDL_strlcat(buf, "-", buflen);
        return;
    }

    object_desc(name, sizeof(name), o_ptr, false, 0);
    SDL_strlcat(buf, name, buflen);
}

static void jewelry_preset_summary(char* buf, size_t buflen, int preset)
{
    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!jewelry_preset_is_set(preset))
    {
        SDL_strlcpy(buf, "<empty>", buflen);
        return;
    }

    jewelry_preset_append_object_name(buf, buflen, "L",
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_LEFT));
    jewelry_preset_append_object_name(buf, buflen, "R",
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_RIGHT));
    jewelry_preset_append_object_name(buf, buflen, "N",
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_NECK));
}

static void jewelry_preset_object_name(char* buf, size_t buflen,
    const object_type* o_ptr)
{
    if (!buf || buflen == 0)
        return;

    if (!o_ptr || !o_ptr->k_idx)
    {
        SDL_strlcpy(buf, "-", buflen);
        return;
    }

    object_desc(buf, buflen, o_ptr, false, 0);
}

static int jewelry_preset_display_rows(const knowledge_browser_layout* layout,
    const supply_list_columns* cols)
{
    int name_w = cols ? cols->name_w : 0;
    int rows;

    if (name_w <= 0 && layout)
        name_w = layout->list_w;

    if (name_w >= 120)
        rows = 1;
    else if (name_w >= 80)
        rows = 2;
    else
        rows = 3;

    if (layout && layout->list_rows > 0 && rows > layout->list_rows)
        rows = layout->list_rows;
    if (rows < 1)
        rows = 1;

    return rows;
}

static int jewelry_preset_entries_per_page(const knowledge_browser_layout* layout,
    const supply_list_columns* cols)
{
    int rows_per_entry = jewelry_preset_display_rows(layout, cols);
    int list_rows = (layout && layout->list_rows > 0) ? layout->list_rows : 1;
    int entries = list_rows / rows_per_entry;

    if (entries < 1)
        entries = 1;

    return entries;
}

static void jewelry_preset_display_line(char* buf, size_t buflen, int preset,
    int line, int rows_per_entry)
{
    char left[80];
    char right[80];
    char neck[80];

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!jewelry_preset_is_set(preset))
    {
        if (line == 0)
            SDL_strlcpy(buf, "<empty>", buflen);
        return;
    }

    jewelry_preset_object_name(left, sizeof(left),
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_LEFT));
    jewelry_preset_object_name(right, sizeof(right),
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_RIGHT));
    jewelry_preset_object_name(neck, sizeof(neck),
        jewelry_preset_object(preset, JEWELRY_PRESET_SLOT_NECK));

    if (rows_per_entry <= 1)
    {
        if (line == 0)
            strnfmt(buf, buflen, "L: %s | R: %s | N: %s", left, right, neck);
        return;
    }

    if (rows_per_entry == 2)
    {
        if (line == 0)
            strnfmt(buf, buflen, "L: %s | R: %s", left, right);
        else if (line == 1)
            strnfmt(buf, buflen, "N: %s", neck);
        return;
    }

    switch (line)
    {
    case 0: strnfmt(buf, buflen, "L: %s", left); break;
    case 1: strnfmt(buf, buflen, "R: %s", right); break;
    case 2: strnfmt(buf, buflen, "N: %s", neck); break;
    default: break;
    }
}

static void supply_put_fitted(int col, int row, int width, byte attr, cptr text)
{
    char fitted[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;

    if (col < 0)
    {
        width += col;
        col = 0;
    }

    if (col >= term_wid || width <= 0)
        return;

    if (col + width > term_wid)
        width = term_wid - col;
    if (width <= 0)
        return;

    settings_ui_fit_text(fitted, sizeof(fitted), text, width);
    Term_putstr(col, row, width, attr, fitted);
}

static int supply_entry_turns(const supply_list_entry* entry,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return -1;

    if (o_ptr->tval == TV_FLASK)
        return -2;

    if (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
        && (!entry || !entry->equipped))
    {
        return -2;
    }

    if (o_ptr->tval != TV_LIGHT)
        return -1;

    if (!fuelable_light_p(o_ptr))
        return -1;

    return player_light_fuel(o_ptr);
}

static bool supply_kind_is_seen(const object_kind* k_ptr);

static bool supply_icon_selected_background(int col, int row, byte* bg_attr)
{
    byte attr;
    char chr;

    if (!Term || !Term->scr || !bg_attr)
        return false;
    if (col < 0 || row < 0 || col >= Term->wid || row >= Term->hgt)
        return false;

    attr = Term->scr->a[row][col];
    chr = Term->scr->c[row][col];
    if (attr < TERM_UI_SELECTED || chr != ' ')
        return false;

    *bg_attr = attr;
    return true;
}

void draw_supply_icon(int col, int row, const object_type* o_ptr)
{
    byte sym_attr;
    char sym_char;
    byte bg_attr = 0;
    bool selected_bg;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    sym_attr = object_attr(o_ptr);
    sym_char = object_char(o_ptr);
    selected_bg = supply_icon_selected_background(col, row, &bg_attr);

    if (selected_bg)
        Term_queue_char(col, row, sym_attr, sym_char, bg_attr, ' ');
    else
        Term_putch(col, row, sym_attr, sym_char);

    if (use_bigtile)
    {
        if (sym_attr & TILE_FLAG)
        {
            if (selected_bg)
                Term_queue_char(col + 1, row, 255, -1, bg_attr, ' ');
            else
                Term_putch(col + 1, row, 255, -1);
        }
        else
        {
            if (selected_bg)
                Term_queue_char(col + 1, row, bg_attr, ' ', bg_attr, ' ');
            else
                Term_putch(col + 1, row, 0, ' ');
        }
    }
}

static int supply_group_fixed_icon_kind(int group)
{
    int k_idx = 0;

    switch (group)
    {
    case SUPPLY_GROUP_HERBS:
        k_idx = lookup_kind(TV_FOOD, SV_FOOD_HEALING);
        break;
    case SUPPLY_GROUP_FOOD:
        k_idx = lookup_kind(TV_FOOD, SV_FOOD_LEMBAS);
        if (k_idx <= 0)
            k_idx = lookup_kind(TV_FOOD, SV_FOOD_BREAD);
        break;
    case SUPPLY_GROUP_POTIONS:
        k_idx = lookup_kind(TV_POTION, SV_POTION_HEALING);
        break;
    case SUPPLY_GROUP_GEMS:
        k_idx = lookup_kind(TV_GEM, SV_GEM_LIGHT);
        break;
    case SUPPLY_GROUP_LIGHTS:
        k_idx = lookup_kind(TV_LIGHT, SV_LIGHT_TORCH);
        break;
    case SUPPLY_GROUP_JEWELRY_PRESETS:
        k_idx = lookup_kind(TV_RING, SV_RING_ACCURACY);
        break;
    case SUPPLY_GROUP_SUPPLY:
        k_idx = lookup_kind(TV_CHEST, SV_CHEST_SMALL_WOODEN);
        break;
    default:
        break;
    }

    if (k_idx > 0)
        return k_idx;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (supply_kind_matches(group, k_ptr->tval, k_ptr->sval))
            return i;
    }

    return 0;
}

static bool supply_group_kind_is_carried(int group, int k_idx)
{
    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (o_ptr->k_idx == k_idx && supply_item_matches(group, o_ptr))
            return true;
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (o_ptr && o_ptr->k_idx == k_idx && supply_item_matches(group, o_ptr))
            return true;
    }

    return false;
}

static bool supply_group_icon_candidate(int group, int k_idx,
    bool require_known_or_carried)
{
    object_kind* k_ptr;

    if (k_idx <= 0 || k_idx >= z_info->k_max)
        return false;

    k_ptr = &k_info[k_idx];
    if (!k_ptr->name)
        return false;
    if (!supply_kind_matches(group, k_ptr->tval, k_ptr->sval))
        return false;

    if (!require_known_or_carried)
        return true;

    return supply_group_kind_is_carried(group, k_idx)
        || supply_kind_is_seen(k_ptr);
}

static int supply_group_random_icon_kind(int group)
{
    int chosen = 0;
    int seen = 0;

    for (int pass = 0; pass < 2 && chosen <= 0; pass++)
    {
        bool require_known_or_carried = (pass == 0);

        seen = 0;
        for (int i = 0; i < z_info->k_max; i++)
        {
            if (!supply_group_icon_candidate(group, i,
                    require_known_or_carried))
                continue;

            seen++;
            if (rand_int(seen) == 0)
                chosen = i;
        }
    }

    return (chosen > 0) ? chosen : supply_group_fixed_icon_kind(group);
}

static void choose_supply_group_icon_kinds(
    int group_icon_kinds[SUPPLY_GROUP_MAX])
{
    bool random_icons = op_ptr && op_ptr->opt[OPT_supply_menu_random_icons];
    u64b saved_state = Rand_state_export();

    for (int group = 0; group < SUPPLY_GROUP_MAX; group++)
    {
        if (group == SUPPLY_GROUP_LIGHTS || !random_icons)
            group_icon_kinds[group] = supply_group_fixed_icon_kind(group);
        else
            group_icon_kinds[group] = supply_group_random_icon_kind(group);
    }

    Rand_state_import(saved_state);
}

static void prepare_supply_group_icons(supply_group_icon icons[SUPPLY_GROUP_MAX],
    const int group_icon_kinds[SUPPLY_GROUP_MAX])
{
    for (int group = 0; group < SUPPLY_GROUP_MAX; group++)
    {
        object_type* icon_obj = &icons[group].obj;
        int k_idx = group_icon_kinds[group];

        icons[group].has_icon = false;
        object_wipe(icon_obj);

        if (group == SUPPLY_GROUP_LIGHTS)
        {
            object_type* light_ptr = &inventory[INVEN_LITE];

            if (light_ptr->k_idx && light_ptr->tval == TV_LIGHT)
            {
                object_copy(icon_obj, light_ptr);
                icons[group].has_icon = true;
                continue;
            }
        }

        if (k_idx <= 0 || k_idx >= z_info->k_max)
            continue;

        object_prep(icon_obj, k_idx);
        icon_obj->ident |= IDENT_KNOWN;
        icons[group].has_icon = true;
    }
}

static void supply_init_columns(const knowledge_browser_layout* layout,
    int current_group, supply_list_columns* cols)
{
    int col;

    if (!layout || !cols)
        return;

    memset(cols, 0, sizeof(*cols));

    cols->show_sym = true;
    cols->show_qty = true;
    cols->show_weight = (current_group == SUPPLY_GROUP_FOOD)
        || (current_group == SUPPLY_GROUP_LIGHTS)
        || (current_group == SUPPLY_GROUP_SUPPLY);
    cols->show_turns = (current_group == SUPPLY_GROUP_LIGHTS);

    if (current_group == SUPPLY_GROUP_JEWELRY_PRESETS)
    {
        cols->show_qty = false;
        cols->show_weight = false;
        cols->show_turns = false;
    }

    col = layout->term_wid;

    if (cols->show_qty)
    {
        col -= 4;
        cols->qty_col = col;
        col -= 1;
    }

    if (cols->show_turns)
    {
        col -= 5;
        cols->turns_col = col;
        col -= 1;
    }

    if (cols->show_weight)
    {
        col -= 5;
        cols->weight_col = col;
        col -= 1;
    }

    cols->name_col = layout->list_col;
    if (cols->show_sym)
    {
        cols->sym_hdr_col = layout->list_col;
        cols->sym_col = layout->list_col;
        cols->name_col = layout->list_col + (use_bigtile ? 2 : 1);
    }

    cols->name_w = col - cols->name_col;
    if (cols->name_w < 1)
        cols->name_w = 1;
}

static int supply_max_name_len(int current_group, supply_list_entry entries[],
    int entry_cnt, bool compact_names)
{
    int max_len = 0;
    int i;

    for (i = 0; i < entry_cnt; i++)
    {
        supply_list_entry* entry = &entries[i];
        object_kind* k_ptr;
        object_type fake;
        object_type* o_ptr;
        char name[128];
        int len;

        if (current_group == SUPPLY_GROUP_JEWELRY_PRESETS)
        {
            if (entry->preset_idx < 0)
                continue;

            jewelry_preset_summary(name, sizeof(name), entry->preset_idx);
            len = (int)strlen(name);
            if (len > max_len)
                max_len = len;
            continue;
        }

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        o_ptr = supply_entry_display_object(entry, k_ptr->aware, &fake);
        if (!o_ptr)
            continue;

        supply_entry_display_name(name, sizeof(name), entry, o_ptr,
            current_group, compact_names);
        len = (int)strlen(name);
        if (current_group == SUPPLY_GROUP_SUPPLY && entry->supply_idx >= 0)
            len += 3;
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static bool supply_use_compact_names_for_width(
    const knowledge_browser_layout* layout)
{
    return layout && (layout->term_wid <= SUPPLY_COMPACT_TERM_WIDTH);
}

static void compute_supply_group_totals(int totals[SUPPLY_GROUP_MAX])
{
    int i;

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        totals[i] = 0;

    totals[SUPPLY_GROUP_JEWELRY_PRESETS] = jewelry_preset_count();
    totals[SUPPLY_GROUP_SUPPLY] = 0;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
        else if (supply_item_matches(SUPPLY_GROUP_LIGHTS, o_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(o_ptr) > 0
                ? player_oil_container_slot_cost(o_ptr) * o_ptr->number
                : o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += s_ptr->number;
        else if (supply_item_matches(SUPPLY_GROUP_LIGHTS, s_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(s_ptr) > 0
                ? player_oil_container_slot_cost(s_ptr) * s_ptr->number
                : s_ptr->number;

        totals[SUPPLY_GROUP_SUPPLY] += s_ptr->number;
    }

    object_type* light_ptr = &inventory[INVEN_LITE];
    if (supply_item_matches(SUPPLY_GROUP_LIGHTS, light_ptr))
    {
        totals[SUPPLY_GROUP_LIGHTS] += player_oil_container_slot_cost(light_ptr) > 0
            ? player_oil_container_slot_cost(light_ptr) * MAX(light_ptr->number, 1)
            : MAX(light_ptr->number, 1);
    }

    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (i = 0; i < floor_num; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr;
            int number;

            if (o_idx <= 0 || o_idx >= o_max)
                continue;

            o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || !supplies_is_supply_object(o_ptr))
                continue;

            number = MAX(o_ptr->number, 1);
            if (supply_kind_matches(SUPPLY_GROUP_HERBS, o_ptr->tval,
                    o_ptr->sval))
            {
                totals[SUPPLY_GROUP_HERBS] += number;
            }
            else if (supply_kind_matches(SUPPLY_GROUP_FOOD, o_ptr->tval,
                    o_ptr->sval))
            {
                totals[SUPPLY_GROUP_FOOD] += number;
            }
            else if (o_ptr->tval == TV_POTION)
            {
                totals[SUPPLY_GROUP_POTIONS] += number;
            }
            else if (o_ptr->tval == TV_GEM)
            {
                totals[SUPPLY_GROUP_GEMS] += number;
            }
            else if (supply_item_matches(SUPPLY_GROUP_LIGHTS, o_ptr))
            {
                int slots = player_oil_container_slot_cost(o_ptr);
                totals[SUPPLY_GROUP_LIGHTS] += (slots > 0)
                    ? slots * number
                    : number;
            }

            totals[SUPPLY_GROUP_SUPPLY] += number;
        }
    }
}

static bool supply_kind_is_seen(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->everseen || k_ptr->tried;
}

/* Append rows for matching items lying on the floor under the player, so they
 * can be compared with carried supplies and picked up from the same menu. */
static void supply_collect_floor_entries(int group_idx,
    supply_list_entry entries[], int* count, int capacity)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || !count)
        return;

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00);

    for (int i = 0; i < floor_num && *count < capacity; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx || !supplies_is_supply_object(o_ptr)
            || !supply_item_matches(group_idx, o_ptr))
            continue;

        entries[*count].k_idx = o_ptr->k_idx;
        entries[*count].item_idx = -1;
        entries[*count].total = MAX(o_ptr->number, 1);
        entries[*count].supply_idx = -1;
        entries[*count].equip_idx = -1;
        entries[*count].preset_idx = -1;
        entries[*count].floor_idx = o_idx;
        entries[*count].equipped = false;
        entries[*count].single_item_display = false;
        (*count)++;
    }
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[],
    int capacity)
{
    int count = 0;
    int i;

    if (!entries || capacity <= 0)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);
    for (i = 0; i < capacity; i++)
    {
        entries[i].item_idx = -1;
        entries[i].supply_idx = -1;
        entries[i].equip_idx = -1;
        entries[i].preset_idx = -1;
        entries[i].floor_idx = -1;
        entries[i].k_idx = -1;
        entries[i].single_item_display = false;
    }

    if (group_idx == SUPPLY_GROUP_JEWELRY_PRESETS)
    {
        int fallback = lookup_kind(TV_RING, SV_RING_ACCURACY);

        for (i = 0; i < JEWELRY_PRESET_MAX && count < capacity; i++)
        {
            int icon_kind = jewelry_preset_icon_kind(i);

            entries[count].k_idx = (icon_kind > 0) ? icon_kind : fallback;
            entries[count].item_idx = -1;
            entries[count].total = jewelry_preset_is_set(i) ? 1 : 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].preset_idx = i;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
            count++;
        }

        if (count < capacity)
        {
            entries[count].k_idx = -1;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].preset_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
        }

        return count;
    }

    if (group_idx == SUPPLY_GROUP_SUPPLY)
    {
        for (i = 0; i < supplies_entry_count() && count < capacity; i++)
        {
            object_type* s_ptr = supplies_entry_at(i);

            if (!s_ptr || !s_ptr->k_idx)
                continue;

            entries[count].k_idx = s_ptr->k_idx;
            entries[count].item_idx = SUPPLIES_INDEX;
            entries[count].total = s_ptr->number;
            entries[count].supply_idx = i;
            entries[count].equip_idx = -1;
            entries[count].preset_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
            count++;
        }

        supply_collect_floor_entries(group_idx, entries, &count, capacity);

        if (count < capacity)
        {
            entries[count].k_idx = -1;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].preset_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
        }

        return count;
    }

    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        for (i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            int value;
            int unit;

            if (!supply_item_matches(group_idx, o_ptr))
                continue;

            value = MAX(o_ptr->number, 1);
            if (o_ptr->tval == TV_FLASK)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
                continue;
            }

            for (unit = 0; unit < value && count < capacity; unit++)
            {
                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = 1;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = (o_ptr->number > 1);
                count++;
            }
        }

        for (i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            int value;
            int unit;

            if (!supply_item_matches(group_idx, s_ptr))
                continue;

            value = MAX(s_ptr->number, 1);
            if (s_ptr->tval == TV_FLASK)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = value;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
                continue;
            }

            for (unit = 0; unit < value && count < capacity; unit++)
            {
                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = 1;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = (s_ptr->number > 1);
                count++;
            }
        }

        {
            object_type* l_ptr = &inventory[INVEN_LITE];

            if (supply_item_matches(group_idx, l_ptr) && count < capacity)
            {
                entries[count].k_idx = l_ptr->k_idx;
                entries[count].item_idx = INVEN_LITE;
                entries[count].total = 1;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = INVEN_LITE;
                entries[count].equipped = true;
                entries[count].single_item_display = false;
                count++;
            }
        }
    }
    else
    {

        /* Aggregate carried items first */
        for (i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            int j;

            if (!o_ptr->k_idx)
                continue;

            if (!supply_item_matches(group_idx, o_ptr))
                continue;

            int value = o_ptr->number;

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == o_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = i;
                    break;
                }
            }

            if (j == count)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = o_ptr->k_idx;
                entries[count].item_idx = i;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
            }
        }

        /* Aggregate supplies from the cache */
        for (i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            int j;

            if (!s_ptr || !s_ptr->k_idx)
                continue;

            if (!supply_item_matches(group_idx, s_ptr))
                continue;

            int value = s_ptr->number;

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == s_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = SUPPLIES_INDEX;
                    entries[j].supply_idx = i;
                    break;
                }
            }

            if (j == count)
            {
                if (count >= capacity)
                    break;

                entries[count].k_idx = s_ptr->k_idx;
                entries[count].item_idx = SUPPLIES_INDEX;
                entries[count].total = value;
                entries[count].supply_idx = i;
                entries[count].equip_idx = -1;
                entries[count].equipped = false;
                entries[count].single_item_display = false;
                count++;
            }
        }

        if (group_idx == SUPPLY_GROUP_LIGHTS)
        {
            object_type* l_ptr = &inventory[INVEN_LITE];
            int j;

            if (supply_item_matches(group_idx, l_ptr))
            {
                for (j = 0; j < count; j++)
                {
                    if (entries[j].k_idx == l_ptr->k_idx)
                    {
                        entries[j].total += MAX(l_ptr->number, 1);
                        entries[j].equip_idx = INVEN_LITE;
                        entries[j].equipped = true;
                        if (entries[j].item_idx < 0)
                            entries[j].item_idx = INVEN_LITE;
                        break;
                    }
                }

                if (j == count && count < capacity)
                {
                    entries[count].k_idx = l_ptr->k_idx;
                    entries[count].item_idx = INVEN_LITE;
                    entries[count].total = MAX(l_ptr->number, 1);
                    entries[count].supply_idx = -1;
                    entries[count].equip_idx = INVEN_LITE;
                    entries[count].equipped = true;
                    entries[count].single_item_display = false;
                    count++;
                }
            }
        }
    }

    supply_collect_floor_entries(group_idx, entries, &count, capacity);

    /* Add seen kinds even when none are carried.
     * Lights are listed only when actually carried, to avoid misleading 0-count
     * placeholders in the supply menu. */
    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        if (count < capacity)
        {
            entries[count].k_idx = -1;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
        }

        return count;
    }

    /* Add seen kinds even when none are carried */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_seen(k_ptr))
            continue;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == i)
                break;
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = i;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            entries[count].equip_idx = -1;
            entries[count].equipped = false;
            entries[count].single_item_display = false;
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
        entries[count].equip_idx = -1;
        entries[count].equipped = false;
        entries[count].single_item_display = false;
    }

    return count;
}

static byte get_supply_item_color(int k_idx, bool aware)
{
    object_kind* k_ptr;

    if (k_idx < 0 || k_idx >= z_info->k_max)
        return TERM_WHITE;

    k_ptr = &k_info[k_idx];

    /* Unidentified items all use slate color */
    if (!aware)
        return TERM_SLATE;

    /* Color by specific item type */
    switch (k_ptr->tval)
    {
        case TV_FOOD: /* Herbs */
            switch (k_ptr->sval)
            {
                case SV_FOOD_RAGE:         return TERM_RED;    /* Red for rage */
                case SV_FOOD_SUSTENANCE:   return TERM_GREEN;    /* Green for sustenance */
                case SV_FOOD_TERROR:       return TERM_VIOLET;   /* Violet for fear */
                case SV_FOOD_HEALING:      return TERM_L_GREEN;  /* Light green for healing */
                case SV_FOOD_RESTORATION:  return TERM_BLUE;     /* Blue for restoration */
                case SV_FOOD_HUNGER:       return TERM_UMBER;    /* Brown for hunger */
                case SV_FOOD_VISIONS:      return TERM_L_UMBER;  /* Light brown for visions */
                case SV_FOOD_ENTRANCEMENT: return TERM_VIOLET;   /* Violet for entrancement */
                case SV_FOOD_WEAKNESS:     return TERM_SLATE;    /* Grey for weakness */
                case SV_FOOD_SICKNESS:     return TERM_L_DARK;   /* Dark grey for sickness */
                case SV_FOOD_LEMBAS:       return TERM_L_WHITE;
                default:                   return TERM_WHITE;
            }

        case TV_POTION:
            switch (k_ptr->sval)
            {
                case SV_POTION_MIRUVOR:          return TERM_WHITE;  /* White for Miruvor */
                case SV_POTION_ORCISH_LIQUOR:    return TERM_UMBER;    /* Brown for liquor */
                case SV_POTION_ESGALDUIN:        return TERM_VIOLET;   /* Violet for Esgalduin */
                case SV_POTION_CLARITY:          return TERM_L_UMBER;  /* Light brown for clarity */
                case SV_POTION_HEALING:          return TERM_L_GREEN;  /* Light green for healing */
                case SV_POTION_VOICE:            return TERM_L_BLUE;  /* White for voice */
                case SV_POTION_true_SIGHT:       return TERM_BLUE;     /* Blue for true sight */
                case SV_POTION_ANTIDOTE:         return TERM_GREEN;    /* Green for antidote */
                case SV_POTION_QUICKNESS:        return TERM_ORANGE;  /* Light brown for speed */
                case SV_POTION_ELEM_RESISTANCE:  return TERM_L_BLUE;   /* Orange for resistance */
                case SV_POTION_STR:              return TERM_RED;      /* Red for strength */
                case SV_POTION_DEX:              return TERM_GREEN;    /* Green for dexterity */
                case SV_POTION_CON:              return TERM_L_RED;     /* Blue for constitution */
                case SV_POTION_GRA:              return TERM_BLUE;   /* Violet for grace */
                case SV_POTION_SLOWNESS:         return TERM_SLATE;    /* Grey for slowness */
                case SV_POTION_POISON:           return TERM_L_DARK;   /* Dark for poison */
                case SV_POTION_BLINDNESS:        return TERM_L_DARK;   /* Dark for blindness */
                case SV_POTION_CONFUSION:        return TERM_SLATE;    /* Grey for confusion */
                case SV_POTION_DEC_DEX:          return TERM_SLATE;    /* Grey for decrease dex */
                case SV_POTION_DEC_GRA:          return TERM_SLATE;    /* Grey for decrease grace */
                default:                         return TERM_WHITE;
            }

        case TV_GEM:
            switch (k_ptr->sval)
            {
                case SV_GEM_FREEDOM:         return TERM_WHITE;  /* White for freedom */
                case SV_GEM_LIGHT:           return TERM_ORANGE;   /* Orange for light */
                case SV_GEM_SANCTITY:        return TERM_L_UMBER;  /* Light brown for sanctity */
                case SV_GEM_UNDERSTANDING:   return TERM_BLUE;     /* Blue for understanding */
                case SV_GEM_REVELATIONS:     return TERM_L_BLUE;   /* Violet for revelations */
                case SV_GEM_TREASURES:       return TERM_ORANGE;   /* Orange for treasures */
                case SV_GEM_FOES:            return TERM_RED;      /* Red for foes */
                case SV_GEM_SELF_KNOWLEDGE:  return TERM_GREEN;  /* Light green for self-knowledge */
                case SV_GEM_WARDING:         return TERM_VIOLET;  /* Light brown for warding */
                case SV_GEM_RECHARGING:      return TERM_BLUE;     /* Blue for recharging */
                case SV_GEM_SHADOWS:         return TERM_L_DARK;   /* Dark for shadows */
                default:                     return TERM_WHITE;
            }

        case TV_FLASK:
            return TERM_YELLOW;

        case TV_LIGHT:
            switch (k_ptr->sval)
            {
                case SV_LIGHT_TORCH:   return TERM_UMBER;
                case SV_LIGHT_MALLORN: return TERM_YELLOW;
                case SV_LIGHT_LANTERN: return TERM_L_UMBER;
                case SV_LIGHT_LESSER_JEWEL: return TERM_L_BLUE;
                case SV_LIGHT_FEANORIAN: return TERM_WHITE;
                default:               return TERM_WHITE;
            }

        default:
            return TERM_WHITE;
    }
}

static byte supply_browser_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static void supply_browser_fill_row(int col, int row, int width, byte attr)
{
    char fill[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

static int supply_browser_selection_width(int start_col, int text_col,
    int text_w, cptr text, int max_w)
{
    int prefix_w = text_col - start_col;
    int text_len = text ? (int)strlen(text) : 0;
    int width;

    if (prefix_w < 0)
        prefix_w = 0;
    if (text_w <= 0)
        text_len = 0;
    else if (text_len > text_w)
        text_len = text_w;

    width = prefix_w + text_len;
    if (width < 1)
        width = 1;
    if (max_w > 0 && width > max_w)
        width = max_w;

    return width;
}

static void display_supply_group_list(int col, int row, int wid,
    int selection_w, int per_page, int grp_idx[], int grp_cur, int grp_top,
    int group_totals[],
    const supply_group_icon icons[SUPPLY_GROUP_MAX], bool active)
{
    int i;
    int total_col = col + wid - 3;
    int text_col = col + (use_bigtile ? 2 : 1);
    int text_w = total_col - text_col;
    int erase_w = (selection_w > wid) ? selection_w : wid;

    for (i = 0; i < per_page; i++)
    {
        int grp_pos = grp_top + i;
        int grp;
        byte base_color;
        byte attr;
        char buf[8];
        bool selected;
        bool highlighted;

        if (grp_pos >= SUPPLY_GROUP_MAX || grp_idx[grp_pos] < 0)
            break;

        grp = grp_idx[grp_pos];

        /* Assign color based on group type */
        switch (grp)
        {
            case SUPPLY_GROUP_HERBS:   base_color = TERM_GREEN;   break;
            case SUPPLY_GROUP_FOOD:    base_color = TERM_L_GREEN; break;
            case SUPPLY_GROUP_POTIONS: base_color = TERM_VIOLET;  break;
            case SUPPLY_GROUP_GEMS:    base_color = TERM_BLUE;    break;
            case SUPPLY_GROUP_LIGHTS:  base_color = TERM_YELLOW;  break;
            case SUPPLY_GROUP_JEWELRY_PRESETS:
                                      base_color = TERM_L_BLUE; break;
            default:                   base_color = TERM_WHITE;   break;
        }

        selected = (grp_top + i == grp_cur);
        highlighted = selected && active;

        if (highlighted)
            attr = supply_browser_selected_attr(base_color);
        else if (selected)
            attr = TERM_L_BLUE;
        else if (group_totals[grp] == 0)
            attr = TERM_L_DARK;
        else
            attr = base_color;

        Term_erase(col, row + i, erase_w);
        if (highlighted)
            supply_browser_fill_row(col, row + i, selection_w, attr);
        if (icons && icons[grp].has_icon)
            draw_supply_icon(col, row + i, &icons[grp].obj);
        if (text_w > 0)
            supply_put_fitted(text_col, row + i, text_w, attr,
                supply_group_text[grp]);

        strnfmt(buf, sizeof(buf), "%3d", group_totals[grp]);
        if (wid >= 3)
            supply_put_fitted(total_col, row + i, 3, attr, buf);
    }
}

static void display_supply_list(const knowledge_browser_layout* layout, int row,
    int per_page, supply_list_entry entries[], int entry_cnt, int entry_cur,
    int entry_top, int current_group, int column,
    const supply_list_columns* cols, bool compact_names)
{
    int i;

    if (current_group == SUPPLY_GROUP_JEWELRY_PRESETS)
    {
        int rows_per_entry = jewelry_preset_display_rows(layout, cols);
        int visible_entries = jewelry_preset_entries_per_page(layout, cols);
        int list_end = row + per_page;
        int name_w = cols ? cols->name_w : (layout ? layout->list_w : 1);

        for (i = 0; i < per_page; i++)
            Term_erase(layout->list_col, row + i, 255);

        for (i = 0; i < visible_entries; i++)
        {
            int idx = entry_top + i;
            int y = row + (i * rows_per_entry);
            supply_list_entry* entry;
            object_type fake;
            const object_type* icon_obj;
            object_type* draw_obj = NULL;
            byte attr;
            bool selected;
            bool set;
            byte base_attr;

            if (idx >= entry_cnt || y >= list_end)
                break;

            entry = &entries[idx];
            if (entry->preset_idx < 0)
                continue;

            selected = (column == 1 && idx == entry_cur);
            set = jewelry_preset_is_set(entry->preset_idx);
            base_attr = set ? TERM_L_BLUE : TERM_L_DARK;
            attr = selected ? supply_browser_selected_attr(base_attr)
                            : base_attr;

            object_wipe(&fake);
            icon_obj = jewelry_preset_first_object(entry->preset_idx);
            if (icon_obj && icon_obj->k_idx)
            {
                object_copy(&fake, icon_obj);
                draw_obj = &fake;
            }
            else if (entry->k_idx > 0 && entry->k_idx < z_info->k_max)
            {
                object_prep(&fake, entry->k_idx);
                fake.ident |= IDENT_KNOWN;
                draw_obj = &fake;
            }

            if (selected)
            {
                char line_buf[160];
                char text_buf[180];
                char prefix[8];
                int selection_w;

                jewelry_preset_display_line(line_buf, sizeof(line_buf),
                    entry->preset_idx, 0, rows_per_entry);
                strnfmt(prefix, sizeof(prefix), "%d  ",
                    entry->preset_idx + 1);
                strnfmt(text_buf, sizeof(text_buf), "%s%s", prefix, line_buf);
                selection_w = supply_browser_selection_width(layout->list_col,
                    cols->name_col, name_w, text_buf, layout->list_w);
                supply_browser_fill_row(layout->list_col, y, selection_w,
                    attr);
            }
            if (cols->show_sym && draw_obj)
                draw_supply_icon(cols->sym_col, y, draw_obj);

            for (int line = 0; line < rows_per_entry && y + line < list_end;
                 line++)
            {
                char prefix[8];
                char line_buf[160];
                char text_buf[180];

                jewelry_preset_display_line(line_buf, sizeof(line_buf),
                    entry->preset_idx, line, rows_per_entry);
                if (!line_buf[0] && line > 0)
                    continue;

                if (line == 0)
                    strnfmt(prefix, sizeof(prefix), "%d  ",
                        entry->preset_idx + 1);
                else
                    SDL_strlcpy(prefix, "   ", sizeof(prefix));

                strnfmt(text_buf, sizeof(text_buf), "%s%s", prefix, line_buf);
                if (selected && line > 0)
                {
                    int selection_w = supply_browser_selection_width(
                        layout->list_col, cols->name_col, name_w, text_buf,
                        layout->list_w);
                    supply_browser_fill_row(layout->list_col, y + line,
                        selection_w, attr);
                }
                supply_put_fitted(cols->name_col, y + line, name_w, attr,
                    text_buf);
            }
        }

        return;
    }

    for (i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        int y = row + i;

        Term_erase(layout->list_col, y, 255);

        if (idx >= entry_cnt)
            continue;

        supply_list_entry* entry = &entries[idx];
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        object_type* o_ptr;
        byte base_attr, attr;
        char name[128];
        char display_name[160];
        char label_prefix[8];
        char cell_buf[16];
        bool selected = (column == 1 && idx == entry_cur);

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        aware = k_ptr->aware;
        /* Items with 0 count should be grey */
        if (entry->total == 0)
        {
            base_attr = TERM_L_DARK;
        }
        else
        {
            /* Get color based on specific item type */
            base_attr = get_supply_item_color(entry->k_idx, aware);
        }
        attr = selected ? supply_browser_selected_attr(base_attr) : base_attr;

        o_ptr = supply_entry_display_object(entry, aware, &fake);
        if (!o_ptr)
            continue;

        supply_entry_display_name(name, sizeof(name), entry, o_ptr,
            current_group, compact_names);
        browser_entry_label_prefix(label_prefix, sizeof(label_prefix), idx);
        strnfmt(display_name, sizeof(display_name), "%s%s", label_prefix,
            name);
        if (entry->floor_idx > 0 && entry->floor_idx < o_max)
            SDL_strlcat(display_name, " [floor]", sizeof(display_name));
        if (selected)
        {
            int selection_w = supply_browser_selection_width(layout->list_col,
                cols->name_col, cols->name_w, display_name, layout->list_w);
            supply_browser_fill_row(layout->list_col, y, selection_w, attr);
        }
        if (cols->show_sym)
            draw_supply_icon(cols->sym_col, y, o_ptr);

        Term_putstr(cols->name_col, y, cols->name_w, attr, display_name);

        if (cols->show_weight)
        {
            strnfmt(cell_buf, sizeof(cell_buf), "%d.%1d",
                o_ptr->weight / 10, o_ptr->weight % 10);
            Term_putstr(cols->weight_col, y, 5, base_attr, cell_buf);
        }

        if (cols->show_turns)
        {
            int turns = supply_entry_turns(entry, o_ptr);

            if (turns >= 0)
                strnfmt(cell_buf, sizeof(cell_buf), "%5d", turns);
            else if (turns == -2)
                strnfmt(cell_buf, sizeof(cell_buf), "%5s", "");
            else
                strnfmt(cell_buf, sizeof(cell_buf), "%5s", "inf");
            Term_putstr(cols->turns_col, y, 5, base_attr, cell_buf);
        }

        if (cols->show_qty)
        {
            strnfmt(cell_buf, sizeof(cell_buf), "x%-3d", entry->total);
            Term_putstr(cols->qty_col, y, 4, base_attr, cell_buf);
        }

    }

    for (; i < per_page; i++)
    {
        Term_erase(layout->list_col, row + i, 255);
    }
}

/*
 * Move the cursor in a browser window
 */
static int browser_move_index(int cur, int count, int delta, bool wrap)
{
    int next;

    if (count <= 0)
        return 0;

    next = cur + delta;
    if (wrap)
    {
        next %= count;
        if (next < 0)
            next += count;
        return next;
    }

    if (next >= count)
        next = count - 1;
    if (next < 0)
        next = 0;
    return next;
}

static void browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows, bool wrap_rows)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;
    int page_jump = (page_rows > 0) ? page_rows : BROWSER_ROWS;

    /* Extract direction */
    d = target_dir(ch);

    if (!d)
        return;

    /* Diagonals - hack */
    if ((ddx[d] > 0) && ddy[d])
    {
        /* Browse group list */
        if (!col)
        {
            int old_grp = grp;

            /* Move up or down */
            grp = browser_move_index(grp, grp_cnt, ddy[d] * page_jump,
                wrap_rows);
            if (grp != old_grp)
                list = 0;
        }

        /* Browse sub-list list */
        else
        {
            /* Move up or down */
            list = browser_move_index(list, list_cnt, ddy[d] * page_jump,
                wrap_rows);
        }

        (*grp_cur) = grp;
        (*list_cur) = list;

        return;
    }

    if (ddx[d])
    {
        col += ddx[d];
        if (col < 0)
            col = 0;
        if (col > 1)
            col = 1;

        (*column) = col;

        return;
    }

    /* Browse group list */
    if (!col)
    {
        int old_grp = grp;

        /* Move up or down */
        grp = browser_move_index(grp, grp_cnt, ddy[d], wrap_rows);
        if (grp != old_grp)
            list = 0;
    }

    /* Browse sub-list list */
    else
    {
        /* Move up or down */
        list = browser_move_index(list, list_cnt, ddy[d], wrap_rows);
    }

    (*grp_cur) = grp;
    (*list_cur) = list;
}

static cptr supply_browser_page_text(int page)
{
    switch (page)
    {
    case SUPPLY_MENU_PAGE_EQUIPPED:
        return "Equipped";
    case SUPPLY_MENU_PAGE_INVENTORY:
        return "Inventory";
    case SUPPLY_MENU_PAGE_SUPPLIES:
        return "Supplies";
    default:
        return "";
    }
}

#define SUPPLY_BROWSER_PREV_PAGE_KEY KTRL('P')
#define SUPPLY_BROWSER_NEXT_PAGE_KEY KTRL('N')

static supply_menu_page supply_browser_turn_page(
    supply_menu_page page, int direction)
{
    int next = (int)page + ((direction < 0) ? -1 : 1);

    if (next < SUPPLY_MENU_PAGE_EQUIPPED)
        next = SUPPLY_MENU_PAGE_SUPPLIES;
    else if (next > SUPPLY_MENU_PAGE_SUPPLIES)
        next = SUPPLY_MENU_PAGE_EQUIPPED;

    return (supply_menu_page)next;
}

static int supply_browser_page_tab_col(int page)
{
    int col = 0;

    for (int i = SUPPLY_MENU_PAGE_EQUIPPED; i < page; i++)
    {
        col += (int)strlen(supply_browser_page_text(i)) + 2;
        if (i != SUPPLY_MENU_PAGE_SUPPLIES)
            col++;
    }

    return col;
}

static int supply_browser_page_token_width(int page)
{
    return (int)strlen(supply_browser_page_text(page)) + 2;
}

static byte supply_browser_page_tab_attr(int page, int selected_page,
    int hover_page)
{
    if (page == hover_page)
        return TERM_L_BLUE;
    if (page == selected_page)
        return TERM_L_BLUE;
    return TERM_SLATE;
}

static void supply_draw_page_header(const knowledge_browser_layout* layout,
    int page, int hover_page, cptr title)
{
    int col;

    if (!layout)
        return;

    Term_erase(0, layout->title_row, 255);

    col = 0;
    for (int i = SUPPLY_MENU_PAGE_EQUIPPED;
         i <= SUPPLY_MENU_PAGE_SUPPLIES; i++)
    {
        cptr label = supply_browser_page_text(i);
        bool selected = (i == page);
        byte attr = supply_browser_page_tab_attr(i, page, hover_page);
        char token[32];
        int token_width;

        if (selected)
            strnfmt(token, sizeof(token), "[%s]", label);
        else
            strnfmt(token, sizeof(token), " %s ", label);

        token_width = (int)strlen(token);
        Term_putstr(col, layout->title_row, token_width, attr, token);
        col += token_width;
        if (i != SUPPLY_MENU_PAGE_SUPPLIES)
        {
            Term_putstr(col, layout->title_row, 1, TERM_L_DARK, " ");
            col++;
        }
    }

    if (title && title[0] && col + 3 < layout->term_wid)
    {
        Term_putstr(col, layout->title_row, 3, TERM_L_DARK, " - ");
        col += 3;
        supply_put_fitted(col, layout->title_row, layout->term_wid - col,
            TERM_L_WHITE + TERM_SHADE, title);
    }
}

static int supply_browser_page_click_choice(int page)
{
    switch (page)
    {
    case SUPPLY_MENU_PAGE_EQUIPPED:
        return SUPPLY_CLICK_PAGE_EQUIPPED;
    case SUPPLY_MENU_PAGE_INVENTORY:
        return SUPPLY_CLICK_PAGE_INVENTORY;
    case SUPPLY_MENU_PAGE_SUPPLIES:
        return SUPPLY_CLICK_PAGE_SUPPLIES;
    default:
        return 0;
    }
}

static void supply_register_page_tabs(const knowledge_browser_layout* layout)
{
    int col;

    if (!layout)
        return;

    col = supply_browser_page_tab_col(SUPPLY_MENU_PAGE_EQUIPPED);
    for (int i = SUPPLY_MENU_PAGE_EQUIPPED;
         i <= SUPPLY_MENU_PAGE_SUPPLIES; i++)
    {
        int choice = supply_browser_page_click_choice(i);
        int width = supply_browser_page_token_width(i);

        if (i != SUPPLY_MENU_PAGE_SUPPLIES
            && col + width < layout->term_wid)
        {
            width++;
        }

        ui_menu_click_add(choice, col, layout->title_row, width);
        col += supply_browser_page_token_width(i);
        if (i != SUPPLY_MENU_PAGE_SUPPLIES)
            col++;
    }
}

static void knowledge_begin_touch_scroll_area(
    const knowledge_browser_layout* layout, int touch_category)
{
    int bottom_row;

    if (!layout)
        return;

    bottom_row = layout->status_row - 1;
    if (bottom_row < layout->list_row)
        bottom_row = layout->list_row;

    ui_scroll_area_begin(layout->list_row, bottom_row, touch_category);
    ui_scroll_area_set_keys('8', '2', '6', '4');
}

static int supply_browser_hover_page(void)
{
    int hover_choice;

    if (!ui_menu_click_get_hover_choice(&hover_choice))
        return -1;

    switch (hover_choice)
    {
    case SUPPLY_CLICK_PAGE_EQUIPPED:
        return SUPPLY_MENU_PAGE_EQUIPPED;
    case SUPPLY_CLICK_PAGE_INVENTORY:
        return SUPPLY_MENU_PAGE_INVENTORY;
    case SUPPLY_CLICK_PAGE_SUPPLIES:
        return SUPPLY_MENU_PAGE_SUPPLIES;
    default:
        return -1;
    }
}

enum
{
    EQUIPMENT_MENU_ALL = -1,
    EQUIPMENT_MENU_RINGS = -2,
    EQUIPMENT_MENU_QUIVERS = -3
};

static const int equipment_menu_slots[] = {
    EQUIPMENT_MENU_ALL,
    INVEN_WIELD,
    INVEN_BOW,
    INVEN_STAFF,
    EQUIPMENT_MENU_RINGS,
    INVEN_NECK,
    INVEN_LITE,
    INVEN_BODY,
    INVEN_OUTER,
    INVEN_ARM,
    INVEN_HEAD,
    INVEN_HANDS,
    INVEN_FEET,
    EQUIPMENT_MENU_QUIVERS,
    INVEN_HORN
};

#define EQUIPMENT_MENU_SLOT_COUNT ((int)N_ELEMENTS(equipment_menu_slots))

static cptr equipment_slot_text(int slot)
{
    switch (slot)
    {
    case EQUIPMENT_MENU_ALL:
        return "All equipped";
    case INVEN_WIELD:
        return "Weapon";
    case INVEN_BOW:
        return "Bow";
    case INVEN_STAFF:
        return "Walking staff";
    case EQUIPMENT_MENU_RINGS:
        return "Rings";
    case INVEN_NECK:
        return "Amulet";
    case INVEN_LITE:
        return "Light";
    case INVEN_BODY:
        return "Body armour";
    case INVEN_OUTER:
        return "Cloak";
    case INVEN_ARM:
        return "Off-hand";
    case INVEN_HEAD:
        return "Head";
    case INVEN_HANDS:
        return "Hands";
    case INVEN_FEET:
        return "Feet";
    case EQUIPMENT_MENU_QUIVERS:
        return "Quivers";
    case INVEN_HORN:
        return "Horn";
    case INVEN_LEFT:
        return "Left ring";
    case INVEN_RIGHT:
        return "Right ring";
    case INVEN_QUIVER1:
        return "1st quiver";
    case INVEN_QUIVER2:
        return "2nd quiver";
    default:
        return "Slot";
    }
}

static bool equipment_menu_slot_group_contains(int group, int slot)
{
    switch (group)
    {
    case EQUIPMENT_MENU_RINGS:
        return slot == INVEN_LEFT || slot == INVEN_RIGHT;
    case EQUIPMENT_MENU_QUIVERS:
        return slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2;
    default:
        return false;
    }
}

static bool equipment_menu_slot_is_group(int slot)
{
    return slot == EQUIPMENT_MENU_RINGS || slot == EQUIPMENT_MENU_QUIVERS;
}

static int equipment_slot_group_width(void)
{
    int width = 0;

    for (int i = 0; i < EQUIPMENT_MENU_SLOT_COUNT; i++)
    {
        int len = (int)strlen(equipment_slot_text(equipment_menu_slots[i])) + 5;
        if (len > width)
            width = len;
    }

    return width + 2;
}

static bool equipment_object_is_wearable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
        return false;

    return wield_slot(o_ptr) >= INVEN_WIELD;
}

static bool equipment_slot_accepts_object(int slot, const object_type* o_ptr)
{
    int natural_slot;

    if (!equipment_object_is_wearable(o_ptr))
        return false;

    natural_slot = wield_slot(o_ptr);
    if (natural_slot == slot)
        return true;

    if (slot == EQUIPMENT_MENU_RINGS)
        return o_ptr->tval == TV_RING;

    if (slot == EQUIPMENT_MENU_QUIVERS)
        return (o_ptr->tval == TV_ARROW)
            || player_can_treat_as_throwing(o_ptr);

    switch (slot)
    {
    case INVEN_WIELD:
        return player_can_treat_as_throwing(o_ptr);
    case INVEN_LEFT:
    case INVEN_RIGHT:
        return o_ptr->tval == TV_RING;
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
        return (o_ptr->tval == TV_ARROW)
            || player_can_treat_as_throwing(o_ptr);
    default:
        return false;
    }
}

static void equipment_entry_clear(equipment_list_entry* entry)
{
    if (!entry)
        return;

    entry->item_idx = -1;
    entry->supply_idx = -1;
    entry->equip_idx = -1;
    entry->floor_idx = -1;
    entry->limit_group = INV_LIMIT_NONE;
    entry->placeholder = EQUIPMENT_ENTRY_PLACEHOLDER_NONE;
    entry->equipped = false;
    entry->show_empty_slot = false;
}

static object_type* equipment_entry_object(const equipment_list_entry* entry)
{
    if (!entry)
        return NULL;

    if (entry->placeholder != EQUIPMENT_ENTRY_PLACEHOLDER_NONE)
        return NULL;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        return &inventory[entry->equip_idx];

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        return &inventory[entry->item_idx];

    if (entry->supply_idx >= 0)
        return supplies_entry_at(entry->supply_idx);

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
        return &o_list[entry->floor_idx];

    return NULL;
}

static bool equipment_add_entry(equipment_list_entry entries[], int* count,
    int capacity, int item_idx, int supply_idx, int equip_idx, bool equipped)
{
    equipment_list_entry* entry;

    if (!entries || !count || *count >= capacity)
        return false;

    entry = &entries[*count];
    entry->item_idx = item_idx;
    entry->supply_idx = supply_idx;
    entry->equip_idx = equip_idx;
    entry->floor_idx = -1;
    entry->limit_group = INV_LIMIT_NONE;
    entry->placeholder = EQUIPMENT_ENTRY_PLACEHOLDER_NONE;
    entry->equipped = equipped;
    entry->show_empty_slot = false;
    (*count)++;
    return true;
}

/*
 * Append a row for an equipment slot the player may place an item into.
 * Unlike equipment_add_entry, an empty destination slot still produces a
 * visible, selectable row (see show_empty_slot).
 */
static bool equipment_add_slot_entry(equipment_list_entry entries[], int* count,
    int capacity, int slot)
{
    equipment_list_entry* entry;
    bool occupied;

    if (!entries || !count || *count >= capacity)
        return false;

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return false;

    occupied = (inventory[slot].k_idx != 0);

    entry = &entries[*count];
    equipment_entry_clear(entry);
    entry->equip_idx = slot;
    entry->equipped = occupied;
    entry->show_empty_slot = !occupied;
    (*count)++;
    return true;
}

/* Append a row backed by a floor item lying under the player. */
static bool equipment_add_floor_entry(equipment_list_entry entries[],
    int* count, int capacity, int floor_idx)
{
    equipment_list_entry* entry;

    if (!entries || !count || *count >= capacity)
        return false;

    entry = &entries[*count];
    entry->item_idx = -1;
    entry->supply_idx = -1;
    entry->equip_idx = -1;
    entry->floor_idx = floor_idx;
    entry->limit_group = INV_LIMIT_NONE;
    entry->placeholder = EQUIPMENT_ENTRY_PLACEHOLDER_NONE;
    entry->equipped = false;
    (*count)++;
    return true;
}

static bool equipment_add_limit_placeholder(equipment_list_entry entries[],
    int* count, int capacity, enum inventory_limit_group limit_group,
    equipment_entry_placeholder placeholder)
{
    equipment_list_entry* entry;

    if (!entries || !count || *count >= capacity)
        return false;

    if (placeholder == EQUIPMENT_ENTRY_PLACEHOLDER_NONE)
        return false;

    entry = &entries[*count];
    equipment_entry_clear(entry);
    entry->limit_group = limit_group;
    entry->placeholder = placeholder;
    (*count)++;
    return true;
}

static int collect_equipment_entries_for_slot(int slot,
    equipment_list_entry entries[], int capacity)
{
    int count = 0;

    if (!entries || capacity <= 0)
        return 0;

    for (int i = 0; i < capacity; i++)
        equipment_entry_clear(&entries[i]);

    if (slot == EQUIPMENT_MENU_ALL)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < capacity; i++)
        {
            if (inventory[i].k_idx)
                equipment_add_entry(entries, &count, capacity, -1, -1, i,
                    true);
        }

        return count;
    }

    if (equipment_menu_slot_is_group(slot))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < capacity; i++)
        {
            if (equipment_menu_slot_group_contains(slot, i)
                && inventory[i].k_idx)
            {
                equipment_add_entry(entries, &count, capacity, -1, -1, i,
                    true);
            }
        }
    }
    else if (slot >= INVEN_WIELD && slot < INVEN_TOTAL
        && inventory[slot].k_idx)
    {
        equipment_add_entry(entries, &count, capacity, -1, -1, slot, true);
    }

    for (int i = 0; i < INVEN_PACK && count < capacity; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!equipment_slot_accepts_object(slot, o_ptr))
            continue;

        equipment_add_entry(entries, &count, capacity, i, -1, -1, false);
    }

    for (int i = 0; i < supplies_entry_count() && count < capacity; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (!equipment_slot_accepts_object(slot, o_ptr))
            continue;

        equipment_add_entry(entries, &count, capacity, SUPPLIES_INDEX, i, -1,
            false);
    }

    /* Floor items under the player that could fill this slot. */
    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (int i = 0; i < floor_num && count < capacity; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr;

            if (o_idx <= 0 || o_idx >= o_max)
                continue;

            o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || supplies_is_supply_object(o_ptr)
                || !equipment_slot_accepts_object(slot, o_ptr))
                continue;

            equipment_add_floor_entry(entries, &count, capacity, o_idx);
        }
    }

    return count;
}

static int count_equipment_entries_for_slot(int slot)
{
    int count = 0;

    if (slot == EQUIPMENT_MENU_ALL)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (inventory[i].k_idx)
                count++;
        }

        return count;
    }

    if (equipment_menu_slot_is_group(slot))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (equipment_menu_slot_group_contains(slot, i)
                && inventory[i].k_idx)
            {
                count++;
            }
        }
    }
    else if (slot >= INVEN_WIELD && slot < INVEN_TOTAL
        && inventory[slot].k_idx)
    {
        count++;
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        if (equipment_slot_accepts_object(slot, &inventory[i]))
            count++;
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (equipment_slot_accepts_object(slot, o_ptr))
            count++;
    }

    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (int i = 0; i < floor_num; i++)
        {
            int o_idx = floor_list[i];

            if (o_idx <= 0 || o_idx >= o_max)
                continue;

            if (!supplies_is_supply_object(&o_list[o_idx])
                && equipment_slot_accepts_object(slot, &o_list[o_idx]))
                count++;
        }
    }

    return count;
}

static bool equipment_prepare_icon_from_kind(int slot, object_type* icon_obj)
{
    if (!icon_obj)
        return false;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        object_type fake;

        if (!k_ptr->name)
            continue;

        object_wipe(&fake);
        object_prep(&fake, i);
        fake.ident |= IDENT_KNOWN;

        if (!equipment_slot_accepts_object(slot, &fake))
            continue;

        object_copy(icon_obj, &fake);
        return true;
    }

    return false;
}

static void prepare_equipment_group_icons(
    supply_group_icon icons[EQUIPMENT_MENU_SLOT_COUNT])
{
    for (int group = 0; group < EQUIPMENT_MENU_SLOT_COUNT; group++)
    {
        int slot = equipment_menu_slots[group];
        object_type* icon_obj = &icons[group].obj;

        object_wipe(icon_obj);
        icons[group].has_icon = false;

        if (slot == EQUIPMENT_MENU_ALL)
        {
            for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
            {
                if (!inventory[i].k_idx)
                    continue;

                object_copy(icon_obj, &inventory[i]);
                icons[group].has_icon = true;
                break;
            }

            continue;
        }

        if (equipment_menu_slot_is_group(slot))
        {
            for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
            {
                if (!equipment_menu_slot_group_contains(slot, i)
                    || !inventory[i].k_idx)
                {
                    continue;
                }

                object_copy(icon_obj, &inventory[i]);
                icons[group].has_icon = true;
                break;
            }

            if (icons[group].has_icon)
                continue;
        }

        if (slot >= INVEN_WIELD && slot < INVEN_TOTAL
            && inventory[slot].k_idx)
        {
            object_copy(icon_obj, &inventory[slot]);
            icons[group].has_icon = true;
            continue;
        }

        for (int i = 0; i < INVEN_PACK; i++)
        {
            if (!equipment_slot_accepts_object(slot, &inventory[i]))
                continue;

            object_copy(icon_obj, &inventory[i]);
            icons[group].has_icon = true;
            break;
        }

        if (icons[group].has_icon)
            continue;

        for (int i = 0; i < supplies_entry_count(); i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (!equipment_slot_accepts_object(slot, o_ptr))
                continue;

            object_copy(icon_obj, o_ptr);
            icons[group].has_icon = true;
            break;
        }

        if (icons[group].has_icon)
            continue;

        {
            int floor_list[MAX_FLOOR_STACK];
            int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK,
                p_ptr->py, p_ptr->px, 0x00);

            for (int i = 0; i < floor_num; i++)
            {
                int o_idx = floor_list[i];
                object_type* o_ptr;

                if (o_idx <= 0 || o_idx >= o_max)
                    continue;

                o_ptr = &o_list[o_idx];
                if (!o_ptr->k_idx || supplies_is_supply_object(o_ptr)
                    || !equipment_slot_accepts_object(slot, o_ptr))
                {
                    continue;
                }

                object_copy(icon_obj, o_ptr);
                icons[group].has_icon = true;
                break;
            }
        }

        if (!icons[group].has_icon
            && equipment_prepare_icon_from_kind(slot, icon_obj))
        {
            icons[group].has_icon = true;
        }
    }
}

static void compute_equipment_group_totals(
    int totals[EQUIPMENT_MENU_SLOT_COUNT])
{
    for (int group = 0; group < EQUIPMENT_MENU_SLOT_COUNT; group++)
        totals[group] = count_equipment_entries_for_slot(
            equipment_menu_slots[group]);
}

static bool equipment_menu_slot_is_filled(int slot)
{
    if (slot == EQUIPMENT_MENU_ALL)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (inventory[i].k_idx)
                return true;
        }

        return false;
    }

    if (equipment_menu_slot_is_group(slot))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (equipment_menu_slot_group_contains(slot, i)
                && inventory[i].k_idx)
            {
                return true;
            }
        }

        return false;
    }

    return slot >= INVEN_WIELD && slot < INVEN_TOTAL
        && inventory[slot].k_idx;
}

static void display_equipment_group_list(int col, int row, int wid,
    int selection_w, int per_page, int grp_cur, int grp_top,
    const int totals[EQUIPMENT_MENU_SLOT_COUNT],
    const supply_group_icon icons[EQUIPMENT_MENU_SLOT_COUNT], bool active)
{
    int total_col = col + wid - 3;
    int text_col = col + (use_bigtile ? 2 : 1);
    int text_w = total_col - text_col;
    int erase_w = (selection_w > wid) ? selection_w : wid;

    for (int i = 0; i < per_page; i++)
    {
        int grp_pos = grp_top + i;
        int slot;
        byte attr;
        char buf[8];
        bool selected;
        bool highlighted;

        Term_erase(col, row + i, erase_w);

        if (grp_pos >= EQUIPMENT_MENU_SLOT_COUNT)
            continue;

        slot = equipment_menu_slots[grp_pos];
        selected = (grp_pos == grp_cur);
        highlighted = selected && active;

        if (highlighted)
            attr = supply_browser_selected_attr(TERM_L_BLUE);
        else if (selected && equipment_menu_slot_is_filled(slot))
            attr = TERM_WHITE;
        else if (selected)
            attr = TERM_L_BLUE;
        else if (totals[grp_pos] == 0)
            attr = TERM_L_DARK;
        else if (equipment_menu_slot_is_filled(slot))
            attr = TERM_WHITE;
        else
            attr = TERM_L_BLUE;

        if (highlighted)
            supply_browser_fill_row(col, row + i, selection_w, attr);
        if (icons && icons[grp_pos].has_icon)
            draw_supply_icon(col, row + i, &icons[grp_pos].obj);

        if (text_w > 0)
            supply_put_fitted(text_col, row + i, text_w, attr,
                equipment_slot_text(slot));

        strnfmt(buf, sizeof(buf), "%3d", totals[grp_pos]);
        if (wid >= 3)
            supply_put_fitted(total_col, row + i, 3, attr, buf);
    }
}

static cptr equipment_entry_source_text(const equipment_list_entry* entry,
    char* buf, size_t buflen)
{
    if (!entry)
        return "";

    if (entry->placeholder != EQUIPMENT_ENTRY_PLACEHOLDER_NONE)
    {
        cptr name = inventory_limit_group_name(entry->limit_group);
        return (name && name[0]) ? name : "Limit";
    }

    if (entry->equipped)
    {
        if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
            return equipment_slot_text(entry->equip_idx);

        return "Equipped";
    }

    if (entry->supply_idx >= 0)
        return "Supply";

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        if (buf && buflen > 0)
            strnfmt(buf, buflen, "Pack %c", index_to_label(entry->item_idx));
        return buf ? buf : "Pack";
    }

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
    {
        if (buf && buflen > 0)
        {
            strnfmt(buf, buflen, "Floor");
            return buf;
        }
        return "Floor";
    }

    return "";
}

static int equipment_entry_source_column_width(
    const knowledge_browser_layout* layout, equipment_list_entry entries[],
    int entry_cnt, int entry_top, int per_page, bool show_source)
{
    int width = 0;
    int max_width;
    int sym_w = use_bigtile ? 2 : 1;
    int name_col;

    if (!show_source || !layout || !entries || layout->term_wid < 64)
        return 0;

    name_col = layout->list_col + sym_w + 1;
    max_width = layout->term_wid - name_col - 8;
    if (max_width < (int)strlen("Where"))
        return 0;

    width = (int)strlen("Where");

    for (int i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        char source_buf[24];
        cptr source;
        int len;

        if (idx < 0 || idx >= entry_cnt)
            continue;

        source = equipment_entry_source_text(&entries[idx], source_buf,
            sizeof(source_buf));
        if (!source || !source[0])
            continue;

        len = (int)strlen(source);
        if (len > width)
            width = len;
    }

    if (width > max_width)
        width = max_width;

    return width;
}

static void display_equipment_slot_entries(
    const knowledge_browser_layout* layout, int row, int per_page,
    equipment_list_entry entries[], int entry_cnt, int entry_cur,
    int entry_top, int column, bool show_source)
{
    int sym_w = use_bigtile ? 2 : 1;
    int source_w = equipment_entry_source_column_width(layout, entries,
        entry_cnt, 0, entry_cnt, show_source);
    int source_col = layout->term_wid - source_w;
    int name_col = layout->list_col + sym_w + 1;
    int name_w = layout->term_wid - name_col - (source_w ? source_w + 1 : 0);

    if (name_w < 1)
        name_w = 1;

    for (int i = 0; i < per_page; i++)
    {
        int idx = entry_top + i;
        int y = row + i;
        equipment_list_entry* entry;
        object_type* o_ptr;
        byte base_attr;
        byte attr;
        char name[160];
        char display_name[180];
        char label_prefix[8];
        char source_buf[24];
        bool selected;

        Term_erase(layout->list_col, y, 255);

        if (idx >= entry_cnt)
            continue;

        entry = &entries[idx];
        o_ptr = equipment_entry_object(entry);
        if (entry->placeholder != EQUIPMENT_ENTRY_PLACEHOLDER_NONE)
        {
            cptr placeholder_text =
                (entry->placeholder == EQUIPMENT_ENTRY_PLACEHOLDER_RESERVED)
                    ? "(reserved)"
                    : "(empty space)";

            selected = (column == 1 && idx == entry_cur);
            base_attr = TERM_L_DARK;
            attr = selected ? supply_browser_selected_attr(base_attr)
                            : base_attr;
            browser_entry_label_prefix(label_prefix, sizeof(label_prefix),
                idx);
            strnfmt(display_name, sizeof(display_name), "%s%s",
                label_prefix, placeholder_text);

            if (selected)
            {
                int selection_w = supply_browser_selection_width(
                    layout->list_col, name_col, name_w, display_name,
                    layout->list_w);
                supply_browser_fill_row(layout->list_col, y, selection_w,
                    attr);
            }

            supply_put_fitted(name_col, y, name_w, attr, display_name);
            if (source_w > 0)
                supply_put_fitted(source_col, y, source_w, base_attr,
                    equipment_entry_source_text(entry, source_buf,
                        sizeof(source_buf)));
            continue;
        }
        if ((!o_ptr || !o_ptr->k_idx) && entry->show_empty_slot
            && entry->equip_idx >= INVEN_WIELD
            && entry->equip_idx < INVEN_TOTAL)
        {
            selected = (column == 1 && idx == entry_cur);
            base_attr = TERM_L_DARK;
            attr = selected ? supply_browser_selected_attr(base_attr)
                            : base_attr;
            browser_entry_label_prefix(label_prefix, sizeof(label_prefix),
                idx);
            strnfmt(display_name, sizeof(display_name), "%s(empty) %s",
                label_prefix, equipment_slot_text(entry->equip_idx));

            if (selected)
            {
                int selection_w = supply_browser_selection_width(
                    layout->list_col, name_col, name_w, display_name,
                    layout->list_w);
                supply_browser_fill_row(layout->list_col, y, selection_w,
                    attr);
            }

            supply_put_fitted(name_col, y, name_w, attr, display_name);
            if (source_w > 0)
                supply_put_fitted(source_col, y, source_w, base_attr,
                    equipment_slot_text(entry->equip_idx));
            continue;
        }
        if (!o_ptr || !o_ptr->k_idx)
            continue;

        selected = (column == 1 && idx == entry_cur);
        base_attr = entry->equipped ? TERM_WHITE
            : object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        attr = selected ? supply_browser_selected_attr(base_attr) : base_attr;

        object_desc(name, sizeof(name), o_ptr, true, 3);
        if (entry->equipped)
            SDL_strlcat(name, " [equipped]", sizeof(name));
        else if (entry->floor_idx > 0 && entry->floor_idx < o_max
            && source_w <= 0)
        {
            SDL_strlcat(name, " [floor]", sizeof(name));
        }
        browser_entry_label_prefix(label_prefix, sizeof(label_prefix), idx);
        strnfmt(display_name, sizeof(display_name), "%s%s", label_prefix,
            name);

        if (selected)
        {
            int selection_w = supply_browser_selection_width(layout->list_col,
                name_col, name_w, display_name, layout->list_w);
            supply_browser_fill_row(layout->list_col, y, selection_w, attr);
        }

        draw_supply_icon(layout->list_col, y, o_ptr);

        supply_put_fitted(name_col, y, name_w, attr, display_name);
        if (source_w > 0)
            supply_put_fitted(source_col, y, source_w, base_attr,
                equipment_entry_source_text(entry, source_buf,
                    sizeof(source_buf)));
    }

    if (entry_cnt == 0 && per_page > 0)
    {
        Term_erase(layout->list_col, row, 255);
        supply_put_fitted(layout->list_col, row, layout->list_w, TERM_L_DARK,
            "(nothing available)");
    }
}

static int equipment_menu_compare_slots(int selected_slot, int slots[],
    int max_slots)
{
    int count = 0;

    if (!slots || max_slots <= 0)
        return 0;

    if (equipment_menu_slot_is_group(selected_slot))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_slots; i++)
        {
            if (equipment_menu_slot_group_contains(selected_slot, i)
                && inventory[i].k_idx)
            {
                slots[count++] = i;
            }
        }

        return count;
    }

    if (selected_slot >= INVEN_WIELD && selected_slot < INVEN_TOTAL
        && inventory[selected_slot].k_idx)
    {
        slots[count++] = selected_slot;
    }

    return count;
}

/*
 * Find an item lying on the floor under the player that would occupy the same
 * slot as 'ref' (so it can be compared against it). Returns NULL when the
 * player is not standing on a comparable item.
 */
static object_type* equipment_menu_floor_item_for_object(const object_type* ref)
{
    int slot;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!ref || !ref->k_idx)
        return NULL;

    slot = wield_slot(ref);
    if (slot < 0)
        return NULL;

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00);

    for (int i = 0; i < floor_num; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr;

        if (o_idx <= 0 || o_idx >= o_max)
            continue;

        o_ptr = &o_list[o_idx];
        if (!o_ptr->k_idx || o_ptr == ref)
            continue;

        if (equipment_slot_accepts_object(slot, o_ptr))
            return o_ptr;
    }

    return NULL;
}

static bool equipment_menu_show_entry_description(equipment_list_entry* entry,
    int selected_slot, bool overlay)
{
    object_type* o_ptr;

    if (!entry)
        return false;

    o_ptr = equipment_entry_object(entry);
    if (!o_ptr || !o_ptr->k_idx)
    {
        if (!overlay)
        {
            bell("Nothing to recall.");
            msg_print("Nothing to recall.");
        }
        return false;
    }

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        (void)player_try_identify_smithing_object_on_examine(o_ptr, true);
    else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        (void)player_try_identify_smithing_object_on_examine(o_ptr, false);

    {
        int compare_slots[2];
        int compare_count =
            equipment_menu_compare_slots(selected_slot, compare_slots, 2);
        const object_type* objects[3];
        const char* headings[3];
        char heading_texts[3][32];
        int count = 0;
        object_type* floor_ptr;

        strnfmt(heading_texts[count], sizeof(heading_texts[count]),
            "Selected item");
        headings[count] = heading_texts[count];
        objects[count++] = o_ptr;

        for (int i = 0; i < compare_count && count < 3; i++)
        {
            int slot = compare_slots[i];

            /* The selected entry is itself this equipped item. */
            if (&inventory[slot] == o_ptr)
                continue;

            strnfmt(heading_texts[count], sizeof(heading_texts[count]),
                "%s", equipment_slot_text(slot));
            headings[count] = heading_texts[count];
            objects[count++] = &inventory[slot];
        }

        /* Shortcut: if standing on a comparable item, line it up too. */
        floor_ptr = equipment_menu_floor_item_for_object(o_ptr);
        if (floor_ptr && count < 3)
        {
            strnfmt(heading_texts[count], sizeof(heading_texts[count]),
                "On floor");
            headings[count] = heading_texts[count];
            objects[count++] = floor_ptr;
        }

        if (count > 1)
        {
            if (overlay)
                return object_info_overlay_show_multi(objects, headings,
                    count);
            object_info_screen_multi(objects, headings, count);
            return true;
        }

        if (overlay)
        {
            const object_type* single[1] = { o_ptr };
            return object_info_overlay_show_multi(single, NULL, 1);
        }
        object_info_screen(o_ptr);
    }
    return true;
}

static bool equipment_menu_overlay_entry(equipment_list_entry* entry,
    int selected_slot)
{
    return equipment_menu_show_entry_description(entry, selected_slot, true);
}

static bool equipment_menu_use_entry(equipment_list_entry* entry,
    int selected_slot, supply_floor_action floor_action)
{
    object_type* o_ptr;

    if (!entry)
        return false;

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
        return floor_entry_perform_action(entry->floor_idx, floor_action,
            equipment_menu_slot_is_group(selected_slot) ? -1 : selected_slot);

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        do_cmd_use_item_by_index(entry->equip_idx);
        return true;
    }

    o_ptr = equipment_entry_object(entry);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (entry->supply_idx >= 0)
    {
        supplies_begin_action(entry->supply_idx);
        if (equipment_menu_slot_is_group(selected_slot))
            do_cmd_wield(o_ptr, SUPPLIES_INDEX);
        else
            do_cmd_wield_to_slot(o_ptr, SUPPLIES_INDEX, selected_slot);
        supplies_end_action();
        return true;
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        if (equipment_menu_slot_is_group(selected_slot))
            do_cmd_wield(o_ptr, entry->item_idx);
        else
            do_cmd_wield_to_slot(o_ptr, entry->item_idx, selected_slot);
        return true;
    }

    return false;
}

static bool equipment_menu_drop_entry(equipment_list_entry* entry)
{
    object_type* o_ptr;

    if (!entry)
        return false;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        do_cmd_drop_item_by_index(entry->equip_idx);
        return true;
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        do_cmd_drop_item_by_index(entry->item_idx);
        return true;
    }

    if (entry->supply_idx < 0)
        return false;

    o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx || o_ptr->number <= 0)
        return false;

    {
        int actual_amt = get_quantity(NULL, o_ptr->number);
        bool dropped;

        if (actual_amt <= 0)
            return false;

        supplies_begin_action(entry->supply_idx);
        dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
        supplies_end_action();

        if (dropped)
            handle_stuff();

        return dropped;
    }
}

static const inventory_menu_group inventory_browser_groups[] = {
    INVENTORY_MENU_GROUP_ALL,
    INVENTORY_MENU_GROUP_RINGS,
    INVENTORY_MENU_GROUP_AMULETS,
    INVENTORY_MENU_GROUP_WEAPONS,
    INVENTORY_MENU_GROUP_THROWABLES,
    INVENTORY_MENU_GROUP_BOWS,
    INVENTORY_MENU_GROUP_ARROWS,
    INVENTORY_MENU_GROUP_ARMOUR,
    INVENTORY_MENU_GROUP_CLOAKS,
    INVENTORY_MENU_GROUP_SHIELDS,
    INVENTORY_MENU_GROUP_HEADGEAR,
    INVENTORY_MENU_GROUP_GLOVES,
    INVENTORY_MENU_GROUP_BOOTS,
    INVENTORY_MENU_GROUP_LIGHTS,
    INVENTORY_MENU_GROUP_STAVES,
    INVENTORY_MENU_GROUP_HORNS,
    INVENTORY_MENU_GROUP_DIGGING,
    INVENTORY_MENU_GROUP_OTHER
};

#define INVENTORY_BROWSER_GROUP_COUNT ((int)N_ELEMENTS(inventory_browser_groups))

static cptr inventory_browser_group_text(inventory_menu_group group)
{
    switch (group)
    {
    case INVENTORY_MENU_GROUP_ALL:
        return "All inventory";
    case INVENTORY_MENU_GROUP_RINGS:
        return "Rings";
    case INVENTORY_MENU_GROUP_AMULETS:
        return "Amulets";
    case INVENTORY_MENU_GROUP_WEAPONS:
        return "Weapons";
    case INVENTORY_MENU_GROUP_THROWABLES:
        return "Throwables";
    case INVENTORY_MENU_GROUP_BOWS:
        return "Bows";
    case INVENTORY_MENU_GROUP_ARROWS:
        return "Arrows";
    case INVENTORY_MENU_GROUP_ARMOUR:
        return "Armour";
    case INVENTORY_MENU_GROUP_CLOAKS:
        return "Cloaks/robes";
    case INVENTORY_MENU_GROUP_SHIELDS:
        return "Shields";
    case INVENTORY_MENU_GROUP_HEADGEAR:
        return "Headgear";
    case INVENTORY_MENU_GROUP_GLOVES:
        return "Gloves";
    case INVENTORY_MENU_GROUP_BOOTS:
        return "Boots";
    case INVENTORY_MENU_GROUP_LIGHTS:
        return "Lights/oil";
    case INVENTORY_MENU_GROUP_STAVES:
        return "Staves";
    case INVENTORY_MENU_GROUP_HORNS:
        return "Horns";
    case INVENTORY_MENU_GROUP_DIGGING:
        return "Digging";
    case INVENTORY_MENU_GROUP_OTHER:
        return "Other";
    default:
        return "";
    }
}

static bool inventory_browser_object_matches_group(
    inventory_menu_group group, const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (group)
    {
    case INVENTORY_MENU_GROUP_RINGS:
        return o_ptr->tval == TV_RING;
    case INVENTORY_MENU_GROUP_AMULETS:
        return o_ptr->tval == TV_AMULET;
    case INVENTORY_MENU_GROUP_WEAPONS:
        return inventory_limit_group_for_object(o_ptr) != INV_LIMIT_THROWABLE
            && (o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_POLEARM
                || o_ptr->tval == TV_SWORD);
    case INVENTORY_MENU_GROUP_THROWABLES:
        return inventory_limit_group_for_object(o_ptr) == INV_LIMIT_THROWABLE;
    case INVENTORY_MENU_GROUP_BOWS:
        return o_ptr->tval == TV_BOW;
    case INVENTORY_MENU_GROUP_ARROWS:
        return o_ptr->tval == TV_ARROW;
    case INVENTORY_MENU_GROUP_ARMOUR:
        return o_ptr->tval == TV_MAIL
            || (o_ptr->tval == TV_SOFT_ARMOR && o_ptr->sval != SV_ROBE);
    case INVENTORY_MENU_GROUP_CLOAKS:
        return o_ptr->tval == TV_CLOAK
            || (o_ptr->tval == TV_SOFT_ARMOR && o_ptr->sval == SV_ROBE);
    case INVENTORY_MENU_GROUP_SHIELDS:
        return o_ptr->tval == TV_SHIELD;
    case INVENTORY_MENU_GROUP_HEADGEAR:
        return o_ptr->tval == TV_HELM || o_ptr->tval == TV_CROWN;
    case INVENTORY_MENU_GROUP_GLOVES:
        return o_ptr->tval == TV_GLOVES;
    case INVENTORY_MENU_GROUP_BOOTS:
        return o_ptr->tval == TV_BOOTS;
    case INVENTORY_MENU_GROUP_LIGHTS:
        return o_ptr->tval == TV_LIGHT || o_ptr->tval == TV_FLASK
            || player_oil_container_object(o_ptr);
    case INVENTORY_MENU_GROUP_STAVES:
        return o_ptr->tval == TV_STAFF;
    case INVENTORY_MENU_GROUP_HORNS:
        return o_ptr->tval == TV_HORN;
    case INVENTORY_MENU_GROUP_DIGGING:
        return o_ptr->tval == TV_DIGGING;
    case INVENTORY_MENU_GROUP_OTHER:
        for (int i = 1; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
        {
            inventory_menu_group candidate = inventory_browser_groups[i];

            if (candidate == INVENTORY_MENU_GROUP_OTHER)
                continue;
            if (inventory_browser_object_matches_group(candidate, o_ptr))
                return false;
        }
        return true;
    default:
        return false;
    }
}

static bool inventory_browser_equipped_slot_matches_group(
    inventory_menu_group group, int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL || !inventory[slot].k_idx)
        return false;

    return inventory_browser_object_matches_group(group, &inventory[slot]);
}

#define INVENTORY_SELECT_INVALID (-1000000)

static bool inventory_item_select_active(const supply_menu_request* request)
{
    return request && request->item_select_mode && request->item_select_item_out;
}

static int inventory_select_entry_item(const equipment_list_entry* entry)
{
    if (!entry)
        return -1;

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
        return 0 - entry->floor_idx;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        return entry->equip_idx;

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        return entry->item_idx;

    return INVENTORY_SELECT_INVALID;
}

static bool inventory_select_item_allowed(
    const supply_menu_request* request, int item)
{
    int flags;

    if (!inventory_item_select_active(request))
        return true;

    flags = request->item_select_flags;

    if (item < 0)
    {
        if (!(flags & USE_FLOOR))
            return false;
    }
    else if (item >= INVEN_WIELD && item < INVEN_TOTAL)
    {
        if (!(flags & USE_EQUIP))
            return false;
    }
    else if (item >= 0 && item < INVEN_PACK)
    {
        if (!(flags & USE_INVEN))
            return false;
    }
    else
    {
        return false;
    }

    return get_item_okay(item);
}

static int inventory_browser_group_width(void)
{
    int width = 0;

    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
    {
        int len = (int)strlen(
            inventory_browser_group_text(inventory_browser_groups[i])) + 5;
        if (len > width)
            width = len;
    }

    return width + 2;
}

static bool inventory_browser_group_contains_limit(
    inventory_menu_group group, enum inventory_limit_group limit_group)
{
    static const enum inventory_limit_group armour[] = {
        INV_LIMIT_SOFT_ARMOUR,
        INV_LIMIT_MAIL
    };
    static const enum inventory_limit_group shields[] = {
        INV_LIMIT_ROUND_SHIELD,
        INV_LIMIT_OTHER_SHIELD
    };
    static const enum inventory_limit_group lights[] = {
        INV_LIMIT_TORCHES,
        INV_LIMIT_BRASS_LAMPS,
        INV_LIMIT_LESSER_JEWEL
    };
    const enum inventory_limit_group* groups = NULL;
    int count = 0;

    switch (group)
    {
    case INVENTORY_MENU_GROUP_WEAPONS:
        return limit_group == INV_LIMIT_MELEE_WEAPON;
    case INVENTORY_MENU_GROUP_THROWABLES:
        return limit_group == INV_LIMIT_THROWABLE;
    case INVENTORY_MENU_GROUP_BOWS:
        return limit_group == INV_LIMIT_BOW;
    case INVENTORY_MENU_GROUP_ARROWS:
        return limit_group == INV_LIMIT_ARROW;
    case INVENTORY_MENU_GROUP_ARMOUR:
        groups = armour;
        count = (int)N_ELEMENTS(armour);
        break;
    case INVENTORY_MENU_GROUP_CLOAKS:
        return limit_group == INV_LIMIT_CLOAK;
    case INVENTORY_MENU_GROUP_SHIELDS:
        groups = shields;
        count = (int)N_ELEMENTS(shields);
        break;
    case INVENTORY_MENU_GROUP_HEADGEAR:
        return limit_group == INV_LIMIT_HELM_CROWN;
    case INVENTORY_MENU_GROUP_GLOVES:
        return limit_group == INV_LIMIT_GLOVES;
    case INVENTORY_MENU_GROUP_BOOTS:
        return limit_group == INV_LIMIT_BOOTS;
    case INVENTORY_MENU_GROUP_LIGHTS:
        groups = lights;
        count = (int)N_ELEMENTS(lights);
        break;
    case INVENTORY_MENU_GROUP_STAVES:
        return limit_group == INV_LIMIT_STAFF;
    case INVENTORY_MENU_GROUP_HORNS:
        return limit_group == INV_LIMIT_HORN;
    case INVENTORY_MENU_GROUP_DIGGING:
        return limit_group == INV_LIMIT_DIGGING;
    default:
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        if (groups[i] == limit_group)
            return true;
    }

    return false;
}

inventory_menu_group inventory_menu_group_for_limit_group(
    enum inventory_limit_group limit_group)
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
    {
        inventory_menu_group group = inventory_browser_groups[i];

        if (inventory_browser_group_contains_limit(group, limit_group))
            return group;
    }

    return INVENTORY_MENU_GROUP_ALL;
}

static int inventory_browser_group_index(inventory_menu_group group)
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
    {
        if (inventory_browser_groups[i] == group)
            return i;
    }

    return 0;
}

static void inventory_browser_group_limit_status(inventory_menu_group group,
    char* buf, size_t buflen)
{
    bool first = true;

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    for (int i = INV_LIMIT_ARROW; i <= INV_LIMIT_FEANORIAN_LAMP; i++)
    {
        enum inventory_limit_group limit_group = (enum inventory_limit_group)i;
        char part[48];
        int limit;
        int used;

        if (!inventory_browser_group_contains_limit(group, limit_group))
            continue;

        limit = inventory_limit_limit_for_group(limit_group);
        if (limit < 0)
            continue;

        used = inventory_limit_usage_for_group(limit_group);
        strnfmt(part, sizeof(part), "%s%s %d/%d",
            first ? "" : ", ", inventory_limit_group_name(limit_group),
            used, limit);
        SDL_strlcat(buf, part, buflen);
        first = false;
    }
}

static void inventory_browser_group_status(inventory_menu_group group,
    int entry_cnt, char* buf, size_t buflen)
{
    char limits[160];

    if (!buf || buflen == 0)
        return;

    if (group == INVENTORY_MENU_GROUP_ALL)
    {
        strnfmt(buf, buflen, "%d choice%s. Pack slots %d/%d.",
            entry_cnt, (entry_cnt == 1) ? "" : "s", p_ptr->inven_cnt,
            INVEN_PACK);
        return;
    }

    inventory_browser_group_limit_status(group, limits, sizeof(limits));
    if (limits[0])
    {
        strnfmt(buf, buflen, "%s: %d choice%s. Limits: %s.",
            inventory_browser_group_text(group), entry_cnt,
            (entry_cnt == 1) ? "" : "s", limits);
    }
    else
    {
        strnfmt(buf, buflen, "%s: %d choice%s.",
            inventory_browser_group_text(group), entry_cnt,
            (entry_cnt == 1) ? "" : "s");
    }
}

static int count_inventory_browser_group_entries(inventory_menu_group group)
{
    int count = 0;

    if (group == INVENTORY_MENU_GROUP_ALL)
    {
        for (int i = 0; i < INVEN_PACK; i++)
        {
            if (inventory[i].k_idx)
                count++;
        }
    }
    else
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (inventory_browser_equipped_slot_matches_group(group, i))
                count++;
        }

        for (int i = 0; i < INVEN_PACK; i++)
        {
            if (inventory_browser_object_matches_group(group, &inventory[i]))
                count++;
        }
    }

    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (int i = 0; i < floor_num; i++)
        {
            int o_idx = floor_list[i];

            if (o_idx <= 0 || o_idx >= o_max)
                continue;

            if (supplies_is_supply_object(&o_list[o_idx]))
                continue;

            if (group == INVENTORY_MENU_GROUP_ALL
                || inventory_browser_object_matches_group(group,
                    &o_list[o_idx]))
            {
                count++;
            }
        }
    }

    return count;
}

static int count_inventory_select_group_entries(inventory_menu_group group,
    const supply_menu_request* request)
{
    int count = 0;

    if (!inventory_item_select_active(request))
        return count_inventory_browser_group_entries(group);

    if (request->item_select_flags & USE_EQUIP)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (!inventory[i].k_idx)
                continue;
            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_equipped_slot_matches_group(group, i))
                continue;
            if (inventory_select_item_allowed(request, i))
                count++;
        }
    }

    if (request->item_select_flags & USE_INVEN)
    {
        for (int i = 0; i < INVEN_PACK; i++)
        {
            if (!inventory[i].k_idx)
                continue;
            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_object_matches_group(group, &inventory[i]))
                continue;
            if (inventory_select_item_allowed(request, i))
                count++;
        }
    }

    if (request->item_select_flags & USE_FLOOR)
    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (int i = 0; i < floor_num; i++)
        {
            int o_idx = floor_list[i];
            int item = 0 - o_idx;

            if (o_idx <= 0 || o_idx >= o_max)
                continue;
            if (!o_list[o_idx].k_idx || supplies_is_supply_object(&o_list[o_idx]))
                continue;
            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_object_matches_group(group, &o_list[o_idx]))
                continue;
            if (inventory_select_item_allowed(request, item))
                count++;
        }
    }

    return count;
}

static void compute_inventory_browser_group_totals(
    int totals[INVENTORY_BROWSER_GROUP_COUNT])
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
        totals[i] = count_inventory_browser_group_entries(
            inventory_browser_groups[i]);
}

static void compute_inventory_select_group_totals(
    int totals[INVENTORY_BROWSER_GROUP_COUNT],
    const supply_menu_request* request)
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
        totals[i] = count_inventory_select_group_entries(
            inventory_browser_groups[i], request);
}

static void prepare_inventory_browser_group_icons(
    supply_group_icon icons[INVENTORY_BROWSER_GROUP_COUNT])
{
    for (int group_idx = 0; group_idx < INVENTORY_BROWSER_GROUP_COUNT;
         group_idx++)
    {
        inventory_menu_group group = inventory_browser_groups[group_idx];
        object_type* icon_obj = &icons[group_idx].obj;

        object_wipe(icon_obj);
        icons[group_idx].has_icon = false;

        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (!inventory_browser_equipped_slot_matches_group(group, i))
                continue;

            object_copy(icon_obj, &inventory[i]);
            icons[group_idx].has_icon = true;
            break;
        }

        if (icons[group_idx].has_icon)
            continue;

        for (int i = 0; i < INVEN_PACK; i++)
        {
            if (!inventory_browser_object_matches_group(group, &inventory[i]))
                continue;

            object_copy(icon_obj, &inventory[i]);
            icons[group_idx].has_icon = true;
            break;
        }

        if (icons[group_idx].has_icon)
            continue;

        {
            int floor_list[MAX_FLOOR_STACK];
            int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK,
                p_ptr->py, p_ptr->px, 0x00);

            for (int i = 0; i < floor_num; i++)
            {
                int o_idx = floor_list[i];
                object_type* o_ptr;

                if (o_idx <= 0 || o_idx >= o_max)
                    continue;

                o_ptr = &o_list[o_idx];
                if (!o_ptr->k_idx || supplies_is_supply_object(o_ptr))
                    continue;

                if (group != INVENTORY_MENU_GROUP_ALL
                    && !inventory_browser_object_matches_group(group, o_ptr))
                {
                    continue;
                }

                object_copy(icon_obj, o_ptr);
                icons[group_idx].has_icon = true;
                break;
            }
        }
    }
}

static void display_inventory_browser_group_list(int col, int row, int wid,
    int selection_w, int per_page, int grp_cur, int grp_top,
    const int totals[INVENTORY_BROWSER_GROUP_COUNT],
    const supply_group_icon icons[INVENTORY_BROWSER_GROUP_COUNT], bool active)
{
    int total_col = col + wid - 3;
    int text_col = col + (use_bigtile ? 2 : 1);
    int text_w = total_col - text_col;
    int erase_w = (selection_w > wid) ? selection_w : wid;

    for (int i = 0; i < per_page; i++)
    {
        int grp_pos = grp_top + i;
        inventory_menu_group group;
        byte attr;
        char buf[8];
        bool selected;
        bool highlighted;

        Term_erase(col, row + i, erase_w);

        if (grp_pos >= INVENTORY_BROWSER_GROUP_COUNT)
            continue;

        group = inventory_browser_groups[grp_pos];
        selected = (grp_pos == grp_cur);
        highlighted = selected && active;

        if (highlighted)
            attr = supply_browser_selected_attr(TERM_L_BLUE);
        else if (selected)
            attr = TERM_L_BLUE;
        else if (totals[grp_pos] == 0)
            attr = TERM_L_DARK;
        else
            attr = TERM_L_BLUE;

        if (highlighted)
            supply_browser_fill_row(col, row + i, selection_w, attr);
        if (icons && icons[grp_pos].has_icon)
            draw_supply_icon(col, row + i, &icons[grp_pos].obj);

        if (text_w > 0)
            supply_put_fitted(text_col, row + i, text_w, attr,
                inventory_browser_group_text(group));

        strnfmt(buf, sizeof(buf), "%3d", totals[grp_pos]);
        if (wid >= 3)
            supply_put_fitted(total_col, row + i, 3, attr, buf);
    }
}

static int inventory_browser_compare_slot_for_entry(inventory_menu_group group,
    const equipment_list_entry* entry)
{
    object_type* o_ptr = equipment_entry_object(entry);

    if (!o_ptr || !o_ptr->k_idx)
        return EQUIPMENT_MENU_ALL;

    switch (group)
    {
    case INVENTORY_MENU_GROUP_RINGS:
        return EQUIPMENT_MENU_RINGS;
    case INVENTORY_MENU_GROUP_AMULETS:
        return INVEN_NECK;
    case INVENTORY_MENU_GROUP_ARMOUR:
        return INVEN_BODY;
    case INVENTORY_MENU_GROUP_CLOAKS:
        return (o_ptr->tval == TV_SOFT_ARMOR && o_ptr->sval == SV_ROBE)
            ? INVEN_BODY
            : INVEN_OUTER;
    case INVENTORY_MENU_GROUP_SHIELDS:
        return INVEN_ARM;
    case INVENTORY_MENU_GROUP_HEADGEAR:
        return INVEN_HEAD;
    case INVENTORY_MENU_GROUP_GLOVES:
        return INVEN_HANDS;
    case INVENTORY_MENU_GROUP_BOOTS:
        return INVEN_FEET;
    case INVENTORY_MENU_GROUP_LIGHTS:
        return INVEN_LITE;
    case INVENTORY_MENU_GROUP_STAVES:
        return INVEN_STAFF;
    case INVENTORY_MENU_GROUP_HORNS:
        return INVEN_HORN;
    case INVENTORY_MENU_GROUP_ARROWS:
    case INVENTORY_MENU_GROUP_THROWABLES:
        return EQUIPMENT_MENU_QUIVERS;
    case INVENTORY_MENU_GROUP_WEAPONS:
    case INVENTORY_MENU_GROUP_BOWS:
    case INVENTORY_MENU_GROUP_DIGGING:
        return wield_slot(o_ptr);
    default:
        return EQUIPMENT_MENU_ALL;
    }
}

static void inventory_browser_add_reserved_limit_rows(inventory_menu_group group,
    const object_type* o_ptr, equipment_list_entry entries[], int* count,
    int capacity)
{
    enum inventory_limit_group limit_group;
    int space;

    if (group == INVENTORY_MENU_GROUP_ALL)
        return;

    if (!inventory_limit_info_for_object(o_ptr, &limit_group, NULL, NULL))
        return;

    if (!inventory_browser_group_contains_limit(group, limit_group))
        return;

    space = inventory_limit_space_for_object(o_ptr);
    for (int i = 1; i < space; i++)
    {
        if (!equipment_add_limit_placeholder(entries, count, capacity,
                limit_group, EQUIPMENT_ENTRY_PLACEHOLDER_RESERVED))
        {
            break;
        }
    }
}

static void inventory_browser_add_empty_limit_rows(inventory_menu_group group,
    equipment_list_entry entries[], int* count, int capacity)
{
    if (group == INVENTORY_MENU_GROUP_ALL)
        return;

    for (int i = INV_LIMIT_ARROW; i <= INV_LIMIT_FEANORIAN_LAMP; i++)
    {
        enum inventory_limit_group limit_group = (enum inventory_limit_group)i;
        int limit;
        int used;

        if (!inventory_browser_group_contains_limit(group, limit_group))
            continue;

        limit = inventory_limit_limit_for_group(limit_group);
        if (limit <= 0)
            continue;

        used = inventory_limit_usage_for_group(limit_group);
        for (int slot = used; slot < limit; slot++)
        {
            if (!equipment_add_limit_placeholder(entries, count, capacity,
                    limit_group, EQUIPMENT_ENTRY_PLACEHOLDER_EMPTY))
            {
                return;
            }
        }
    }
}

static int collect_inventory_page_entries(inventory_menu_group group,
    equipment_list_entry entries[], int capacity,
    const supply_menu_request* request)
{
    int count = 0;
    bool select_mode = inventory_item_select_active(request);

    if (!entries || capacity <= 0)
        return 0;

    for (int i = 0; i < capacity; i++)
        equipment_entry_clear(&entries[i]);

    if (group != INVENTORY_MENU_GROUP_ALL
        || (select_mode && (request->item_select_flags & USE_EQUIP)))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < capacity; i++)
        {
            if (!inventory[i].k_idx)
                continue;
            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_equipped_slot_matches_group(group, i))
                continue;
            if (!inventory_select_item_allowed(request, i))
                continue;

            if (inventory_browser_equipped_slot_matches_group(group, i)
                || group == INVENTORY_MENU_GROUP_ALL)
            {
                equipment_add_entry(entries, &count, capacity, -1, -1, i,
                    true);
            }
        }
    }

    for (int i = 0; i < INVEN_PACK && count < capacity; i++)
    {
        if (!inventory[i].k_idx)
            continue;

        if (select_mode && !(request->item_select_flags & USE_INVEN))
            continue;

        if (group != INVENTORY_MENU_GROUP_ALL
            && !inventory_browser_object_matches_group(group, &inventory[i]))
        {
            continue;
        }

        if (!inventory_select_item_allowed(request, i))
            continue;

        if (equipment_add_entry(entries, &count, capacity, i, -1, -1, false))
        {
            if (!select_mode)
                inventory_browser_add_reserved_limit_rows(group, &inventory[i],
                    entries, &count, capacity);
        }
    }

    /* Items lying on the floor under the player, so they can be compared and
     * picked up from the same list. */
    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py,
            p_ptr->px, 0x00);

        for (int i = 0; i < floor_num && count < capacity; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr;

            if (o_idx <= 0 || o_idx >= o_max)
                continue;

            o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || supplies_is_supply_object(o_ptr))
                continue;

            if (select_mode && !(request->item_select_flags & USE_FLOOR))
                continue;

            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_object_matches_group(group, o_ptr))
            {
                continue;
            }

            if (!inventory_select_item_allowed(request, 0 - o_idx))
                continue;

            equipment_add_floor_entry(entries, &count, capacity, o_idx);
        }
    }

    if (!select_mode)
        inventory_browser_add_empty_limit_rows(group, entries, &count, capacity);

    return count;
}

static bool inventory_replacement_type_matches(const object_type* incoming,
    const object_type* candidate)
{
    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    if (player_oil_container_object(incoming)
        && player_oil_container_object(candidate))
    {
        return true;
    }

    if (incoming->tval == candidate->tval)
        return true;

    {
        int incoming_slot = wield_slot(incoming);

        if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
        {
            int candidate_slot = wield_slot(candidate);

            if (candidate_slot == incoming_slot)
                return true;
        }
    }

    return false;
}

static bool inventory_replacement_object_allowed(
    const supply_menu_request* request, const object_type* o_ptr, bool equipped)
{
    if (!request || !request->replacement_mode)
        return true;

    if (!request->replacement_incoming || !request->replacement_incoming->k_idx)
        return false;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (equipped)
    {
        if (!request->replacement_include_equip)
            return false;
        if (cursed_p(o_ptr))
            return false;
    }

    if (!inven_carry_limit_can_replace(o_ptr))
        return false;

    return inventory_replacement_type_matches(request->replacement_incoming,
        o_ptr);
}

static int count_inventory_replacement_group_entries(
    inventory_menu_group group, const supply_menu_request* request)
{
    int count = 0;

    if (!request || !request->replacement_mode)
        return count_inventory_browser_group_entries(group);

    if (request->replacement_include_equip && group != INVENTORY_MENU_GROUP_ALL)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (inventory_browser_equipped_slot_matches_group(group, i)
                && inventory_replacement_object_allowed(request, &inventory[i],
                    true))
            {
                count++;
            }
        }
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx)
            continue;

        if (group != INVENTORY_MENU_GROUP_ALL
            && !inventory_browser_object_matches_group(group, &inventory[i]))
        {
            continue;
        }

        if (inventory_replacement_object_allowed(request, &inventory[i], false))
            count++;
    }

    if (request->replacement_include_supplies)
    {
        for (int i = 0; i < supplies_entry_count(); i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (!o_ptr || !o_ptr->k_idx)
                continue;

            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_object_matches_group(group, o_ptr))
            {
                continue;
            }

            if (inventory_replacement_object_allowed(request, o_ptr, false))
                count++;
        }
    }

    return count;
}

static void compute_inventory_replacement_group_totals(
    int totals[INVENTORY_BROWSER_GROUP_COUNT],
    const supply_menu_request* request)
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
        totals[i] = count_inventory_replacement_group_entries(
            inventory_browser_groups[i], request);
}

static int collect_inventory_replacement_entries(inventory_menu_group group,
    equipment_list_entry entries[], int capacity,
    const supply_menu_request* request)
{
    int count = 0;

    if (!entries || capacity <= 0 || !request || !request->replacement_mode)
        return 0;

    for (int i = 0; i < capacity; i++)
        equipment_entry_clear(&entries[i]);

    if (request->replacement_include_equip && group != INVENTORY_MENU_GROUP_ALL)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < capacity; i++)
        {
            if (inventory_browser_equipped_slot_matches_group(group, i)
                && inventory_replacement_object_allowed(request, &inventory[i],
                    true))
            {
                equipment_add_entry(entries, &count, capacity, -1, -1, i,
                    true);
            }
        }
    }

    for (int i = 0; i < INVEN_PACK && count < capacity; i++)
    {
        if (!inventory[i].k_idx)
            continue;

        if (group != INVENTORY_MENU_GROUP_ALL
            && !inventory_browser_object_matches_group(group, &inventory[i]))
        {
            continue;
        }

        if (inventory_replacement_object_allowed(request, &inventory[i], false))
            equipment_add_entry(entries, &count, capacity, i, -1, -1, false);
    }

    if (request->replacement_include_supplies)
    {
        for (int i = 0; i < supplies_entry_count() && count < capacity; i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (!o_ptr || !o_ptr->k_idx)
                continue;

            if (group != INVENTORY_MENU_GROUP_ALL
                && !inventory_browser_object_matches_group(group, o_ptr))
            {
                continue;
            }

            if (inventory_replacement_object_allowed(request, o_ptr, false))
            {
                equipment_add_entry(entries, &count, capacity, SUPPLIES_INDEX,
                    i, -1, false);
            }
        }
    }

    return count;
}

static int inventory_replacement_entry_item(const equipment_list_entry* entry)
{
    if (!entry)
        return -1;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
        return entry->equip_idx;

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
        return entry->item_idx;

    if (entry->supply_idx >= 0)
        return SUPPLIES_INDEX + entry->supply_idx;

    return -1;
}

static bool inventory_replacement_overlay_entry(
    const supply_menu_request* request, equipment_list_entry* entry)
{
    const object_type* objects[2];
    const char* headings[2] = { "Picking up", "What to replace" };
    object_type* candidate;

    if (!request || !request->replacement_mode
        || !request->replacement_incoming || !entry)
    {
        return false;
    }

    candidate = equipment_entry_object(entry);
    if (!candidate || !candidate->k_idx)
        return false;

    objects[0] = request->replacement_incoming;
    objects[1] = candidate;

    return object_info_overlay_show_multi(objects, headings, 2);
}

static bool slot_pick_slot_enabled(const supply_menu_request* request, int slot)
{
    if (!request || !request->slot_pick_mode || !request->slot_pick_enabled)
        return false;

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return false;

    return request->slot_pick_enabled[slot];
}

/* Which browser category a candidate destination slot is filed under. */
static inventory_menu_group slot_pick_slot_group(int slot)
{
    if (slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2)
        return INVENTORY_MENU_GROUP_THROWABLES;

    return INVENTORY_MENU_GROUP_WEAPONS;
}

static int count_inventory_slot_pick_group_entries(inventory_menu_group group,
    const supply_menu_request* request)
{
    int count = 0;

    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL; slot++)
    {
        if (!slot_pick_slot_enabled(request, slot))
            continue;

        if (group != INVENTORY_MENU_GROUP_ALL
            && slot_pick_slot_group(slot) != group)
            continue;

        count++;
    }

    return count;
}

static void compute_inventory_slot_pick_group_totals(
    int totals[INVENTORY_BROWSER_GROUP_COUNT],
    const supply_menu_request* request)
{
    for (int i = 0; i < INVENTORY_BROWSER_GROUP_COUNT; i++)
        totals[i] = count_inventory_slot_pick_group_entries(
            inventory_browser_groups[i], request);
}

static int collect_inventory_slot_pick_entries(inventory_menu_group group,
    equipment_list_entry entries[], int capacity,
    const supply_menu_request* request)
{
    int count = 0;

    if (!entries || capacity <= 0 || !request || !request->slot_pick_mode)
        return 0;

    for (int i = 0; i < capacity; i++)
        equipment_entry_clear(&entries[i]);

    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL && count < capacity; slot++)
    {
        if (!slot_pick_slot_enabled(request, slot))
            continue;

        if (group != INVENTORY_MENU_GROUP_ALL
            && slot_pick_slot_group(slot) != group)
            continue;

        equipment_add_slot_entry(entries, &count, capacity, slot);
    }

    return count;
}

static bool inventory_slot_pick_overlay_entry(
    const supply_menu_request* request, equipment_list_entry* entry)
{
    const object_type* objects[2];
    const char* headings[2] = { "Placing", "Currently here" };
    object_type* current;

    if (!request || !request->slot_pick_mode || !request->slot_pick_incoming
        || !entry)
    {
        return false;
    }

    current = equipment_entry_object(entry);
    objects[0] = request->slot_pick_incoming;

    /* Empty destination slot: just describe the item being placed. */
    if (!current || !current->k_idx)
        return object_info_overlay_show_multi(objects, headings, 1);

    objects[1] = current;

    return object_info_overlay_show_multi(objects, headings, 2);
}

static bool inventory_page_use_entry(equipment_list_entry* entry,
    supply_floor_action floor_action)
{
    if (!entry)
        return false;

    if (entry->floor_idx > 0 && entry->floor_idx < o_max)
        return floor_entry_perform_action(entry->floor_idx, floor_action, -1);

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        do_cmd_use_item_by_index(entry->equip_idx);
        return true;
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        do_cmd_use_item_by_index(entry->item_idx);
        return true;
    }

    return false;
}

static bool inventory_page_drop_entry(equipment_list_entry* entry)
{
    if (!entry)
        return false;

    if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
    {
        do_cmd_drop_item_by_index(entry->equip_idx);
        return true;
    }

    if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
    {
        do_cmd_drop_item_by_index(entry->item_idx);
        return true;
    }

    return false;
}

static bool inventory_page_delete_entry(equipment_list_entry* entry)
{
    int item = inventory_select_entry_item(entry);

    if (item == INVENTORY_SELECT_INVALID)
        return false;

    return do_cmd_delete_item_by_index(item);
}

/*
 * Hack -- Create a "forged" artefact
 */
static bool prepare_fake_artefact(object_type* o_ptr, byte name1)
{
    s16b i;

    artefact_type* a_ptr = &a_info[name1];

    /* Ignore "empty" artefacts */
    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    /* Get the "kind" index */
    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    /* Oops */
    if (!i)
        return (false);

    /* Create the artefact */
    object_prep(o_ptr, i);

    /* Save the name */
    o_ptr->name1 = name1;

    /* Extract the fields */
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
    memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /*identify it*/
    object_known(o_ptr);

    /*make it a spoiler item*/
    o_ptr->ident |= IDENT_SPOIL;

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Success */
    return (true);
}

/*
 * Describe fake artefact
 */
void desc_art_fake(int a_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make fake artefact */
    prepare_fake_artefact(i_ptr, a_idx);

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
    log_debug("Player opened artifacts knowledge screen");
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_ARTEFACTS);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] = { "Uniques", /*All uniques, all letters*/
    /*Unused*/ /*'a'*/
    /*Unused*/ /*'A'*/
    "Bats & Birds", /*'b'*/
    /*Unused*/ /*'B'*/
    /*Unused*/ /*'c'*/
    "Canines", /*'C'*/
    "Young Dragons", /*'d'*/
    "Great Dragons", /*'D'*/
    /*Unused*/ /*'e'*/
    /*Unused*/ /*'E'*/
    "Felines", /*'f'*/
    /*Unused*/ /*'F'*/
    /*Unused*/ /*'g'*/
    "Giants", /*'G'*/
    /*Unused*/ /*'h'*/
    "Horrors", /*'H'*/
    /*Unused*/ /*'i'*/
    "Insects", /*'I'*/
    /*Unused*/ /*'j'*/
    /*Unused*/ /*'J'*/
    /*Unused*/ /*'k'*/
    /*Unused*/ /*'K'*/
    /*Unused*/ /*'l'*/
    /*Unused*/ /*'L'*/
    "Young Spiders", /*'m'*/
    "Spiders", /*'M'*/
    /*Unused*/ /*'n'*/
    "Nameless Things", /*'N'*/
    "Orcs", /*'o'*/
    /*Unused*/ /*'O'*/
    /*Unused*/ /*'p'*/
    /*Unused*/ /*'P'*/
    /*Unused*/ /*'q'*/
    /*Unused*/ /*'Q'*/
    /*Unused*/ /*'r'*/
    "Raukar", /*'R'*/
    "Serpents", /*'s'*/
    "Ancient Serpents", /*'S'*/
    /*Unused*/ /*'t'*/
    "Trolls", /*'T'*/
    /*Unused*/ /*'u'*/
    /*Unused*/ /*'U'*/
    "Vampires", /*'v'*/
    "Valar", /*'V'*/
    "Creeping Shadows", /*'w'*/
    "Wights and Wraiths", /*'W'*/
    /*Unused*/ /*'x'*/
    /*Unused*/ /*'X'*/
    /*Unused*/ /*'y'*/
    /*Unused*/ /*'Y'*/
    /*Unused*/ /*'Z'*/
    /*Unused*/ /*'Z'*/
    "Plants", /*'&'*/
    "People", /*'@'*/
    NULL };

/*
 * Symbols of monsters in each group. Note the "Uniques" group
 * is handled differently.
 */
static cptr monster_group_char[] = { (char*)-1L,
    /*"a", Unused*/
    /*"A", Unused*/
    "b",
    /*"B", Unused*/
    /*"c", Unused*/
    "C", "d", "D",
    /*"e", Unused*/
    /*"E", Unused*/
    "f",
    /*"F", Unused*/
    /*"g", Unused*/
    "G",
    /*"h", Unused*/
    "H",
    /*"i", Unused*/
    "I",
    /*"j", Unused*/
    /*"J", Unused*/
    /*"k", Unused*/
    /*"K", Unused*/
    /*"l", Unused*/
    /*"L", Unused*/
    "m", "M",
    /*"n", Unused*/
    "N", "o",
    /*"O", Unused*/
    /*"p", Unused*/
    /*"P", Unused*/
    /*"q", Unused*/
    /*"Q", Unused*/
    /*"r", Unused*/
    "R", "s", "S",
    /*"t", Unused*/
    "T",
    /*"u", Unused*/
    /*"U", Unused*/
    "v", "V", "w", "W",
    /*"x", Unused*/
    /*"X", Unused*/
    /*"y", Unused*/
    /*"Y", Unused*/
    /*"z", Unused*/
    /*"Z", Unused*/
    "&", // plants
    "@", // human/elf/dwarf
    NULL };

/*
 * Build a list of monster indexes in the given group. Return the number
 * of monsters in the group.
 */
static int collect_monsters(int grp_cur, monster_list_entry* mon_idx, int mode)
{
    int i, mon_count = 0;

    /* Get a list of x_char in this group */
    cptr group_char = monster_group_char[grp_cur];

    /* XXX Hack -- Check if this is the "Uniques" group */
    bool grp_unique = (monster_group_char[grp_cur] == (char*)-1L);

    /* Check every race */
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Access the race */
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Is this a unique? */
        bool unique = (r_ptr->flags1 & (RF1_UNIQUE));

        /* Skip empty race */
        if (!r_ptr->name)
            continue;

        if (grp_unique && !(unique))
            continue;

        /* Require known monsters */
        if (!(mode & 0x02) && (!cheat_know) && (!know_monster_info)
            && (!(l_ptr->tsights)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Check for race in the group */
        if ((grp_unique) || (strchr(group_char, r_ptr->d_char)))
        {
            /* Add the race */
            mon_idx[mon_count++].r_idx = i;

            /* XXX Hack -- Just checking for non-empty group */
            if (mode & 0x01)
                break;
        }
    }

    /* Terminate the list */
    mon_idx[mon_count].r_idx = 0;

    /* Return the number of races */
    return (mon_count);
}

#if 0
/*
 * Display the monsters in a group.
 */
static void display_monster_list(int col, int row, int per_page,
    monster_list_entry* mon_idx, int mon_cur, int mon_top, int grp_cur)
{
    int i;

    u32b known_uniques, dead_uniques, slay_count;

    /* Start with 0 kills*/
    known_uniques = dead_uniques = slay_count = 0;

    /* Count up monster kill counts */
    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        // skip monsters that cannot be generated
        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        /* Require non-unique monsters */
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            /*Count if we have seen the unique*/
            if (l_ptr->tsights)
            {
                known_uniques++;

                /*Count if the unique is dead*/
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }

            // increase the uniques count anyway for forewarned or cheaters
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }

        /* Collect "appropriate" monsters */
        else
            slay_count += l_ptr->pkills;
    }

    /* Display lines until done */
    for (i = 0; i < per_page && mon_idx[i].r_idx; i++)
    {
        byte attr;

        /* Get the race index */
        int r_idx = mon_idx[mon_top + i].r_idx;

        /* Access the race */
        monster_race* r_ptr = &r_info[r_idx];
        monster_lore* l_ptr = &l_list[r_idx];

        char race_name[80];

        /* Get the monster race name (singular)*/
        monster_desc_race(race_name, sizeof(race_name), r_idx);

        /* Choose a color */
        attr = ((i + mon_top == mon_cur) ? TERM_L_BLUE : TERM_WHITE);

        /* Display the name */
        c_prt(attr, race_name, row + i, col);

        if (cheat_know)
        {
            c_prt(attr, format("%d", r_idx), row + i, 60);
        }

        /* Display symbol */
        Term_putch(68, row + i, r_ptr->x_attr, r_ptr->x_char);
        if (use_bigtile)
        {
            if ((byte)(r_ptr->x_attr) & 0x80)
                Term_putch(69, row + i, 255, -1);
            else
                Term_putch(69, row + i, 0, ' ');
        }

        /* Display kills */
        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /*use alive/dead for uniques*/
            put_str(format("%s", (r_ptr->max_num == 0) ? " dead" : "alive"),
                row + i, 73);
        }
        else
            put_str(format("%5d", l_ptr->pkills), row + i, 73);
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }

    /*Clear the monster count line*/
    Term_erase(0, 22, 255);

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        c_put_str(TERM_L_BLUE,
            format("Total Creatures Slain: %d. ", slay_count), 22, col + 2);
    }
    else
    {
        c_put_str(TERM_L_BLUE,
            format("Known Uniques: %d, Slain Uniques: %d.", known_uniques,
                dead_uniques),
            22, col + 2);
    }
}
#endif

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_MONSTERS);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type* o_ptr)
{
    s16b old_pval = o_ptr->pval;

    /* Analyze type */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;
    }

    /*many rings need a pval*/
    case TV_RING:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Strength, Dexterity */
        case SV_RING_STR:
        case SV_RING_DEX:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Accuracy */
        case SV_RING_ACCURACY:
        {
            /* Bonus to hit */
            if (o_ptr->att < 1)
                o_ptr->att = 1;

            break;
        }

        /* Ring of Evasion */
        case SV_RING_EVASION:
        {
            /* Bonus to evasion */
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;

            break;
        }

        /* Ring of Secrets */
        case SV_RING_SECRETS:
        {
            /* Bonus to perception */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Ered Luin */
        case SV_RING_ERED_LUIN:
        {
            /* Bonus to will */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Ring of the Laiquendi */
        case SV_RING_LAIQUENDI:
        {
            /* Bonus to stealth and archery */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }
        }

        /*break for TVAL-Rings*/
        break;
    }

    case TV_AMULET:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Various amulets */
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of Protection */
        case SV_AMULET_PROTECTION:
        {
            if (o_ptr->pd < 1)
                o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;
        }

        /* Amulet of the Blessed Realm */
        case SV_AMULET_BLESSED_REALM:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of the Vigilant Eye */
        case SV_AMULET_VIGILANT_EYE:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        default:
            break;
        }
        /*break for TVAL-Amulets*/
        break;
    }

    case TV_LIGHT:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
        {
            o_ptr->timeout = 0;

            break;
        }
        }
        /*break for TVAL-Lights*/
        break;
    }

    /*give them one charge*/
    case TV_STAFF:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        break;
    }
    }

    int pval_delta = (int)o_ptr->pval - (int)old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
}

/*
 * Describe fake object
 */
static void desc_obj_fake(int k_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Create the object */
    object_prep(i_ptr, k_idx);

    /*add minimum bonuses so the descriptions don't look strange*/
    apply_magic_fake(i_ptr);

    /* It's fully known */
    i_ptr->ident |= IDENT_KNOWN;

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    object_info_screen(i_ptr);
}

#if 0
/*
 * Display the objects in a group. (Incorporates some code from jdh)
 */
static void display_object_list(int col, int row, int per_page,
    object_list_entry object_idx[], int object_cur, int object_top)
{
    int i;

    /* Display lines until done */
    for (i = 0; i < per_page && object_idx[i].type != OBJ_NONE; i++)
    {
        char buf[80];

        /* Get the object index */
        int oidx = object_top + i;
        object_list_entry* obj = &object_idx[oidx];
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr, cursor;

        switch (obj->type)
        {
        case OBJ_NORMAL:
            /* Access the object */
            k_ptr = &k_info[obj->idx];

            /* Choose a color */
            attr = ((k_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((k_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            /* Acquire the basic "name" of the object*/
            strip_name(buf, obj->idx);

            /* Display the name */
            c_prt(attr, buf, row + i, col);

            if (cheat_know)
                c_prt(attr, format("%d", obj->idx), row + i, 70);

            if (k_ptr->aware)
            {
                /* Obtain attr/char */
                byte a = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_attr)
                                       : k_ptr->d_attr;
                byte c = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_char)
                                       : k_ptr->d_char;

                /* Display symbol */
                Term_putch(76, row + i, a, c);
            }

            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];

            /* Choose a color */
            attr = ((e_ptr->aware) ? TERM_WHITE : TERM_SLATE);
            cursor = ((e_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
            attr = ((oidx == object_cur) ? cursor : attr);

            if (obj->sval == -1)
            {
                buf[0] = '\0';
                snprintf(buf, sizeof(buf), "  %s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char buf2[80];

                /* Find the specific type */
                buf[0] = '\0';
                buf2[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval)
                        && (k_info[j].sval == obj->sval))
                    {
                        strip_name(buf2, j);
                        break;
                    }
                }

                snprintf(buf, sizeof(buf), "%s %s", buf2, &e_name[e_ptr->name]);
            }

            c_prt(attr, buf, row + i, col);

            break;

        case OBJ_NONE:
        default:
            break;
        }
    }

    /* Clear remaining lines */
    for (; i < per_page; i++)
    {
        Term_erase(col, row + i, 255);
    }
}
#endif

static cptr knowledge_page_name(int page)
{
    switch (page)
    {
    case KNOWLEDGE_PAGE_ARTEFACTS:
        return "Artefacts";
    case KNOWLEDGE_PAGE_OBJECTS:
        return "Objects";
    case KNOWLEDGE_PAGE_MONSTERS:
        return "Monsters";
    case KNOWLEDGE_PAGE_CURSES:
        return "Curses";
    default:
        return "Known";
    }
}

static int knowledge_normalize_page(int page)
{
    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return g_knowledge_last_page;

    return page;
}

static cptr knowledge_tab_label(int page)
{
    static const cptr labels[] = {
        "Arts",
        "Objs",
        "Mons",
        "Curses"
    };

    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return "";

    return labels[page];
}

static void knowledge_init_layout(knowledge_browser_layout* layout,
    int max_group_len, bool has_groups)
{
    int min_group_w = 8;
    int min_list_w = 16;

    Term_get_size(&layout->term_wid, &layout->term_hgt);

    if (layout->term_wid < 1)
        layout->term_wid = 80;
    if (layout->term_hgt < 1)
        layout->term_hgt = 24;

    layout->title_row = 0;
    layout->tabs_row = (layout->term_hgt > 1) ? 1 : 0;
    layout->header_row = (layout->term_hgt > 2) ? 2 : layout->tabs_row;
    layout->divider_row = (layout->term_hgt > 3) ? 3 : layout->header_row;
    layout->list_row = layout->divider_row + 1;
    layout->prompt_row = layout->term_hgt - 1;
    layout->status_row = (layout->prompt_row > layout->list_row)
        ? (layout->prompt_row - 1)
        : layout->prompt_row;
    layout->list_rows = layout->status_row - layout->list_row;
    if (layout->list_rows < 1)
        layout->list_rows = 1;

    if (!has_groups)
    {
        layout->group_col = 0;
        layout->group_w = 0;
        layout->divider_col = -1;
        layout->list_col = 0;
        layout->list_w = layout->term_wid;
        return;
    }

    layout->group_col = 0;
    layout->group_w = max_group_len;
    if (layout->group_w < 10)
        layout->group_w = 10;
    if (layout->group_w > layout->term_wid / 3)
        layout->group_w = layout->term_wid / 3;
    if (layout->group_w < min_group_w)
        layout->group_w = min_group_w;

    while ((layout->group_w > min_group_w)
        && (layout->term_wid - (layout->group_w + 3) < min_list_w))
    {
        layout->group_w--;
    }

    if (layout->term_wid - (layout->group_w + 3) < min_list_w)
    {
        layout->group_w = layout->term_wid - min_list_w - 3;
        if (layout->group_w < min_group_w)
            layout->group_w = min_group_w;
    }

    layout->divider_col = layout->group_w + 1;
    layout->list_col = layout->divider_col + 2;
    layout->list_w = layout->term_wid - layout->list_col;
    if (layout->list_w < 1)
        layout->list_w = 1;
}

static void knowledge_expand_active_column(knowledge_browser_layout* layout)
{
    if (!layout)
        return;

    layout->group_col = 0;
    layout->group_w = layout->term_wid;
    layout->divider_col = -1;
    layout->list_col = 0;
    layout->list_w = layout->term_wid;
}

static void knowledge_draw_tabs(const knowledge_browser_layout* layout, int page,
    bool tabs_focus)
{
    int i;
    int col = 0;

    (void)tabs_focus;
    Term_erase(0, layout->tabs_row, 255);

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        cptr label = knowledge_tab_label(i);
        byte attr = TERM_SLATE;
        int remaining = layout->term_wid - col;
        int len;

        if (remaining <= 0)
            break;

        if (i == page)
            attr = TERM_L_BLUE;

        len = (int)strlen(label);
        Term_putstr(col, layout->tabs_row, remaining, attr, label);
        col += len;

        if ((i < KNOWLEDGE_PAGE_CURSES) && (col < layout->term_wid))
        {
            Term_putstr(col, layout->tabs_row, layout->term_wid - col,
                TERM_SLATE, " ");
            col++;
        }
    }
}

static void knowledge_draw_frame(const knowledge_browser_layout* layout, int page,
    bool has_groups, cptr list_label, bool tabs_focus)
{
    int i;
    char title[64];
    char page_buf[16];

    Term_clear();

    strnfmt(title, sizeof(title), "Known lore - %s", knowledge_page_name(page));
    Term_putstr(0, layout->title_row, layout->term_wid, TERM_L_WHITE + TERM_SHADE,
        title);

    strnfmt(page_buf, sizeof(page_buf), "%d/4", page + 1);
    if ((int)strlen(page_buf) < layout->term_wid)
    {
        int page_col = layout->term_wid - (int)strlen(page_buf);
        Term_putstr(page_col, layout->title_row, layout->term_wid - page_col,
            TERM_SLATE, page_buf);
    }

    knowledge_draw_tabs(layout, page, tabs_focus);

    Term_erase(0, layout->header_row, 255);
    if (has_groups)
    {
        Term_putstr(layout->group_col, layout->header_row, layout->group_w,
            TERM_SLATE, "Group");
        Term_putstr(layout->list_col, layout->header_row, layout->list_w,
            TERM_SLATE, list_label);
    }
    else
    {
        Term_putstr(0, layout->header_row, layout->term_wid, TERM_SLATE,
            list_label);
    }

    for (i = 0; i < layout->term_wid; i++)
    {
        Term_putch(i, layout->divider_row, TERM_L_DARK, '=');
    }

    if (has_groups && layout->divider_col >= 0)
    {
        for (i = 0; i < layout->list_rows; i++)
        {
            Term_putch(layout->divider_col, layout->list_row + i, TERM_L_DARK, '|');
        }
    }

    if (layout->status_row != layout->prompt_row)
        Term_erase(0, layout->status_row, 255);
    Term_erase(0, layout->prompt_row, 255);
}

static void knowledge_register_prompt_clicks(
    const knowledge_browser_layout* layout, cptr prompt);

static void knowledge_draw_prompt(const knowledge_browser_layout* layout)
{
    char prompt[128];

    if (steamdeck_controls_active())
    {
        char prev_label[16];
        char next_label[16];
        char confirm_label[16];
        char back_label[16];
        char prompt_full[128];
        char prompt_mid[96];
        char prompt_short[80];
        const char* variants[3];

        controller_prompt_label(steamdeck_prev_page_key(), "L1", prev_label,
            sizeof(prev_label));
        controller_prompt_label(steamdeck_next_page_key(), "R1", next_label,
            sizeof(next_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_full, sizeof(prompt_full),
            "D-pad move  [%s/%s] page  [%s] recall  [%s] back",
            prev_label, next_label, confirm_label, back_label);
        strnfmt(prompt_mid, sizeof(prompt_mid),
            "[%s/%s] page  [%s] recall  [%s] back",
            prev_label, next_label, confirm_label, back_label);
        strnfmt(prompt_short, sizeof(prompt_short),
            "[%s] recall  [%s] back", confirm_label, back_label);
        variants[0] = prompt_full;
        variants[1] = prompt_mid;
        variants[2] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->term_wid,
            false, variants, N_ELEMENTS(variants));
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_L_DARK, prompt);
        knowledge_register_prompt_clicks(layout, prompt);
    }
    else if (sdl_touch_only_device_active())
    {
        const char* variants[] = {
            "Tap a row to recall",
            "Tap to recall",
            "Tap to recall"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->term_wid,
            false, variants, N_ELEMENTS(variants));
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_SLATE, prompt);
        knowledge_register_prompt_clicks(layout, prompt);
    }
    else
    {
        const char* variants[] = {
            "Dir move  Tab page  Space recall  Esc",
            "Tab page  Space recall  Esc",
            "Space recall  Esc"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->term_wid,
            false, variants, N_ELEMENTS(variants));
        Term_putstr(0, layout->prompt_row, layout->term_wid, TERM_SLATE, prompt);
        knowledge_register_prompt_clicks(layout, prompt);
    }
}

static byte knowledge_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static void knowledge_fill_row(int col, int row, int width, byte attr)
{
    char fill[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

static int knowledge_selection_width(const knowledge_browser_layout* layout,
    int text_col, int text_w, cptr text)
{
    int prefix_w;
    int text_len = text ? (int)strlen(text) : 0;
    int width;

    if (!layout)
        return 1;

    prefix_w = text_col - layout->list_col;
    if (prefix_w < 0)
        prefix_w = 0;
    if (text_w <= 0)
        text_len = 0;
    else if (text_len > text_w)
        text_len = text_w;

    width = prefix_w + text_len;
    if (width < 1)
        width = 1;
    if (width > layout->list_w)
        width = layout->list_w;

    return width;
}

static void knowledge_register_tabs(const knowledge_browser_layout* layout)
{
    int col = 0;

    if (!layout)
        return;

    for (int i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        int len = (int)strlen(knowledge_tab_label(i));

        if (col >= layout->term_wid)
            break;

        ui_menu_click_add(KNOWLEDGE_CLICK_TAB_BASE + i, col,
            layout->tabs_row, len);
        col += len + 1;
    }
}

static void knowledge_register_visible_rows(int click_base,
    const knowledge_browser_layout* layout, int top, int count, int col,
    int width)
{
    if (!layout || width <= 0)
        return;

    for (int i = 0; i < layout->list_rows; i++)
    {
        int idx = top + i;

        if (idx >= count)
            break;

        ui_menu_click_add(click_base + idx, col, layout->list_row + i, width);
    }
}

static void knowledge_register_prompt_clicks(
    const knowledge_browser_layout* layout, cptr prompt)
{
    if (!layout || !prompt)
        return;

    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_RECALL, 0,
        layout->prompt_row, prompt, "recall");
    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_BACK, 0, layout->prompt_row,
        prompt, "back");
    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_BACK, 0, layout->prompt_row,
        prompt, "Esc");
    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_NEXT_PAGE, 0,
        layout->prompt_row, prompt, "Tab");
    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_PREV_PAGE, 0,
        layout->prompt_row, prompt, "L1");
    ui_menu_click_add_text_token(KNOWLEDGE_CLICK_NEXT_PAGE, 0,
        layout->prompt_row, prompt, "R1");
}

static void knowledge_begin_clicks(const knowledge_browser_layout* layout)
{
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    ui_menu_click_set_touch_exit_button(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
    knowledge_begin_touch_scroll_area(layout, SDL_TOUCH_MENU_CATEGORY_OTHER);
    knowledge_register_tabs(layout);
}

static bool knowledge_consume_click(int* ch, int* page,
    knowledge_browser_state* state, int group_cnt, int entry_cnt,
    bool has_groups)
{
    int clicked_choice = 0;
    int click_action = UI_MENU_CLICK_PRIMARY;

    if (!ui_menu_click_take_action(&clicked_choice, &click_action))
        return false;

    ui_menu_click_clear();

    if (clicked_choice >= KNOWLEDGE_CLICK_TAB_BASE
        && clicked_choice < KNOWLEDGE_CLICK_TAB_BASE + 4)
    {
        *page = clicked_choice - KNOWLEDGE_CLICK_TAB_BASE;
        g_knowledge_last_page = *page;
        state->tabs_focus = true;
        return true;
    }

    if (clicked_choice >= KNOWLEDGE_CLICK_GROUP_BASE)
    {
        int clicked_group = clicked_choice - KNOWLEDGE_CLICK_GROUP_BASE;

        if (clicked_group >= 0 && clicked_group < group_cnt)
        {
            state->tabs_focus = false;
            state->column[*page] = 0;
            state->group_cur[*page] = clicked_group;
            state->entry_cur[*page] = 0;
            state->entry_top[*page] = 0;
            return true;
        }
    }

    if (clicked_choice >= KNOWLEDGE_CLICK_ENTRY_BASE)
    {
        int clicked_entry = clicked_choice - KNOWLEDGE_CLICK_ENTRY_BASE;

        if (clicked_entry >= 0 && clicked_entry < entry_cnt)
        {
            state->tabs_focus = false;
            if (has_groups)
                state->column[*page] = 1;

            if (click_action == UI_MENU_CLICK_HOVER
                || state->entry_cur[*page] != clicked_entry)
            {
                state->entry_cur[*page] = clicked_entry;
                return true;
            }

            state->entry_cur[*page] = clicked_entry;
            *ch = 'r';
            return false;
        }
    }

    if (click_action == UI_MENU_CLICK_HOVER)
        return true;

    switch (clicked_choice)
    {
    case KNOWLEDGE_CLICK_BACK: *ch = ESCAPE; return false;
    case KNOWLEDGE_CLICK_RECALL: *ch = 'r'; return false;
    case KNOWLEDGE_CLICK_PREV_PAGE: *ch = '['; return false;
    case KNOWLEDGE_CLICK_NEXT_PAGE: *ch = ']'; return false;
    default: return true;
    }
}

static void knowledge_clamp_group_state(int* column, int* grp_cur, int* grp_top,
    int grp_cnt, int* entry_cur, int* entry_top, int entry_cnt, int per_page)
{
    if (grp_cnt <= 0)
    {
        *column = 0;
        *grp_cur = 0;
        *grp_top = 0;
        *entry_cur = 0;
        *entry_top = 0;
        return;
    }

    if (*grp_cur >= grp_cnt)
        *grp_cur = grp_cnt - 1;
    if (*grp_cur < 0)
        *grp_cur = 0;
    if (*grp_top > *grp_cur)
        *grp_top = *grp_cur;
    if (*grp_cur >= *grp_top + per_page)
        *grp_top = *grp_cur - per_page + 1;
    if (*grp_top < 0)
        *grp_top = 0;

    if (entry_cnt <= 0)
    {
        *column = 0;
        *entry_cur = 0;
        *entry_top = 0;
    }
    else
    {
        if (*entry_cur >= entry_cnt)
            *entry_cur = entry_cnt - 1;
        if (*entry_cur < 0)
            *entry_cur = 0;
        if (*entry_top > *entry_cur)
            *entry_top = *entry_cur;
        if (*entry_cur >= *entry_top + per_page)
            *entry_top = *entry_cur - per_page + 1;
        if (*entry_top < 0)
            *entry_top = 0;
    }

    if (*column < 0)
        *column = 0;
    if (*column > 1)
        *column = 1;
    if (entry_cnt <= 0)
        *column = 0;
}

static void knowledge_clamp_list_state(int* cur, int* top, int count, int per_page)
{
    if (count <= 0)
    {
        *cur = 0;
        *top = 0;
        return;
    }

    if (*cur >= count)
        *cur = count - 1;
    if (*cur < 0)
        *cur = 0;
    if (*top > *cur)
        *top = *cur;
    if (*cur >= *top + per_page)
        *top = *cur - per_page + 1;
    if (*top < 0)
        *top = 0;
}

static void knowledge_display_groups(const knowledge_browser_layout* layout,
    int grp_idx[], cptr group_text[], int grp_cnt, int grp_cur, int grp_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int y = layout->list_row + i;
        int idx = grp_top + i;

        Term_erase(layout->group_col, y, layout->group_w);

        if (idx >= grp_cnt)
            continue;

        if (idx == grp_cur)
        {
            int selection_w = MIN((int)strlen(group_text[grp_idx[idx]]),
                layout->group_w);
            if (selection_w < 1)
                selection_w = 1;
            knowledge_fill_row(layout->group_col, y, selection_w,
                knowledge_selected_attr(TERM_WHITE));
        }

        Term_putstr(layout->group_col, y, layout->group_w,
            (idx == grp_cur) ? knowledge_selected_attr(TERM_WHITE) : TERM_WHITE,
            group_text[grp_idx[idx]]);
    }
}

static int knowledge_entry_icon_w(const knowledge_browser_layout* layout)
{
    (void)layout;
    return use_bigtile ? 3 : 2;
}

static bool knowledge_entry_icons_fit(const knowledge_browser_layout* layout)
{
    if (!layout)
        return false;

    return layout->list_w > knowledge_entry_icon_w(layout) + 3;
}

static int knowledge_entry_name_col(const knowledge_browser_layout* layout)
{
    if (!layout)
        return 0;

    return layout->list_col
        + (knowledge_entry_icons_fit(layout) ? knowledge_entry_icon_w(layout) : 0);
}

static int knowledge_entry_name_width(const knowledge_browser_layout* layout)
{
    int prefix_w;
    int name_w;

    if (!layout)
        return 1;

    prefix_w = knowledge_entry_name_col(layout) - layout->list_col;
    name_w = layout->list_w - prefix_w;

    return (name_w > 0) ? name_w : 1;
}

static bool knowledge_icon_selected_background(int col, int row, byte* bg_attr)
{
    byte attr;
    char chr;

    if (!Term || !Term->scr || !bg_attr)
        return false;
    if (col < 0 || row < 0 || col >= Term->wid || row >= Term->hgt)
        return false;

    attr = Term->scr->a[row][col];
    chr = Term->scr->c[row][col];
    if (attr < TERM_UI_SELECTED || chr != ' ')
        return false;

    *bg_attr = attr;
    return true;
}

static void knowledge_put_entry_icon(const knowledge_browser_layout* layout,
    int row, byte attr, char chr)
{
    int col;
    byte bg_attr = 0;
    bool selected_bg;

    if (!knowledge_entry_icons_fit(layout) || !chr)
        return;

    col = layout->list_col;
    selected_bg = knowledge_icon_selected_background(col, row, &bg_attr);

    if (selected_bg)
        Term_queue_char(col, row, attr, chr, bg_attr, ' ');
    else
        Term_putch(col, row, attr, chr);

    if (use_bigtile)
    {
        if (attr & TILE_FLAG)
        {
            if (selected_bg)
                Term_queue_char(col + 1, row, 255, -1, bg_attr, ' ');
            else
                Term_putch(col + 1, row, 255, -1);
        }
        else
        {
            if (selected_bg)
                Term_queue_char(col + 1, row, bg_attr, ' ', bg_attr, ' ');
            else
                Term_putch(col + 1, row, 0, ' ');
        }
    }
}

static bool knowledge_object_entry_icon(const object_list_entry* obj, byte* attr,
    char* chr)
{
    int k_idx = 0;

    if (!obj || !attr || !chr)
        return false;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        k_idx = obj->idx;
        break;

    case OBJ_SPECIAL:
        if (obj->sval >= 0)
        {
            k_idx = lookup_kind(obj->tval, obj->sval);
        }
        else if (obj->e_idx > 0 && obj->e_idx < z_info->e_max)
        {
            ego_item_type* e_ptr = &e_info[obj->e_idx];
            int i;

            for (i = 0; i < EGO_TVALS_MAX && !k_idx; i++)
            {
                int j;

                if (e_ptr->tval[i] != obj->tval)
                    continue;

                for (j = 1; j < z_info->k_max; j++)
                {
                    object_kind* k_ptr = &k_info[j];

                    if (!k_ptr->name || !k_ptr->everseen)
                        continue;
                    if (k_ptr->tval != obj->tval)
                        continue;
                    if (k_ptr->sval < e_ptr->min_sval[i]
                        || k_ptr->sval > e_ptr->max_sval[i])
                    {
                        continue;
                    }

                    k_idx = j;
                    break;
                }
            }
        }
        break;

    case OBJ_NONE:
    default:
        break;
    }

    if (k_idx <= 0 || k_idx >= z_info->k_max)
        return false;

    *attr = object_type_attr(k_idx);
    *chr = object_type_char(k_idx);

    return (*chr != '\0');
}

static int knowledge_artefact_name_width(const knowledge_browser_layout* layout,
    bool* show_debug)
{
    bool debug = cheat_know && (layout->term_wid >= 78);
    int name_col = knowledge_entry_name_col(layout);
    int name_w = knowledge_entry_name_width(layout);

    if (debug)
    {
        int debug_name_w = (layout->term_wid - 12) - name_col - 1;
        if (debug_name_w >= 12)
            name_w = debug_name_w;
        else
            debug = false;
    }

    if (show_debug)
        *show_debug = debug;

    return (name_w > 0) ? name_w : 1;
}

static void knowledge_object_display_name(char* buf, size_t buflen,
    const object_list_entry* obj)
{
    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (!obj)
        return;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        strip_name(buf, obj->idx);
        break;

    case OBJ_SPECIAL:
    {
        ego_item_type* e_ptr = &e_info[obj->e_idx];

        if (obj->sval == -1)
        {
            strnfmt(buf, buflen, "  %s", &e_name[e_ptr->name]);
        }
        else
        {
            int j;
            char base_name[80];

            base_name[0] = '\0';
            for (j = 0; j < z_info->k_max; ++j)
            {
                if ((k_info[j].tval == obj->tval)
                    && (k_info[j].sval == obj->sval))
                {
                    strip_name(base_name, j);
                    break;
                }
            }

            strnfmt(buf, buflen, "%s %s", base_name, &e_name[e_ptr->name]);
        }
        break;
    }

    case OBJ_NONE:
    default:
        break;
    }
}

static int knowledge_object_name_width(const knowledge_browser_layout* layout,
    bool* show_idx)
{
    bool idx = cheat_know && (layout->term_wid >= 70);
    int name_col = knowledge_entry_name_col(layout);
    int name_w = knowledge_entry_name_width(layout);

    if (idx)
    {
        int idx_name_w = (layout->term_wid - 5) - name_col - 1;
        if (idx_name_w >= 12 && idx_name_w < name_w)
            name_w = idx_name_w;
        else if (idx_name_w < 12)
            idx = false;
    }

    if (show_idx)
        *show_idx = idx;

    return (name_w > 0) ? name_w : 1;
}

static int knowledge_monster_name_width(const knowledge_browser_layout* layout,
    bool* show_kills)
{
    bool kills = (layout->term_wid >= 56);
    int name_col = knowledge_entry_name_col(layout);
    int name_w = knowledge_entry_name_width(layout);

    if (kills)
    {
        int kills_name_w = (layout->term_wid - 5) - name_col - 1;
        if (kills_name_w >= 12 && kills_name_w < name_w)
            name_w = kills_name_w;
        else if (kills_name_w < 12)
            kills = false;
    }

    if (show_kills)
        *show_kills = kills;

    return (name_w > 0) ? name_w : 1;
}

static int knowledge_max_artefact_name_len(int artefact_idx[], int artefact_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < artefact_cnt; i++)
    {
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;
        char o_name[80];
        int len;

        object_wipe(i_ptr);
        prepare_fake_artefact(i_ptr, artefact_idx[i]);
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);
        len = (int)strlen(o_name);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static int knowledge_max_object_name_len(object_list_entry object_idx[],
    int object_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < object_cnt; i++)
    {
        char buf[80];
        int len;

        knowledge_object_display_name(buf, sizeof(buf), &object_idx[i]);
        len = (int)strlen(buf);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static int knowledge_max_monster_name_len(monster_list_entry mon_idx[],
    int mon_cnt)
{
    int max_len = 0;
    int i;

    for (i = 0; i < mon_cnt; i++)
    {
        char race_name[80];
        int len;

        monster_desc_race(race_name, sizeof(race_name), mon_idx[i].r_idx);
        len = (int)strlen(race_name);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static bool knowledge_should_use_single_column_for_names(int split_name_w,
    int full_name_w, int max_name_len)
{
    if (split_name_w < 12)
        return full_name_w > split_name_w;

    return (max_name_len > split_name_w) && (full_name_w > split_name_w);
}

static void knowledge_display_artefacts(const knowledge_browser_layout* layout,
    int artefact_idx[], int artefact_cnt, int artefact_cur, int artefact_top)
{
    bool show_debug = false;
    int idx_col = layout->term_wid - 12;
    int dep_col = layout->term_wid - 8;
    int rar_col = layout->term_wid - 4;
    int name_w = knowledge_artefact_name_width(layout, &show_debug);
    int name_col = knowledge_entry_name_col(layout);
    int i;

    if (show_debug)
    {
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");
        Term_putstr(dep_col, layout->header_row, 3, TERM_SLATE, "Dep");
        Term_putstr(rar_col, layout->header_row, 3, TERM_SLATE, "Rar");
    }

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = artefact_top + i;
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;
        char o_name[80];
        byte attr;

        Term_erase(layout->list_col, row, 255);

        if (idx >= artefact_cnt)
            continue;

        attr = (idx == artefact_cur) ? knowledge_selected_attr(TERM_WHITE)
                                     : TERM_WHITE;
        object_wipe(i_ptr);
        prepare_fake_artefact(i_ptr, artefact_idx[idx]);
        object_desc(o_name, sizeof(o_name), i_ptr, true, 0);
        if (idx == artefact_cur)
        {
            int selection_w = knowledge_selection_width(layout, name_col,
                name_w, o_name);
            knowledge_fill_row(layout->list_col, row, selection_w, attr);
        }
        knowledge_put_entry_icon(layout, row, object_attr(i_ptr),
            object_char(i_ptr));
        Term_putstr(name_col, row, name_w, attr, o_name);

        if (show_debug)
        {
            artefact_type* a_ptr = &a_info[artefact_idx[idx]];
            c_prt(TERM_WHITE, format("%3d", artefact_idx[idx]), row, idx_col);
            c_prt(TERM_WHITE, format("%3d", a_ptr->level), row, dep_col);
            c_prt(TERM_WHITE, format("%3d", a_ptr->rarity), row, rar_col);
        }
    }
}

static void knowledge_display_objects(const knowledge_browser_layout* layout,
    object_list_entry object_idx[], int object_cnt, int object_cur, int object_top)
{
    bool show_idx = false;
    int idx_col = layout->term_wid - 5;
    int name_w = knowledge_object_name_width(layout, &show_idx);
    int name_col = knowledge_entry_name_col(layout);
    int icon_w = knowledge_entry_icons_fit(layout)
        ? knowledge_entry_icon_w(layout)
        : 0;
    int i;

    if (show_idx)
        Term_putstr(idx_col, layout->header_row, 3, TERM_SLATE, "Idx");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int oidx = object_top + i;
        object_list_entry* obj;
        object_kind* k_ptr;
        ego_item_type* e_ptr;
        byte attr;
        byte cursor;
        char buf[80];
        byte icon_attr = TERM_WHITE;
        char icon_chr = '\0';
        bool has_icon;
        int text_col;
        int text_w;

        Term_erase(layout->list_col, row, 255);

        if (oidx >= object_cnt)
            continue;

        obj = &object_idx[oidx];
        has_icon = knowledge_object_entry_icon(obj, &icon_attr, &icon_chr);
        text_col = has_icon ? name_col : layout->list_col;
        text_w = has_icon ? name_w : name_w + icon_w;

        switch (obj->type)
        {
        case OBJ_NORMAL:
            k_ptr = &k_info[obj->idx];
            attr = k_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = k_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;
            knowledge_object_display_name(buf, sizeof(buf), obj);
            if (oidx == object_cur)
            {
                attr = knowledge_selected_attr(attr);
                knowledge_fill_row(layout->list_col, row,
                    knowledge_selection_width(layout, text_col, text_w, buf),
                    attr);
            }
            if (has_icon)
                knowledge_put_entry_icon(layout, row, icon_attr, icon_chr);
            Term_putstr(text_col, row, text_w, attr, buf);

            if (show_idx)
                c_prt(k_ptr->aware ? TERM_WHITE : TERM_SLATE,
                    format("%d", obj->idx), row, idx_col);
            break;

        case OBJ_SPECIAL:
            e_ptr = &e_info[obj->e_idx];
            attr = e_ptr->aware ? TERM_WHITE : TERM_SLATE;
            cursor = e_ptr->aware ? TERM_L_BLUE : TERM_BLUE;
            attr = (oidx == object_cur) ? cursor : attr;
            knowledge_object_display_name(buf, sizeof(buf), obj);
            if (oidx == object_cur)
            {
                attr = knowledge_selected_attr(attr);
                knowledge_fill_row(layout->list_col, row,
                    knowledge_selection_width(layout, text_col, text_w, buf),
                    attr);
            }
            if (has_icon)
                knowledge_put_entry_icon(layout, row, icon_attr, icon_chr);
            Term_putstr(text_col, row, text_w, attr, buf);
            break;

        case OBJ_NONE:
        default:
            break;
        }
    }
}

static void knowledge_monster_summary(char* buf, size_t buflen, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(buf, buflen, "Total creatures slain: %u.", (unsigned)slay_count);
    }
    else
    {
        strnfmt(buf, buflen, "Known uniques: %u, slain uniques: %u.",
            (unsigned)known_uniques, (unsigned)dead_uniques);
    }
}

static void knowledge_display_monsters(const knowledge_browser_layout* layout,
    monster_list_entry mon_idx[], int mon_cnt, int mon_cur, int mon_top)
{
    bool show_kills = false;
    int kills_col = layout->term_wid - 5;
    int name_w = knowledge_monster_name_width(layout, &show_kills);
    int name_col = knowledge_entry_name_col(layout);
    int i;

    if (show_kills)
        Term_putstr(kills_col, layout->header_row, 5, TERM_SLATE, "Kills");

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = mon_top + i;
        int r_idx;
        monster_race* r_ptr;
        monster_lore* l_ptr;
        byte attr;
        char race_name[80];

        Term_erase(layout->list_col, row, 255);

        if (idx >= mon_cnt)
            continue;

        r_idx = mon_idx[idx].r_idx;
        r_ptr = &r_info[r_idx];
        l_ptr = &l_list[r_idx];
        attr = (idx == mon_cur) ? knowledge_selected_attr(TERM_WHITE)
                                : TERM_WHITE;
        monster_desc_race(race_name, sizeof(race_name), r_idx);
        if (idx == mon_cur)
        {
            int selection_w = knowledge_selection_width(layout, name_col,
                name_w, race_name);
            knowledge_fill_row(layout->list_col, row, selection_w, attr);
        }

        knowledge_put_entry_icon(layout, row, r_ptr->x_attr, r_ptr->x_char);
        Term_putstr(name_col, row, name_w, attr, race_name);

        if (show_kills)
        {
            if (r_ptr->flags1 & RF1_UNIQUE)
                put_str((r_ptr->max_num == 0) ? " dead" : "alive", row, kills_col);
            else
                put_str(format("%5d", l_ptr->pkills), row, kills_col);
        }
    }
}

static int knowledge_collect_curses(int curse_idx[])
{
    int id;
    int count = 0;

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (CURSE_SEEN(id))
            curse_idx[count++] = id;
    }

    return count;
}

static cptr knowledge_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0)
        raw += 8;

    return raw;
}

static cptr knowledge_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name)
    {
        cptr raw = cu_name + cu_info[idx].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;

        return raw;
    }

    return knowledge_curse_display_name(idx);
}

static void knowledge_display_curses(const knowledge_browser_layout* layout,
    int curse_idx[], int curse_cnt, int curse_cur, int curse_top)
{
    int i;

    for (i = 0; i < layout->list_rows; i++)
    {
        int row = layout->list_row + i;
        int idx = curse_top + i;
        int id;
        byte attr;

        Term_erase(0, row, 255);

        if (idx >= curse_cnt)
            continue;

        id = curse_idx[idx];
        attr = (idx == curse_cur) ? knowledge_selected_attr(TERM_L_RED)
                                  : TERM_L_RED;
        if (idx == curse_cur)
        {
            cptr sel_name = knowledge_curse_display_name(id);
            int selection_w = MIN(
                utf8_display_width_n(sel_name, (int)strlen(sel_name)),
                layout->term_wid);
            if (selection_w < 1)
                selection_w = 1;
            knowledge_fill_row(0, row, selection_w, attr);
        }
        Term_putstr(0, row, layout->term_wid, attr,
            knowledge_curse_display_name(id));
    }
}

static void knowledge_detail_prompt(int row, bool steamdeck, cptr title,
    cptr accept_label)
{
    Term_erase(0, row, 255);
    if (steamdeck)
    {
        char hint_buf[48];
        strnfmt(hint_buf, sizeof(hint_buf), "(press %s)", accept_label);
        Term_putstr(1, row, -1, TERM_L_WHITE, hint_buf);
    }
    else
    {
        Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
    }

    ui_menu_click_begin();
    ui_menu_click_add_full_row('\r', row);
    (void)inkey();
    ui_menu_click_clear();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, title);
}

static void knowledge_show_curse_detail(int curse_id)
{
    int row = 2;
    int wrap_width = Term->wid - 4;
    int page_limit = Term->hgt - 3;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    curse_type* c = &cu_info[curse_id];
    cptr cname = cu_name + c->name;
    cptr cdesc = cu_text + c->text;
    cptr cpower = cu_text + c->power;
    cptr bname = knowledge_blessing_display_name(curse_id);
    cptr bdesc = (c->blessing_text) ? (cu_text + c->blessing_text) : "";
    cptr bpower = (c->blessing_power) ? (cu_text + c->blessing_power) : "";
    bool has_blessing_text = bdesc && *bdesc;
    bool has_blessing_effect = bpower && *bpower;
    bool has_blessing_info = has_blessing_text || has_blessing_effect
        || (c->blessing_name != 0);
    char effect_line[256];

    if (wrap_width < 20)
        wrap_width = 20;

    if (steamdeck)
    {
        controller_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curse:");

    text_out_hook = text_out_to_screen;
    text_out_wrap = wrap_width;

    c_put_str(TERM_L_RED, cname, row++, 1);

    if (row + count_wrapped_lines(cdesc, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_WHITE, cdesc);
    row += count_wrapped_lines(cdesc, text_out_wrap, 3);

    strnfmt(effect_line, sizeof(effect_line), "Effect: %s",
        (*cpower) ? cpower : "[no additional effect listed]");
    if (row + count_wrapped_lines(effect_line, text_out_wrap, 3) >= page_limit)
        knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);
    Term_gotoxy(3, row);
    text_out_c(TERM_RED, "Effect: ");
    text_out_c(TERM_L_DARK, (*cpower) ? cpower : "[no additional effect listed]");
    row += count_wrapped_lines(effect_line, text_out_wrap, 3);

    row++;

    if (has_blessing_info)
    {
        char blessing_line[256];

        if (row + 1 >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:", accept_label);

        Term_putstr(3, row++, -1, TERM_L_GREEN, format("Blessing: %s", bname));

        if (has_blessing_text)
        {
            if (row + count_wrapped_lines(bdesc, text_out_wrap, 5) >= page_limit)
                knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                    accept_label);
            Term_gotoxy(5, row);
            text_out_c(TERM_WHITE, bdesc);
            row += count_wrapped_lines(bdesc, text_out_wrap, 5);
        }

        strnfmt(blessing_line, sizeof(blessing_line), "Effect: %s",
            has_blessing_effect ? bpower : "[no additional effect listed]");
        if (row + count_wrapped_lines(blessing_line, text_out_wrap, 5) >= page_limit)
            knowledge_detail_prompt(row, steamdeck, "Known Curse:",
                accept_label);
        Term_gotoxy(5, row);
        text_out_c(TERM_L_GREEN, "Effect: ");
        text_out_c(TERM_WHITE,
            has_blessing_effect ? bpower : "[no additional effect listed]");
        row += count_wrapped_lines(blessing_line, text_out_wrap, 5);
    }

    if (row + 1 >= Term->hgt)
        row = Term->hgt - 2;

    knowledge_detail_prompt(row + 1, steamdeck, "Known Curse:", accept_label);
    screen_load();
}

static bool knowledge_handle_page_input(char ch, int* page)
{
    int next_page = *page;

    switch (ch)
    {
    case 'A':
    case 'a':
        next_page = KNOWLEDGE_PAGE_ARTEFACTS;
        break;
    case 'B':
    case 'b':
        next_page = KNOWLEDGE_PAGE_OBJECTS;
        break;
    case 'N':
    case 'n':
        next_page = KNOWLEDGE_PAGE_MONSTERS;
        break;
    case 'U':
    case 'u':
        next_page = KNOWLEDGE_PAGE_CURSES;
        break;
    case '\t':
    case ']':
        next_page = (*page + 1) % 4;
        break;
    case '[':
        next_page = (*page + 3) % 4;
        break;
    default:
        return false;
    }

    *page = next_page;
    g_knowledge_last_page = next_page;
    return true;
}

static bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs)
{
    int d = target_dir(ch);

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d] > 0)
        {
            *page = (*page + 1) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddx[d] < 0)
        {
            *page = (*page + 3) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
        {
            return true;
        }
    }

    return false;
}

static bool knowledge_is_recall_input(int ch)
{
    int confirm_key = steamdeck_confirm_key();

    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == 'R' || ch == 'r'
        || ch == 'X' || ch == 'x' || ch == INPUT_BIND_CONFIRM)
    {
        return true;
    }

    if (confirm_key != GAMEPAD_BIND_NONE && ch == confirm_key)
        return true;

    return false;
}

void do_cmd_knowledge_browser_page(int page)
{
    int i;
    int artefact_grp_idx[100];
    int object_grp_idx[100];
    int monster_grp_idx[100];
    int* artefact_idx = mem_alloc_array(z_info->art_max, int);
    object_list_entry* object_idx =
        mem_alloc_array(z_info->k_max + z_info->e_max + 1, object_list_entry);
    monster_list_entry* mon_idx =
        mem_alloc_array(z_info->r_max, monster_list_entry);
    int* curse_idx = mem_alloc_array(z_info->cu_max, int);
    int artefact_grp_cnt = 0;
    int object_grp_cnt = 0;
    int monster_grp_cnt = 0;
    int artefact_group_w = 0;
    int object_group_w = 0;
    int monster_group_w = 0;
    int curse_cnt = 0;
    int artefact_old = -1;
    int object_old = -1;
    int monster_old = -1;
    knowledge_browser_state state = { 0 };
    bool done = false;

    page = knowledge_normalize_page(page);
    g_knowledge_last_page = page;

    FILE_TYPE(FILE_TYPE_TEXT);

    if (dismiss_active_narrative_banner())
    {
        do_cmd_redraw();
    }

    for (i = 0; object_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(object_group_text[i]);

        if (len > artefact_group_w)
            artefact_group_w = len;
        if (len > object_group_w)
            object_group_w = len;

        if (collect_artefacts(i, artefact_idx))
            artefact_grp_idx[artefact_grp_cnt++] = i;
        if (collect_objects(i, NULL))
            object_grp_idx[object_grp_cnt++] = i;
    }

    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        int len = (int)strlen(monster_group_text[i]);

        if (len > monster_group_w)
            monster_group_w = len;
        if ((monster_group_char[i] == (char*)-1L)
            || collect_monsters(i, mon_idx, 0x01))
        {
            monster_grp_idx[monster_grp_cnt++] = i;
        }
    }

    curse_cnt = knowledge_collect_curses(curse_idx);

    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();
    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    bool saved_hide_cursor = hide_cursor;

    while (!done)
    {
        knowledge_browser_layout layout;
        int ch;

        /* These lists highlight the selection directly, so keep the blinking
         * text cursor hidden.  Re-asserted each iteration in case a recall
         * sub-screen turned it back on. */
        hide_cursor = true;

        switch (page)
        {
        case KNOWLEDGE_PAGE_ARTEFACTS:
        {
            int artefact_cnt = 0;
            int selected_artefact = -1;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[96];
            cptr list_label = "Artefact";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, artefact_group_w, true);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], artefact_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], artefact_cnt, layout.list_rows);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_artefact_name_width(&layout, NULL);
            full_name_w = knowledge_artefact_name_width(&full_layout, NULL);
            max_name_len = knowledge_max_artefact_name_len(artefact_idx,
                artefact_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (artefact_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = object_group_text[
                        artefact_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            knowledge_begin_clicks(&draw_layout);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, artefact_grp_idx,
                    object_group_text, artefact_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_GROUP_BASE,
                    &draw_layout, state.group_top[page], artefact_grp_cnt,
                    single_column ? 0 : draw_layout.group_col,
                    single_column ? draw_layout.term_wid : draw_layout.group_w);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_artefacts(&draw_layout, artefact_idx,
                    artefact_cnt, state.entry_cur[page], state.entry_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_ENTRY_BASE,
                    &draw_layout, state.entry_top[page], artefact_cnt,
                    single_column ? 0 : draw_layout.list_col,
                    draw_layout.term_wid
                        - (single_column ? 0 : draw_layout.list_col));
            }

            if (artefact_cnt > 0)
            {
                selected_artefact = artefact_idx[state.entry_cur[page]];
                strnfmt(status, sizeof(status), "%d artefact%s in %s.",
                    artefact_cnt, (artefact_cnt == 1) ? "" : "s",
                    object_group_text[artefact_grp_idx[state.group_cur[page]]]);
            }
            else
            {
                SDL_strlcpy(status, "No known artefacts yet.", sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (selected_artefact != artefact_old)
            {
                handle_stuff();
                artefact_old = selected_artefact;
            }

            if (artefact_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (knowledge_consume_click(&ch, &page, &state, artefact_grp_cnt,
                artefact_cnt, true))
            {
                break;
            }
            ch = steamdeck_menu_key(ch, '[', ']');

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (artefact_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (artefact_cnt > 0)
                    desc_art_fake(artefact_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], artefact_grp_cnt,
                    &state.entry_cur[page], artefact_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_OBJECTS:
        {
            int object_cnt = 0;
            int tracked_kind = 0;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[112];
            cptr list_label = "Object";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, object_group_w, true);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], object_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], object_cnt, layout.list_rows);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_object_name_width(&layout, NULL);
            full_name_w = knowledge_object_name_width(&full_layout, NULL);
            max_name_len = knowledge_max_object_name_len(object_idx, object_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (object_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = object_group_text[
                        object_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            knowledge_begin_clicks(&draw_layout);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, object_grp_idx,
                    object_group_text, object_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_GROUP_BASE,
                    &draw_layout, state.group_top[page], object_grp_cnt,
                    single_column ? 0 : draw_layout.group_col,
                    single_column ? draw_layout.term_wid : draw_layout.group_w);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_objects(&draw_layout, object_idx, object_cnt,
                    state.entry_cur[page], state.entry_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_ENTRY_BASE,
                    &draw_layout, state.entry_top[page], object_cnt,
                    single_column ? 0 : draw_layout.list_col,
                    draw_layout.term_wid
                        - (single_column ? 0 : draw_layout.list_col));
            }

            if ((object_cnt > 0)
                && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL))
            {
                tracked_kind = object_idx[state.entry_cur[page]].idx;
            }

            if (object_cnt > 0)
            {
                object_list_entry* obj = &object_idx[state.entry_cur[page]];
                if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
                {
                    strnfmt(status, sizeof(status), "%d object%s in %s. Recall available.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
                else
                {
                    strnfmt(status, sizeof(status),
                        "%d object%s in %s. Recall works for identified base items.",
                        object_cnt, (object_cnt == 1) ? "" : "s",
                        object_group_text[object_grp_idx[state.group_cur[page]]]);
                }
            }
            else
            {
                SDL_strlcpy(status, "No known objects yet.", sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (tracked_kind != object_old)
            {
                object_kind_track(tracked_kind);
                handle_stuff();
                object_old = tracked_kind;
            }

            if (object_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (knowledge_consume_click(&ch, &page, &state, object_grp_cnt,
                object_cnt, true))
            {
                break;
            }
            ch = steamdeck_menu_key(ch, '[', ']');

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (object_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if ((object_cnt > 0)
                    && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL)
                    && k_info[object_idx[state.entry_cur[page]].idx].aware)
                {
                    desc_obj_fake(object_idx[state.entry_cur[page]].idx);
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], object_grp_cnt,
                    &state.entry_cur[page], object_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_MONSTERS:
        {
            int monster_cnt = 0;
            int selected_r_idx = 0;
            bool single_column;
            knowledge_browser_layout draw_layout;
            knowledge_browser_layout full_layout;
            char status[96];
            cptr list_label = "Monster";
            int split_name_w;
            int full_name_w;
            int max_name_len;

            knowledge_init_layout(&layout, monster_group_w, true);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], monster_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], monster_cnt, layout.list_rows);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            full_layout = layout;
            knowledge_expand_active_column(&full_layout);
            split_name_w = knowledge_monster_name_width(&layout, NULL);
            full_name_w = knowledge_monster_name_width(&full_layout, NULL);
            max_name_len = knowledge_max_monster_name_len(mon_idx, monster_cnt);
            single_column = knowledge_should_use_single_column_for_names(
                split_name_w, full_name_w, max_name_len);
            draw_layout = layout;
            if (single_column)
            {
                knowledge_expand_active_column(&draw_layout);
                if ((state.column[page] == 0) || (monster_grp_cnt <= 0))
                    list_label = "Group";
                else
                    list_label = monster_group_text[
                        monster_grp_idx[state.group_cur[page]]];
            }

            knowledge_draw_frame(&draw_layout, page, !single_column, list_label,
                state.tabs_focus);
            knowledge_begin_clicks(&draw_layout);
            if (!single_column || (state.column[page] == 0))
            {
                knowledge_display_groups(&draw_layout, monster_grp_idx,
                    monster_group_text, monster_grp_cnt, state.group_cur[page],
                    state.group_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_GROUP_BASE,
                    &draw_layout, state.group_top[page], monster_grp_cnt,
                    single_column ? 0 : draw_layout.group_col,
                    single_column ? draw_layout.term_wid : draw_layout.group_w);
            }
            if (!single_column || (state.column[page] == 1))
            {
                knowledge_display_monsters(&draw_layout, mon_idx, monster_cnt,
                    state.entry_cur[page], state.entry_top[page]);
                knowledge_register_visible_rows(KNOWLEDGE_CLICK_ENTRY_BASE,
                    &draw_layout, state.entry_top[page], monster_cnt,
                    single_column ? 0 : draw_layout.list_col,
                    draw_layout.term_wid
                        - (single_column ? 0 : draw_layout.list_col));
            }

            if (monster_cnt > 0)
            {
                selected_r_idx = mon_idx[state.entry_cur[page]].r_idx;
                knowledge_monster_summary(status, sizeof(status),
                    monster_grp_idx[state.group_cur[page]]);
            }
            else
            {
                SDL_strlcpy(status, "No known monsters in this group yet.",
                    sizeof(status));
            }
            if (draw_layout.status_row != draw_layout.prompt_row)
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE, status);
            knowledge_draw_prompt(&draw_layout);

            if (selected_r_idx != monster_old)
            {
                monster_race_track(selected_r_idx);
                handle_stuff();
                monster_old = selected_r_idx;
            }

            if (monster_grp_cnt > 0)
            {
                if (state.column[page] == 0)
                    Term_gotoxy(draw_layout.group_col, draw_layout.list_row
                        + (state.group_cur[page] - state.group_top[page]));
                else
                    Term_gotoxy(draw_layout.list_col, draw_layout.list_row
                        + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (knowledge_consume_click(&ch, &page, &state, monster_grp_cnt,
                monster_cnt, true))
            {
                break;
            }
            ch = steamdeck_menu_key(ch, '[', ']');

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (monster_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (monster_cnt > 0)
                {
                    if (!screen_roff(mon_idx[state.entry_cur[page]].r_idx,
                            NULL))
                    {
                        ui_menu_click_begin();
                        for (int click_row = 0; click_row < Term->hgt;
                             click_row++)
                        {
                            ui_menu_click_add_full_row('\r', click_row);
                        }
                        (void)inkey();
                        ui_menu_click_clear();
                    }
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], monster_grp_cnt,
                    &state.entry_cur[page], monster_cnt, layout.list_rows,
                    false);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_CURSES:
        default:
        {
            char status[256];

            knowledge_init_layout(&layout, 0, false);
            knowledge_clamp_list_state(&state.entry_cur[page], &state.entry_top[page],
                curse_cnt, layout.list_rows);
            knowledge_draw_frame(&layout, page, false, "Known curses",
                state.tabs_focus);
            knowledge_begin_clicks(&layout);
            knowledge_display_curses(&layout, curse_idx, curse_cnt,
                state.entry_cur[page], state.entry_top[page]);
            knowledge_register_visible_rows(KNOWLEDGE_CLICK_ENTRY_BASE,
                &layout, state.entry_top[page], curse_cnt, 0, layout.term_wid);

            if (curse_cnt > 0)
            {
                curse_type* c = &cu_info[curse_idx[state.entry_cur[page]]];
                cptr cpower = cu_text + c->power;
                strnfmt(status, sizeof(status), "Effect: %s",
                    (*cpower) ? cpower : "[no additional effect listed]");
            }
            else
            {
                SDL_strlcpy(status, "No known curses yet.", sizeof(status));
            }
            if (layout.status_row != layout.prompt_row)
                Term_putstr(0, layout.status_row, layout.term_wid, TERM_L_BLUE, status);
            knowledge_draw_prompt(&layout);

            if (curse_cnt > 0)
            {
                Term_gotoxy(0, layout.list_row
                    + (state.entry_cur[page] - state.entry_top[page]));
            }

            ch = inkey();
            if (knowledge_consume_click(&ch, &page, &state, 0, curse_cnt,
                false))
            {
                break;
            }
            ch = steamdeck_menu_key(ch, '[', ']');

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus, (curse_cnt <= 0) || (state.entry_cur[page] == 0)))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (curse_cnt > 0)
                    knowledge_show_curse_detail(curse_idx[state.entry_cur[page]]);
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
            {
                int d = target_dir(ch);
                int page_jump = (layout.list_rows > 0) ? layout.list_rows : 1;

                if (curse_cnt <= 0)
                {
                    state.entry_cur[page] = 0;
                    break;
                }

                if (!d)
                    break;

                if (ddx[d] && ddy[d])
                    state.entry_cur[page] += ddy[d] * page_jump;
                else if (ddy[d])
                    state.entry_cur[page] += ddy[d];

                if (state.entry_cur[page] < 0)
                    state.entry_cur[page] = 0;
                if (state.entry_cur[page] >= curse_cnt)
                    state.entry_cur[page] = curse_cnt - 1;
                break;
            }
            }
            break;
        }
        }
    }

    hide_cursor = saved_hide_cursor;

    mem_free_null(curse_idx);
    mem_free_null(mon_idx);
    mem_free_null(object_idx);
    mem_free_null(artefact_idx);

    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

/*
 * Display known objects
 */
static void supply_register_prompt_clicks(const knowledge_browser_layout* layout,
    cptr prompt, cptr recall_label, cptr use_label, cptr confirm_label,
    cptr drop_label, cptr back_label)
{
    int row;

    if (!layout || !prompt)
        return;

    row = layout->prompt_row;

    ui_menu_click_add_text_token(SUPPLY_CLICK_RECALL, 0, row, prompt, "recall");
    ui_menu_click_add_text_token(SUPPLY_CLICK_RECALL, 0, row, prompt, "r/->");
    ui_menu_click_add_text_token(SUPPLY_CLICK_RECALL, 0, row, prompt,
        recall_label);
    ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0, row, prompt,
        "x preview");
    ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0, row, prompt,
        "preview");
    ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0, row, prompt,
        "x/->");
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt, "use");
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt, "equip");
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt, "take off");
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt, "u/Space");
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt, use_label);
    ui_menu_click_add_text_token(SUPPLY_CLICK_USE, 0, row, prompt,
        confirm_label);
    ui_menu_click_add_text_token(SUPPLY_CLICK_DROP, 0, row, prompt, "drop");
    ui_menu_click_add_text_token(SUPPLY_CLICK_DROP, 0, row, prompt, "z drop");
    ui_menu_click_add_text_token(SUPPLY_CLICK_DROP, 0, row, prompt, drop_label);
    ui_menu_click_add_text_token(SUPPLY_CLICK_DELETE, 0, row, prompt, "delete");
    ui_menu_click_add_text_token(SUPPLY_CLICK_DELETE, 0, row, prompt,
        "y delete");
    ui_menu_click_add_text_token(SUPPLY_CLICK_TAB, 0, row, prompt, "Tab");
    ui_menu_click_add_text_token(SUPPLY_CLICK_BACK, 0, row, prompt, "back");
    ui_menu_click_add_text_token(SUPPLY_CLICK_BACK, 0, row, prompt, "cancel");
    ui_menu_click_add_text_token(SUPPLY_CLICK_BACK, 0, row, prompt, "Esc");
    ui_menu_click_add_text_token(SUPPLY_CLICK_BACK, 0, row, prompt, back_label);
}

static void supply_highlight_prompt_token(const knowledge_browser_layout* layout,
    cptr prompt, cptr token, byte attr)
{
    cptr match;

    if (!layout || !prompt || !token || !token[0])
        return;

    match = strstr(prompt, token);
    if (!match)
        return;

    Term_putstr((int)(match - prompt), layout->prompt_row,
        (int)strlen(token), attr, token);
}

/*
 * Tracks what the description overlay ('x' preview) is currently showing so we
 * only rebuild it when the selection, page, or terminal size actually changes.
 * The overlay is drawn on top of whichever page is active, so the toggle (and
 * its rendered contents) survive switching between the Equipped, Inventory, and
 * Supplies pages.
 */
typedef struct supply_overlay_cache
{
    bool active;
    int page;
    int entry;
    int group;
    int term_wid;
    int term_hgt;
} supply_overlay_cache;

static void supply_overlay_cache_reset(supply_overlay_cache* cache)
{
    object_info_overlay_clear();
    cache->active = false;
    cache->page = -1;
    cache->entry = -1;
    cache->group = -1;
    cache->term_wid = -1;
    cache->term_hgt = -1;
}

static bool supply_overlay_cache_stale(const supply_overlay_cache* cache,
    int page, int entry, int group, int term_wid, int term_hgt)
{
    return !cache->active || cache->page != page || cache->entry != entry
        || cache->group != group || cache->term_wid != term_wid
        || cache->term_hgt != term_hgt;
}

static void supply_overlay_cache_set(supply_overlay_cache* cache, int page,
    int entry, int group, int term_wid, int term_hgt)
{
    cache->active = true;
    cache->page = page;
    cache->entry = entry;
    cache->group = group;
    cache->term_wid = term_wid;
    cache->term_hgt = term_hgt;
}

static void supply_overlay_avoid_selection(
    const knowledge_browser_layout* layout, bool entry_column, int group_cur,
    int group_top, int entry_cur, int entry_top, int entry_row_stride)
{
    int row;
    int rows = 1;
    int list_end;

    if (!layout || layout->term_wid <= 0 || layout->list_rows <= 0)
    {
        sdl_description_overlay_clear_avoid();
        return;
    }

    if (entry_column)
    {
        if (entry_row_stride < 1)
            entry_row_stride = 1;
        row = layout->list_row + (entry_cur - entry_top) * entry_row_stride;
        rows = entry_row_stride;
    }
    else
    {
        row = layout->list_row + (group_cur - group_top);
    }

    list_end = layout->list_row + layout->list_rows;
    if (row < layout->list_row || row >= list_end)
    {
        sdl_description_overlay_clear_avoid();
        return;
    }
    if (row + rows > list_end)
        rows = list_end - row;
    if (rows < 1)
        rows = 1;

    sdl_description_overlay_set_avoid_term_rect(0, row, layout->term_wid,
        rows);
}

static bool supply_overlay_handle_scroll_key(char ch)
{
    if (ch == '8')
        return sdl_description_overlay_scroll_by(-1);
    if (ch == '2')
        return sdl_description_overlay_scroll_by(1);
    if (ch == '9')
        return sdl_description_overlay_scroll_page(-1);
    if (ch == '3')
        return sdl_description_overlay_scroll_page(1);

    return false;
}

bool do_cmd_knowledge_supplies(const supply_menu_request* request)
{
    int i;
    int max = 0;
    int equip_max = equipment_slot_group_width();
    int grp_cnt = SUPPLY_GROUP_MAX;
    int grp_idx[SUPPLY_GROUP_MAX + 1];
    int group_totals[SUPPLY_GROUP_MAX];
    int group_icon_kinds[SUPPLY_GROUP_MAX];
    supply_group_icon group_icons[SUPPLY_GROUP_MAX];
    supply_list_entry* entries;
    equipment_list_entry* equip_entries;
    int entry_capacity;
    int equip_capacity = z_info->k_max + INVEN_TOTAL;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int equip_grp_cur = 0;
    int equip_grp_top = 0;
    int equip_entry_cur = 0;
    int equip_entry_top = 0;
    int equip_column = 0;
    int inv_grp_cur = 0;
    int inv_grp_top = 0;
    int inv_entry_cur = 0;
    int inv_entry_top = 0;
    int inv_column = 0;
    supply_menu_page page = SUPPLY_MENU_PAGE_SUPPLIES;
    int column = 0;
    bool flag = false;
    bool redraw = true;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool acted = false;
    bool refresh_after_close = false;
    bool drop_click_mode = false;
    bool desc_overlay_on = false;
    bool replacement_mode = false;
    bool slot_pick_mode = false;
    bool item_select_mode = false;
    int focus_floor_o_idx = -1;
    supply_floor_action floor_action = SUPPLY_FLOOR_ACTION_DEFAULT;
    supply_overlay_cache overlay_cache = { false, -1, -1, -1, -1, -1 };
    bool prev_single_column = false;
    int prev_group = -1;
    int prev_column = -1;
    int prev_term_wid = -1;
    int prev_term_hgt = -1;
    int prev_divider_col = -2;

    if (request)
    {
        forced_action = request->action;
        if (request->focus_page
            && request->page >= SUPPLY_MENU_PAGE_EQUIPPED
            && request->page <= SUPPLY_MENU_PAGE_SUPPLIES)
        {
            page = request->page;
        }
        if (request->focus_group || forced_action != SUPPLY_MENU_ACTION_NONE)
            page = SUPPLY_MENU_PAGE_SUPPLIES;
        if (request->focus_group && request->group >= 0 && request->group < SUPPLY_GROUP_MAX)
            grp_cur = request->group;
        if (request->focus_inventory_group
            && request->inventory_group >= INVENTORY_MENU_GROUP_ALL
            && request->inventory_group < INVENTORY_MENU_GROUP_MAX)
        {
            page = SUPPLY_MENU_PAGE_INVENTORY;
            inv_grp_cur = inventory_browser_group_index(
                request->inventory_group);
        }
        if (request->preview_inventory_description)
        {
            page = SUPPLY_MENU_PAGE_INVENTORY;
            inv_column = 1;
            desc_overlay_on = true;
            if (!request->focus_inventory_group)
                inv_grp_cur = inventory_browser_group_index(
                    INVENTORY_MENU_GROUP_ALL);
        }
        if (request->focus_floor_item && request->floor_o_idx > 0
            && request->floor_o_idx < o_max)
        {
            focus_floor_o_idx = request->floor_o_idx;
        }
        if (request->floor_action >= SUPPLY_FLOOR_ACTION_DEFAULT
            && request->floor_action <= SUPPLY_FLOOR_ACTION_WIELD)
        {
            floor_action = request->floor_action;
        }
        if (forced_action != SUPPLY_MENU_ACTION_NONE)
            column = 1;
        if (forced_action == SUPPLY_MENU_ACTION_DROP)
            drop_click_mode = true;
        if (request->replacement_mode && request->replacement_incoming
            && request->replacement_incoming->k_idx
            && request->replacement_item_out)
        {
            replacement_mode = true;
            page = SUPPLY_MENU_PAGE_INVENTORY;
            inv_column = 1;
            desc_overlay_on = true;
            forced_action = SUPPLY_MENU_ACTION_NONE;
            drop_click_mode = false;
            *request->replacement_item_out = -1;
        }
        if (request->slot_pick_mode && request->slot_pick_incoming
            && request->slot_pick_incoming->k_idx
            && request->slot_pick_enabled && request->slot_pick_item_out)
        {
            slot_pick_mode = true;
            page = SUPPLY_MENU_PAGE_INVENTORY;
            inv_column = 1;
            desc_overlay_on = true;
            forced_action = SUPPLY_MENU_ACTION_NONE;
            drop_click_mode = false;
            *request->slot_pick_item_out = -1;
        }
        if (request->item_select_mode && request->item_select_item_out)
        {
            item_select_mode = true;
            page = SUPPLY_MENU_PAGE_INVENTORY;
            inv_column = 1;
            desc_overlay_on = true;
            forced_action = SUPPLY_MENU_ACTION_NONE;
            drop_click_mode = false;
            *request->item_select_item_out = -1;
        }
    }

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
    {
        int len = strlen(supply_group_text[i]) + 5;
        if (len > max)
            max = len;
        grp_idx[i] = i;
    }
    grp_idx[grp_cnt] = -1;
    max += 2;
    int inv_max = inventory_browser_group_width();

    choose_supply_group_icon_kinds(group_icon_kinds);

    entry_capacity = MAX(z_info->k_max, supplies_entry_count());
    if (entry_capacity < 1)
        entry_capacity = 1;

    entries = mem_alloc_array(entry_capacity, supply_list_entry);
    if (request && request->replacement_include_supplies)
        equip_capacity += supplies_entry_count();
    equip_entries = mem_alloc_array(equip_capacity, equipment_list_entry);

    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_description_overlay_main_anchor();
    sdl_push_terminal_menu_scale();

    while (!flag)
    {
        if (page == SUPPLY_MENU_PAGE_EQUIPPED)
        {
            int equip_entry_cnt;
            int equip_totals[EQUIPMENT_MENU_SLOT_COUNT];
            supply_group_icon equip_icons[EQUIPMENT_MENU_SLOT_COUNT];
            knowledge_browser_layout layout;
            int selected_slot;
            char status_buf[120];
            int entry_page_rows;

            prepare_equipment_group_icons(equip_icons);
            compute_equipment_group_totals(equip_totals);
            knowledge_init_layout(&layout, equip_max, true);
            entry_page_rows = layout.list_rows;
            if (entry_page_rows < 1)
                entry_page_rows = 1;

            if (equip_grp_cur >= EQUIPMENT_MENU_SLOT_COUNT)
                equip_grp_cur = EQUIPMENT_MENU_SLOT_COUNT - 1;
            if (equip_grp_cur < 0)
                equip_grp_cur = 0;

            selected_slot = equipment_menu_slots[equip_grp_cur];
            equip_entry_cnt = collect_equipment_entries_for_slot(selected_slot,
                equip_entries, equip_capacity);

            if (equip_entry_cnt == 0)
            {
                equip_entry_cur = 0;
                equip_entry_top = 0;
                if (equip_column)
                    equip_column = 0;
            }
            else
            {
                if (equip_entry_cur >= equip_entry_cnt)
                    equip_entry_cur = equip_entry_cnt - 1;
                if (equip_entry_cur < 0)
                    equip_entry_cur = 0;
                if (equip_entry_cur < equip_entry_top)
                    equip_entry_top = equip_entry_cur;
                if (equip_entry_cur >= equip_entry_top + entry_page_rows)
                    equip_entry_top = equip_entry_cur - entry_page_rows + 1;
                if (equip_entry_top < 0)
                    equip_entry_top = 0;
            }

            if (equip_grp_cur < equip_grp_top)
                equip_grp_top = equip_grp_cur;
            if (equip_grp_cur >= equip_grp_top + layout.list_rows)
                equip_grp_top = equip_grp_cur - layout.list_rows + 1;
            if (equip_grp_top < 0)
                equip_grp_top = 0;

            (void)Term_set_extra_cursor(false, 0, 0, false);
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            ui_menu_click_set_outside_cancel_enabled(true);
            ui_menu_click_set_touch_exit_button(true);
            ui_menu_click_set_touch_category(
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
            knowledge_begin_touch_scroll_area(&layout,
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);

            int source_w = equipment_entry_source_column_width(&layout,
                equip_entries, equip_entry_cnt, 0, equip_entry_cnt, true);

            Term_clear();
            supply_draw_page_header(&layout, page,
                supply_browser_hover_page(), "Equipped");
            Term_putstr(0, layout.tabs_row, layout.term_wid, TERM_SLATE,
                "Equipped items, slots, and matching pack/supply choices");
            Term_erase(0, layout.header_row, 255);
            Term_putstr(0, layout.header_row, layout.group_w, TERM_SLATE,
                "Slot");
            Term_putstr(layout.list_col, layout.header_row, layout.list_w,
                TERM_SLATE, equipment_slot_text(selected_slot));
            if (source_w > 0)
                Term_putstr(layout.term_wid - source_w, layout.header_row,
                    source_w,
                    TERM_SLATE, "Where");

            for (i = 0; i < layout.term_wid; i++)
                Term_putch(i, layout.divider_row, TERM_L_DARK, '=');
            for (i = 0; i < layout.list_rows; i++)
                Term_putch(layout.divider_col, layout.list_row + i,
                    TERM_L_DARK, '|');

            supply_register_page_tabs(&layout);

            {
                int group_selection_w = layout.group_w;

                if (layout.divider_col > layout.group_col)
                    group_selection_w = layout.divider_col - layout.group_col;

                display_equipment_group_list(layout.group_col, layout.list_row,
                    layout.group_w, group_selection_w, layout.list_rows,
                    equip_grp_cur, equip_grp_top, equip_totals, equip_icons,
                    equip_column == 0);
            }

            for (i = 0; i < layout.list_rows; i++)
            {
                int grp_pos = equip_grp_top + i;
                if (grp_pos >= EQUIPMENT_MENU_SLOT_COUNT)
                    break;

                ui_menu_click_add(SUPPLY_CLICK_GROUP_BASE + grp_pos,
                    layout.group_col, layout.list_row + i,
                    (layout.divider_col > layout.group_col)
                        ? (layout.divider_col - layout.group_col)
                        : layout.group_w);
            }

            display_equipment_slot_entries(&layout, layout.list_row,
                layout.list_rows, equip_entries, equip_entry_cnt,
                equip_entry_cur, equip_entry_top, equip_column, true);

            for (i = 0; i < entry_page_rows; i++)
            {
                int entry_pos = equip_entry_top + i;
                if (entry_pos >= equip_entry_cnt)
                    break;

                ui_menu_click_add(SUPPLY_CLICK_ENTRY_BASE + entry_pos,
                    layout.list_col, layout.list_row + i, layout.list_w);
            }

            strnfmt(status_buf, sizeof(status_buf), "%s: %d choice%s.",
                equipment_slot_text(selected_slot), equip_entry_cnt,
                (equip_entry_cnt == 1) ? "" : "s");
            if (layout.status_row != layout.prompt_row)
            {
                Term_erase(0, layout.status_row, 255);
                Term_putstr(0, layout.status_row, layout.term_wid,
                    TERM_L_BLUE, status_buf);
            }

            Term_erase(0, layout.prompt_row, 255);
            if (steamdeck_controls_active())
            {
                char prev_label[16];
                char next_label[16];
                char use_label[16];
                char confirm_label[16];
                char drop_label[16];
                char back_label[16];
                char prompt_buf[160];
                char prompt_full[160];
                char prompt_mid[128];
                char prompt_short[96];
                const char* variants[3];

                controller_prompt_label(steamdeck_prev_page_key(), "L1",
                    prev_label, sizeof(prev_label));
                controller_prompt_label(steamdeck_next_page_key(), "R1",
                    next_label, sizeof(next_label));
                controller_prompt_label(steamdeck_alt_action_key(), "X",
                    use_label, sizeof(use_label));
                controller_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                controller_prompt_label('f', "B", drop_label,
                    sizeof(drop_label));
                controller_prompt_label(ESCAPE, "Start", back_label,
                    sizeof(back_label));
                strnfmt(prompt_full, sizeof(prompt_full),
                    "D-pad move  [%s/%s] page  [%s/%s] equip  [%s] drop  [%s] back",
                    prev_label, next_label, use_label, confirm_label,
                    drop_label, back_label);
                strnfmt(prompt_mid, sizeof(prompt_mid),
                    "[%s/%s] page  [%s/%s] equip  [%s] drop",
                    prev_label, next_label, use_label, confirm_label,
                    drop_label);
                strnfmt(prompt_short, sizeof(prompt_short),
                    "[%s] equip  [%s] drop", confirm_label, drop_label);
                variants[0] = prompt_full;
                variants[1] = prompt_mid;
                variants[2] = prompt_short;
                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, N_ELEMENTS(variants));
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_L_DARK, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL,
                    use_label, confirm_label, drop_label, back_label);
            }
            else if (sdl_touch_only_device_active())
            {
                char prompt_buf[160];
                /* Keep "preview"/"drop" present so supply_register_prompt_
                 * clicks() still finds those tappable tokens on touch. */
                const char* variants[] = {
                    "Tap a row to equip, tap preview or drop",
                    "Tap row: equip, preview, drop",
                    "Tap to equip"
                };

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, N_ELEMENTS(variants));
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_SLATE, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL, NULL,
                    NULL, NULL, NULL);
            }
            else
            {
                char prompt_buf[160];
                const char* const* variants;
                size_t variant_count;
                static const char* letter_variants[] = {
                    "letter use  Dir move  x preview  u equip  z drop  Tab  Esc",
                    "letter use  x preview  z drop  Tab  Esc",
                    "letter use  z drop  Tab  Esc"
                };
                static const char* move_variants[] = {
                    "Dir move  x preview  u equip  z drop  Tab  Esc",
                    "x preview  u equip  z drop  Tab  Esc",
                    "u equip  z drop  Esc"
                };

                if (indexed_menu_letters_enabled())
                {
                    variants = letter_variants;
                    variant_count = N_ELEMENTS(letter_variants);
                }
                else
                {
                    variants = move_variants;
                    variant_count = N_ELEMENTS(move_variants);
                }

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, variant_count);
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_SLATE, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL, NULL,
                    NULL, NULL, NULL);
                if (drop_click_mode)
                    supply_highlight_prompt_token(&layout, prompt_buf, "drop",
                        TERM_YELLOW);
            }

            if (desc_overlay_on && equip_entry_cnt)
            {
                supply_overlay_avoid_selection(&layout, equip_column != 0,
                    equip_grp_cur, equip_grp_top, equip_entry_cur,
                    equip_entry_top, 1);

                if (supply_overlay_cache_stale(&overlay_cache, page,
                        equip_entry_cur, equip_grp_cur, layout.term_wid,
                        layout.term_hgt))
                {
                    if (equipment_menu_overlay_entry(
                            &equip_entries[equip_entry_cur], selected_slot))
                        supply_overlay_cache_set(&overlay_cache, page,
                            equip_entry_cur, equip_grp_cur, layout.term_wid,
                            layout.term_hgt);
                    else
                        supply_overlay_cache_reset(&overlay_cache);
                }
            }
            else if (overlay_cache.active)
            {
                supply_overlay_cache_reset(&overlay_cache);
            }

            if (!equip_column)
                Term_gotoxy(layout.group_col,
                    layout.list_row + (equip_grp_cur - equip_grp_top));
            else if (equip_entry_cnt)
                Term_gotoxy(layout.list_col,
                    layout.list_row + (equip_entry_cur - equip_entry_top));
            else
                Term_gotoxy(layout.group_col,
                    layout.list_row + (equip_grp_cur - equip_grp_top));

            char ch = inkey();
            bool click_generated_command = false;
            {
                int clicked_choice = -1;
                int click_action = UI_MENU_CLICK_PRIMARY;

                if (ui_menu_click_take_action(&clicked_choice, &click_action))
                {
                    if (clicked_choice == SUPPLY_CLICK_PAGE_SUPPLIES)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        page = SUPPLY_MENU_PAGE_SUPPLIES;
                        redraw = true;
                        continue;
                    }
                    else if (clicked_choice == SUPPLY_CLICK_PAGE_INVENTORY)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        page = SUPPLY_MENU_PAGE_INVENTORY;
                        continue;
                    }
                    else if (clicked_choice == SUPPLY_CLICK_PAGE_EQUIPPED)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        continue;
                    }
                    else if (clicked_choice >= SUPPLY_CLICK_ENTRY_BASE)
                    {
                        int clicked_entry = clicked_choice
                            - SUPPLY_CLICK_ENTRY_BASE;

                        if (clicked_entry >= 0
                            && clicked_entry < equip_entry_cnt)
                        {
                            equip_entry_cur = clicked_entry;
                            equip_column = 1;
                            if (click_action == UI_MENU_CLICK_HOVER)
                                continue;
                            ch = (click_action == UI_MENU_CLICK_SECONDARY) ? 'x'
                                : (drop_click_mode ? 'z' : 'u');
                            click_generated_command = true;
                        }
                    }
                    else if (clicked_choice >= SUPPLY_CLICK_GROUP_BASE)
                    {
                        int clicked_group = clicked_choice
                            - SUPPLY_CLICK_GROUP_BASE;

                        if (clicked_group >= 0
                            && clicked_group < EQUIPMENT_MENU_SLOT_COUNT)
                        {
                            equip_grp_cur = clicked_group;
                            equip_entry_cur = 0;
                            equip_entry_top = 0;
                            equip_column = 0;
                            if (click_action == UI_MENU_CLICK_HOVER)
                                continue;
                            continue;
                        }
                    }
                    else
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;

                        switch (clicked_choice)
                        {
                        case SUPPLY_CLICK_BACK:
                            ch = ESCAPE;
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_RECALL:
                            ch = 'x';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_PREVIEW:
                            ch = 'x';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_USE:
                            ch = 'u';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_DROP:
                            drop_click_mode = !drop_click_mode;
                            continue;
                        case SUPPLY_CLICK_TAB:
                            ch = KTRL('I');
                            click_generated_command = true;
                            break;
                        default: break;
                        }
                    }
                }
            }

            if (!click_generated_command)
                ch = (char)steamdeck_menu_key(ch,
                    SUPPLY_BROWSER_PREV_PAGE_KEY,
                    SUPPLY_BROWSER_NEXT_PAGE_KEY);

            if (steamdeck_controls_active() && ch == 'f')
                ch = 'z';

            if (desc_overlay_on && overlay_cache.active
                && supply_overlay_handle_scroll_key(ch))
            {
                continue;
            }

            if (!click_generated_command)
            {
                int hotkey_entry =
                    browser_entry_index_from_label(ch, equip_entry_cnt);
                if (hotkey_entry >= 0)
                {
                    equip_entry_cur = hotkey_entry;
                    equip_column = 1;
                    ch = 'u';
                }
            }

            if ((ch == '\r' || ch == '\n'
                    || (steamdeck_controls_active()
                        && ch == steamdeck_confirm_key()))
                && equip_column && equip_entry_cnt)
            {
                ch = (forced_action == SUPPLY_MENU_ACTION_DROP) ? 'z' : 'u';
            }

            switch (ch)
            {
            case ESCAPE:
                flag = true;
                break;

            case SUPPLY_BROWSER_PREV_PAGE_KEY:
                page = supply_browser_turn_page(page, -1);
                redraw = true;
                break;

            case SUPPLY_BROWSER_NEXT_PAGE_KEY:
                page = supply_browser_turn_page(page, 1);
                redraw = true;
                break;

            case KTRL('I'):
                page = SUPPLY_MENU_PAGE_INVENTORY;
                redraw = true;
                break;

            case 'e':
            case 'E':
                break;

            case 'i':
            case 'I':
                page = SUPPLY_MENU_PAGE_INVENTORY;
                break;

            case 'j':
            case 'J':
                page = SUPPLY_MENU_PAGE_SUPPLIES;
                redraw = true;
                break;

            case 'X':
            case 'x':
            case '6':
#ifdef ARROW_RIGHT
            case ARROW_RIGHT:
#endif
                if (!equip_column && equip_entry_cnt)
                {
                    equip_column = 1;
                    desc_overlay_on = true;
                }
                else if (equip_column && equip_entry_cnt)
                {
                    desc_overlay_on = !desc_overlay_on;
                    if (!desc_overlay_on)
                        supply_overlay_cache_reset(&overlay_cache);
                }
                break;

            case 'u':
            case 'U':
            case ' ':
                if (!equip_column && equip_entry_cnt)
                {
                    equip_column = 1;
                }
                else if (equip_column && equip_entry_cnt)
                {
                    if (death_spectator_active())
                    {
                        msg_print("You can no longer take that action.");
                        break;
                    }

                    if (equipment_menu_use_entry(&equip_entries[equip_entry_cur],
                            selected_slot, floor_action))
                    {
                        acted = true;
                        refresh_after_close = true;
                        flag = true;
                    }
                }
                break;

        case 'z':
        case 'Z':
                if (!equip_column && equip_entry_cnt)
                {
                    equip_column = 1;
                }
                else if (equip_column && equip_entry_cnt)
                {
                    if (death_spectator_active())
                    {
                        msg_print("You can no longer take that action.");
                        break;
                    }

                    if (equipment_menu_drop_entry(&equip_entries[equip_entry_cur]))
                    {
                        acted = true;
                        refresh_after_close = true;
                        flag = true;
                    }
                }
                break;

            default:
                browser_cursor_with_rows(ch, &equip_column, &equip_grp_cur,
                    EQUIPMENT_MENU_SLOT_COUNT, &equip_entry_cur,
                    equip_entry_cnt, entry_page_rows, true);
                break;
            }
            continue;
        }

        if (page == SUPPLY_MENU_PAGE_INVENTORY)
        {
            int inventory_entry_cnt;
            int inventory_choice_cnt;
            int inventory_totals[INVENTORY_BROWSER_GROUP_COUNT];
            supply_group_icon inventory_icons[INVENTORY_BROWSER_GROUP_COUNT];
            knowledge_browser_layout layout;
            inventory_menu_group selected_group;
            int entry_page_rows;
            char status_buf[180];

            prepare_inventory_browser_group_icons(inventory_icons);
            if (replacement_mode)
                compute_inventory_replacement_group_totals(inventory_totals,
                    request);
            else if (slot_pick_mode)
                compute_inventory_slot_pick_group_totals(inventory_totals,
                    request);
            else if (item_select_mode)
                compute_inventory_select_group_totals(inventory_totals,
                    request);
            else
                compute_inventory_browser_group_totals(inventory_totals);
            /* Slot-pick is a flat, single-column destination list (no groups). */
            knowledge_init_layout(&layout, inv_max, !slot_pick_mode);
            entry_page_rows = layout.list_rows;
            if (entry_page_rows < 1)
                entry_page_rows = 1;

            if (inv_grp_cur >= INVENTORY_BROWSER_GROUP_COUNT)
                inv_grp_cur = INVENTORY_BROWSER_GROUP_COUNT - 1;
            if (inv_grp_cur < 0)
                inv_grp_cur = 0;

            if (slot_pick_mode)
            {
                /* One flat list of every candidate slot; keep focus on it. */
                selected_group = INVENTORY_MENU_GROUP_ALL;
                inv_column = 1;
            }
            else
            {
                selected_group = inventory_browser_groups[inv_grp_cur];
            }
            if (replacement_mode)
                inventory_entry_cnt = collect_inventory_replacement_entries(
                    selected_group, equip_entries, equip_capacity, request);
            else if (slot_pick_mode)
                inventory_entry_cnt = collect_inventory_slot_pick_entries(
                    selected_group, equip_entries, equip_capacity, request);
            else
                inventory_entry_cnt = collect_inventory_page_entries(
                    selected_group, equip_entries, equip_capacity, request);
            inventory_choice_cnt = (replacement_mode || slot_pick_mode
                    || item_select_mode)
                ? inventory_entry_cnt
                : count_inventory_browser_group_entries(selected_group);

            if (!replacement_mode && !slot_pick_mode && !item_select_mode
                && focus_floor_o_idx > 0)
            {
                for (i = 0; i < inventory_entry_cnt; i++)
                {
                    if (equip_entries[i].floor_idx != focus_floor_o_idx)
                        continue;

                    inv_entry_cur = i;
                    inv_column = 1;
                    focus_floor_o_idx = -1;
                    break;
                }
            }

            if (inventory_entry_cnt == 0)
            {
                inv_entry_cur = 0;
                inv_entry_top = 0;
                if (inv_column)
                    inv_column = 0;
            }
            else
            {
                if (inv_entry_cur >= inventory_entry_cnt)
                    inv_entry_cur = inventory_entry_cnt - 1;
                if (inv_entry_cur < 0)
                    inv_entry_cur = 0;
                if (inv_entry_cur < inv_entry_top)
                    inv_entry_top = inv_entry_cur;
                if (inv_entry_cur >= inv_entry_top + entry_page_rows)
                    inv_entry_top = inv_entry_cur - entry_page_rows + 1;
                if (inv_entry_top < 0)
                    inv_entry_top = 0;
            }

            if (inv_grp_cur < inv_grp_top)
                inv_grp_top = inv_grp_cur;
            if (inv_grp_cur >= inv_grp_top + layout.list_rows)
                inv_grp_top = inv_grp_cur - layout.list_rows + 1;
            if (inv_grp_top < 0)
                inv_grp_top = 0;

            (void)Term_set_extra_cursor(false, 0, 0, false);
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            ui_menu_click_set_outside_cancel_enabled(true);
            ui_menu_click_set_touch_exit_button(true);
            ui_menu_click_set_touch_category(
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
            knowledge_begin_touch_scroll_area(&layout,
                SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);

            bool show_source =
                slot_pick_mode || selected_group != INVENTORY_MENU_GROUP_ALL;
            int source_w = equipment_entry_source_column_width(&layout,
                equip_entries, inventory_entry_cnt, 0, inventory_entry_cnt,
                show_source);

            Term_clear();
            if (replacement_mode)
            {
                char incoming_name[120];
                char incoming_line[160];
                cptr reason = request->replacement_reason;

                object_desc(incoming_name, sizeof(incoming_name),
                    request->replacement_incoming, true, 3);
                if (reason && reason[0])
                    supply_put_fitted(0, layout.title_row, layout.term_wid,
                        TERM_YELLOW, reason);
                else
                    Term_putstr(0, layout.title_row, layout.term_wid,
                        TERM_L_WHITE + TERM_SHADE, "What to replace");
                strnfmt(incoming_line, sizeof(incoming_line),
                    "Picking up: %s - choose one to replace", incoming_name);
                supply_put_fitted(0, layout.tabs_row, layout.term_wid,
                    TERM_SLATE, incoming_line);
            }
            else if (slot_pick_mode)
            {
                char incoming_name[120];
                char incoming_line[160];
                cptr reason = request->slot_pick_reason;

                object_desc(incoming_name, sizeof(incoming_name),
                    request->slot_pick_incoming, true, 3);
                if (reason && reason[0])
                    supply_put_fitted(0, layout.title_row, layout.term_wid,
                        TERM_YELLOW, reason);
                else
                    Term_putstr(0, layout.title_row, layout.term_wid,
                        TERM_L_WHITE + TERM_SHADE, "Place where");
                strnfmt(incoming_line, sizeof(incoming_line),
                    "Placing: %s - choose a destination", incoming_name);
                supply_put_fitted(0, layout.tabs_row, layout.term_wid,
                    TERM_SLATE, incoming_line);
            }
            else if (item_select_mode)
            {
                cptr reason = request->item_select_reason;

                if (reason && reason[0])
                    supply_put_fitted(0, layout.title_row, layout.term_wid,
                        TERM_YELLOW, reason);
                else
                    Term_putstr(0, layout.title_row, layout.term_wid,
                        TERM_L_WHITE + TERM_SHADE, "Choose item");
                Term_putstr(0, layout.tabs_row, layout.term_wid, TERM_SLATE,
                    "Choose one matching item from inventory, equipment, or floor");
            }
            else
            {
                supply_draw_page_header(&layout, page,
                    supply_browser_hover_page(), "Inventory");
                Term_putstr(0, layout.tabs_row, layout.term_wid, TERM_SLATE,
                    "Inventory categories, equipped matches, and pack limits");
            }
            Term_erase(0, layout.header_row, 255);
            Term_putstr(0, layout.header_row, layout.group_w, TERM_SLATE,
                "Category");
            Term_putstr(layout.list_col, layout.header_row, layout.list_w,
                TERM_SLATE,
                replacement_mode ? "Replacement candidates"
                : slot_pick_mode ? "Destination slots"
                : item_select_mode ? "Matching items"
                : inventory_browser_group_text(selected_group));
            if (source_w > 0)
            {
                Term_putstr(layout.term_wid - source_w, layout.header_row,
                    source_w,
                    TERM_SLATE, "Where");
            }

            for (i = 0; i < layout.term_wid; i++)
                Term_putch(i, layout.divider_row, TERM_L_DARK, '=');
            if (layout.divider_col >= 0)
                for (i = 0; i < layout.list_rows; i++)
                    Term_putch(layout.divider_col, layout.list_row + i,
                        TERM_L_DARK, '|');

            if (!replacement_mode && !slot_pick_mode && !item_select_mode)
                supply_register_page_tabs(&layout);

            if (!slot_pick_mode)
            {
                int group_selection_w = layout.group_w;

                if (layout.divider_col > layout.group_col)
                    group_selection_w = layout.divider_col - layout.group_col;

                display_inventory_browser_group_list(layout.group_col,
                    layout.list_row, layout.group_w, group_selection_w,
                    layout.list_rows, inv_grp_cur, inv_grp_top,
                    inventory_totals, inventory_icons, inv_column == 0);

                for (i = 0; i < layout.list_rows; i++)
                {
                    int grp_pos = inv_grp_top + i;
                    if (grp_pos >= INVENTORY_BROWSER_GROUP_COUNT)
                        break;

                    ui_menu_click_add(SUPPLY_CLICK_GROUP_BASE + grp_pos,
                        layout.group_col, layout.list_row + i,
                        (layout.divider_col > layout.group_col)
                            ? (layout.divider_col - layout.group_col)
                            : layout.group_w);
                }
            }

            display_equipment_slot_entries(&layout, layout.list_row,
                layout.list_rows, equip_entries, inventory_entry_cnt,
                inv_entry_cur, inv_entry_top, inv_column,
                show_source);

            for (i = 0; i < entry_page_rows; i++)
            {
                int entry_pos = inv_entry_top + i;
                if (entry_pos >= inventory_entry_cnt)
                    break;

                ui_menu_click_add(SUPPLY_CLICK_ENTRY_BASE + entry_pos,
                    layout.list_col, layout.list_row + i, layout.list_w);
            }

            if (replacement_mode)
            {
                strnfmt(status_buf, sizeof(status_buf),
                    "%s: %d replacement candidate%s.",
                    inventory_browser_group_text(selected_group),
                    inventory_entry_cnt, (inventory_entry_cnt == 1) ? "" : "s");
            }
            else if (slot_pick_mode)
            {
                strnfmt(status_buf, sizeof(status_buf),
                    "%s: %d destination slot%s.",
                    inventory_browser_group_text(selected_group),
                    inventory_entry_cnt, (inventory_entry_cnt == 1) ? "" : "s");
            }
            else if (item_select_mode)
            {
                strnfmt(status_buf, sizeof(status_buf),
                    "%s: %d matching item%s.",
                    inventory_browser_group_text(selected_group),
                    inventory_entry_cnt, (inventory_entry_cnt == 1) ? "" : "s");
            }
            else
            {
                inventory_browser_group_status(selected_group, inventory_choice_cnt,
                    status_buf, sizeof(status_buf));
            }
            if (layout.status_row != layout.prompt_row)
            {
                Term_erase(0, layout.status_row, 255);
                Term_putstr(0, layout.status_row, layout.term_wid,
                    TERM_L_BLUE, status_buf);
            }

            Term_erase(0, layout.prompt_row, 255);
            if (replacement_mode)
            {
                char prompt[160];

                if (steamdeck_controls_active())
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    controller_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    controller_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    strnfmt(prompt_full, sizeof(prompt_full),
                        "Replace: D-pad move  [%s] select  [%s] cancel",
                        confirm_label, back_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Replace: [%s] select  [%s] cancel", confirm_label,
                        back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else if (sdl_touch_only_device_active())
                {
                    const char* variants[] = {
                        "Replace: tap a row to select",
                        "Tap a row to select",
                        "Tap to select"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else
                {
                    const char* variants[] = {
                        "Replace: Dir/mouse move  Enter select  Esc cancel",
                        "Replace: Enter select  Esc cancel",
                        "Enter select  Esc cancel"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_L_DARK, prompt);
                supply_register_prompt_clicks(&layout, prompt, NULL, "select",
                    NULL, NULL, "Esc");
            }
            else if (slot_pick_mode)
            {
                char prompt[160];

                if (steamdeck_controls_active())
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_full[160];
                    char prompt_short[96];
                    const char* variants[2];

                    controller_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    controller_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    strnfmt(prompt_full, sizeof(prompt_full),
                        "Place: D-pad move  [%s] select  [%s] cancel",
                        confirm_label, back_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "Place: [%s] select  [%s] cancel", confirm_label,
                        back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else if (sdl_touch_only_device_active())
                {
                    const char* variants[] = {
                        "Place: tap a row to select",
                        "Tap a row to select",
                        "Tap to select"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else
                {
                    const char* variants[] = {
                        "Place: Dir/mouse move  Enter select  Esc cancel",
                        "Place: Enter select  Esc cancel",
                        "Enter select  Esc cancel"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_L_DARK, prompt);
                supply_register_prompt_clicks(&layout, prompt, NULL, "select",
                    NULL, NULL, "Esc");
            }
            else if (item_select_mode)
            {
                char prompt[160];

                if (steamdeck_controls_active())
                {
                    char preview_label[16];
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_full[160];
                    char prompt_mid[128];
                    char prompt_short[96];
                    const char* variants[3];

                    controller_prompt_label(steamdeck_info_key(), "RS Right",
                        preview_label, sizeof(preview_label));
                    controller_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    controller_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    strnfmt(prompt_full, sizeof(prompt_full),
                        "Choose: D-pad move  [%s] select  [%s] preview  [%s] cancel",
                        confirm_label, preview_label, back_label);
                    strnfmt(prompt_mid, sizeof(prompt_mid),
                        "Choose: [%s] select  [%s] preview  [%s] cancel",
                        confirm_label, preview_label, back_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "[%s] select  [%s] cancel", confirm_label,
                        back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_mid;
                    variants[2] = prompt_short;
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else if (sdl_touch_only_device_active())
                {
                    const char* variants[] = {
                        "Choose: tap a row to select",
                        "Tap a row to select",
                        "Tap to select"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                else
                {
                    const char* variants[] = {
                        "Choose: Dir/mouse move  Enter select  x preview  Esc cancel",
                        "Enter select  x preview  Esc cancel",
                        "Enter select  Esc cancel"
                    };
                    terminal_prompt_pick_variant(prompt, sizeof(prompt),
                        layout.term_wid, false, variants,
                        N_ELEMENTS(variants));
                }
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_L_DARK, prompt);
                supply_register_prompt_clicks(&layout, prompt, NULL, "select",
                    NULL, NULL, "Esc");
                ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0,
                    layout.prompt_row, prompt, "preview");
                ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0,
                    layout.prompt_row, prompt, "x preview");
            }
            else if (steamdeck_controls_active())
            {
                char prev_label[16];
                char next_label[16];
                char preview_label[16];
                char use_label[16];
                char confirm_label[16];
                char drop_label[16];
                char back_label[16];
                char prompt_buf[160];
                char prompt_full[160];
                char prompt_mid[128];
                char prompt_short[96];
                const char* variants[3];

                controller_prompt_label(steamdeck_prev_page_key(), "L1",
                    prev_label, sizeof(prev_label));
                controller_prompt_label(steamdeck_next_page_key(), "R1",
                    next_label, sizeof(next_label));
                controller_prompt_label(steamdeck_info_key(), "RS Right",
                    preview_label, sizeof(preview_label));
                controller_prompt_label(steamdeck_alt_action_key(), "X",
                    use_label, sizeof(use_label));
                controller_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                controller_prompt_label('f', "B", drop_label,
                    sizeof(drop_label));
                controller_prompt_label(ESCAPE, "Start", back_label,
                    sizeof(back_label));
                strnfmt(prompt_full, sizeof(prompt_full),
                    "D-pad move  [%s/%s] page  [%s] preview  [%s/%s] use  [%s] drop  [%s] back",
                    prev_label, next_label, preview_label, use_label,
                    confirm_label, drop_label, back_label);
                strnfmt(prompt_mid, sizeof(prompt_mid),
                    "[%s/%s] page  [%s/%s] use  [%s] drop",
                    prev_label, next_label, use_label, confirm_label,
                    drop_label);
                strnfmt(prompt_short, sizeof(prompt_short),
                    "[%s/%s] use  [%s] drop", use_label, confirm_label,
                    drop_label);
                variants[0] = prompt_full;
                variants[1] = prompt_mid;
                variants[2] = prompt_short;
                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, N_ELEMENTS(variants));
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_L_DARK, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL,
                    use_label, confirm_label, drop_label, back_label);
                ui_menu_click_add_text_token(SUPPLY_CLICK_PREVIEW, 0,
                    layout.prompt_row, prompt_buf, preview_label);
            }
            else if (sdl_touch_only_device_active())
            {
                char prompt_buf[160];
                /* Keep "preview"/"drop"/"delete" present so supply_register_
                 * prompt_clicks() still finds those tappable tokens on touch. */
                const char* variants[] = {
                    "Tap a row to use, tap preview, drop or delete",
                    "Tap row: use, preview, drop, delete",
                    "Tap to use"
                };

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, N_ELEMENTS(variants));
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_SLATE, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL, NULL,
                    NULL, NULL, NULL);
            }
            else
            {
                char prompt_buf[160];
                const char* const* variants;
                size_t variant_count;
                static const char* letter_variants[] = {
                    "letter use  Dir move  x preview  z drop  y delete  Tab  Esc",
                    "letter use  x preview  z drop  y delete  Tab  Esc",
                    "letter use  z drop  Tab  Esc"
                };
                static const char* move_variants[] = {
                    "Dir move  x preview  u use  z drop  y delete  Tab  Esc",
                    "x preview  u use  z drop  y delete  Tab  Esc",
                    "u use  z drop  Esc"
                };

                if (indexed_menu_letters_enabled())
                {
                    variants = letter_variants;
                    variant_count = N_ELEMENTS(letter_variants);
                }
                else
                {
                    variants = move_variants;
                    variant_count = N_ELEMENTS(move_variants);
                }

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    layout.term_wid, false, variants, variant_count);
                Term_putstr(0, layout.prompt_row, layout.term_wid,
                    TERM_SLATE, prompt_buf);
                supply_register_prompt_clicks(&layout, prompt_buf, NULL, NULL,
                    NULL, NULL, NULL);
                if (drop_click_mode)
                    supply_highlight_prompt_token(&layout, prompt_buf, "drop",
                        TERM_YELLOW);
            }

            if (desc_overlay_on && inventory_entry_cnt)
            {
                int selected_slot = (selected_group == INVENTORY_MENU_GROUP_ALL)
                    ? EQUIPMENT_MENU_ALL
                    : inventory_browser_compare_slot_for_entry(selected_group,
                          &equip_entries[inv_entry_cur]);

                supply_overlay_avoid_selection(&layout, inv_column != 0,
                    inv_grp_cur, inv_grp_top, inv_entry_cur, inv_entry_top,
                    1);

                if (supply_overlay_cache_stale(&overlay_cache, page,
                        inv_entry_cur, (int)selected_group, layout.term_wid,
                        layout.term_hgt))
                {
                    bool shown = replacement_mode
                        ? inventory_replacement_overlay_entry(request,
                            &equip_entries[inv_entry_cur])
                        : slot_pick_mode
                        ? inventory_slot_pick_overlay_entry(request,
                            &equip_entries[inv_entry_cur])
                        : equipment_menu_overlay_entry(
                            &equip_entries[inv_entry_cur], selected_slot);

                    if (shown)
                        supply_overlay_cache_set(&overlay_cache, page,
                            inv_entry_cur, (int)selected_group, layout.term_wid,
                            layout.term_hgt);
                    else
                        supply_overlay_cache_reset(&overlay_cache);
                }
            }
            else if (overlay_cache.active)
            {
                supply_overlay_cache_reset(&overlay_cache);
            }

            if (!inv_column)
                Term_gotoxy(layout.group_col,
                    layout.list_row + (inv_grp_cur - inv_grp_top));
            else if (inventory_entry_cnt)
                Term_gotoxy(layout.list_col,
                    layout.list_row + (inv_entry_cur - inv_entry_top));
            else
                Term_gotoxy(layout.group_col,
                    layout.list_row + (inv_grp_cur - inv_grp_top));

            char ch = inkey();
            bool click_generated_command = false;
            {
                int clicked_choice = -1;
                int click_action = UI_MENU_CLICK_PRIMARY;

                if (ui_menu_click_take_action(&clicked_choice, &click_action))
                {
                    if (clicked_choice == SUPPLY_CLICK_PAGE_EQUIPPED)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        if (item_select_mode)
                            continue;
                        page = SUPPLY_MENU_PAGE_EQUIPPED;
                        continue;
                    }
                    else if (clicked_choice == SUPPLY_CLICK_PAGE_SUPPLIES)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        if (item_select_mode)
                            continue;
                        page = SUPPLY_MENU_PAGE_SUPPLIES;
                        redraw = true;
                        continue;
                    }
                    else if (clicked_choice == SUPPLY_CLICK_PAGE_INVENTORY)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        continue;
                    }
                    else if (clicked_choice >= SUPPLY_CLICK_ENTRY_BASE)
                    {
                        int clicked_entry = clicked_choice
                            - SUPPLY_CLICK_ENTRY_BASE;

                        if (clicked_entry >= 0
                            && clicked_entry < inventory_entry_cnt)
                        {
                            inv_entry_cur = clicked_entry;
                            inv_column = 1;
                            if (click_action == UI_MENU_CLICK_HOVER)
                                continue;
                            ch = (click_action == UI_MENU_CLICK_SECONDARY) ? 'x'
                                : (drop_click_mode ? 'z' : 'u');
                            click_generated_command = true;
                        }
                    }
                    else if (clicked_choice >= SUPPLY_CLICK_GROUP_BASE)
                    {
                        int clicked_group = clicked_choice
                            - SUPPLY_CLICK_GROUP_BASE;

                        if (clicked_group >= 0
                            && clicked_group < INVENTORY_BROWSER_GROUP_COUNT)
                        {
                            inv_grp_cur = clicked_group;
                            inv_entry_cur = 0;
                            inv_entry_top = 0;
                            inv_column = 0;
                            if (click_action == UI_MENU_CLICK_HOVER)
                                continue;
                            continue;
                        }
                    }
                    else
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;

                        switch (clicked_choice)
                        {
                        case SUPPLY_CLICK_BACK:
                            ch = ESCAPE;
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_RECALL:
                            ch = 'x';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_PREVIEW:
                            ch = 'x';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_USE:
                            ch = 'u';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_DROP:
                            drop_click_mode = !drop_click_mode;
                            continue;
                        case SUPPLY_CLICK_DELETE:
                            ch = 'y';
                            click_generated_command = true;
                            break;
                        case SUPPLY_CLICK_TAB:
                            ch = KTRL('I');
                            click_generated_command = true;
                            break;
                        default: break;
                        }
                    }
                }
            }

            if (!click_generated_command)
                ch = (char)steamdeck_menu_key(ch,
                    SUPPLY_BROWSER_PREV_PAGE_KEY,
                    SUPPLY_BROWSER_NEXT_PAGE_KEY);

            if (steamdeck_controls_active() && ch == 'f')
                ch = 'z';

            if (desc_overlay_on && overlay_cache.active
                && supply_overlay_handle_scroll_key(ch))
            {
                continue;
            }

            if (!click_generated_command)
            {
                int hotkey_entry =
                    browser_entry_index_from_label(ch, inventory_entry_cnt);
                if (hotkey_entry >= 0)
                {
                    inv_entry_cur = hotkey_entry;
                    inv_column = 1;
                    ch = 'u';
                }
            }

            if (ch == '-' && !replacement_mode && !slot_pick_mode
                && inventory_entry_cnt)
            {
                bool found_floor = false;

                for (i = 0; i < inventory_entry_cnt; i++)
                {
                    if (equip_entries[i].floor_idx <= 0)
                        continue;

                    inv_entry_cur = i;
                    inv_column = 1;
                    found_floor = true;
                    break;
                }

                if (!found_floor)
                    bell("No floor item in this category.");
                continue;
            }

            if ((ch == '\r' || ch == '\n'
                    || (steamdeck_controls_active()
                        && ch == steamdeck_confirm_key()))
                && inventory_entry_cnt)
            {
                ch = (forced_action == SUPPLY_MENU_ACTION_DROP) ? 'z' : 'u';
            }

            switch (ch)
            {
            case ESCAPE:
                flag = true;
                break;

            case SUPPLY_BROWSER_PREV_PAGE_KEY:
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                page = supply_browser_turn_page(page, -1);
                redraw = true;
                break;

            case SUPPLY_BROWSER_NEXT_PAGE_KEY:
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                page = supply_browser_turn_page(page, 1);
                redraw = true;
                break;

            case KTRL('I'):
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                page = SUPPLY_MENU_PAGE_SUPPLIES;
                redraw = true;
                break;

            case 'e':
            case 'E':
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                page = SUPPLY_MENU_PAGE_EQUIPPED;
                break;

            case 'i':
            case 'I':
                break;

            case 'j':
            case 'J':
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                page = SUPPLY_MENU_PAGE_SUPPLIES;
                redraw = true;
                break;


            case 'X':
            case 'x':
            case '6':
#ifdef ARROW_RIGHT
            case ARROW_RIGHT:
#endif
                if (replacement_mode || slot_pick_mode || item_select_mode)
                {
                    if (inventory_entry_cnt)
                    {
                        inv_column = 1;
                        desc_overlay_on = true;
                    }
                    break;
                }
                if (inventory_entry_cnt)
                {
                    if (!inv_column)
                    {
                        inv_column = 1;
                        desc_overlay_on = true;
                    }
                    else
                    {
                        desc_overlay_on = !desc_overlay_on;
                        if (!desc_overlay_on)
                            supply_overlay_cache_reset(&overlay_cache);
                    }
                }
                break;

            case 'u':
            case 'U':
            case ' ':
                if (replacement_mode)
                {
                    if (!inv_column && inventory_entry_cnt)
                    {
                        inv_column = 1;
                    }
                    else if (inventory_entry_cnt)
                    {
                        int replacement_item =
                            inventory_replacement_entry_item(
                                &equip_entries[inv_entry_cur]);

                        if (replacement_item >= 0
                            && request && request->replacement_item_out)
                        {
                            *request->replacement_item_out = replacement_item;
                            acted = true;
                            flag = true;
                        }
                    }
                    break;
                }
                if (slot_pick_mode)
                {
                    if (!inv_column && inventory_entry_cnt)
                    {
                        inv_column = 1;
                    }
                    else if (inventory_entry_cnt)
                    {
                        int chosen_slot =
                            equip_entries[inv_entry_cur].equip_idx;

                        if (chosen_slot >= INVEN_WIELD
                            && chosen_slot < INVEN_TOTAL
                            && request && request->slot_pick_item_out)
                        {
                            *request->slot_pick_item_out = chosen_slot;
                            acted = true;
                            flag = true;
                        }
                    }
                    break;
                }
                if (item_select_mode)
                {
                    if (!inv_column && inventory_entry_cnt)
                    {
                        inv_column = 1;
                    }
                    else if (inventory_entry_cnt)
                    {
                        int selected_item =
                            inventory_select_entry_item(
                                &equip_entries[inv_entry_cur]);

                        if (selected_item != INVENTORY_SELECT_INVALID
                            && request && request->item_select_item_out
                            && get_item_allow(selected_item))
                        {
                            *request->item_select_item_out = selected_item;
                            acted = true;
                            flag = true;
                        }
                    }
                    break;
                }
                if (!inv_column && inventory_entry_cnt)
                {
                    inv_column = 1;
                }
                else if (inv_column && inventory_entry_cnt)
                {
                    if (death_spectator_active())
                    {
                        msg_print("You can no longer take that action.");
                        break;
                    }

                    if (inventory_page_use_entry(&equip_entries[inv_entry_cur],
                            floor_action))
                    {
                        acted = true;
                        refresh_after_close = true;
                        flag = true;
                    }
                }
                break;

            case 'z':
            case 'Z':
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                if (!inv_column && inventory_entry_cnt)
                {
                    inv_column = 1;
                }
                else if (inv_column && inventory_entry_cnt)
                {
                    if (death_spectator_active())
                    {
                        msg_print("You can no longer take that action.");
                        break;
                    }

                    if (inventory_page_drop_entry(&equip_entries[inv_entry_cur]))
                    {
                        acted = true;
                        refresh_after_close = true;
                        flag = true;
                    }
                }
                break;

            case 'y':
            case 'Y':
                if (replacement_mode || slot_pick_mode || item_select_mode)
                    break;
                if (!inv_column && inventory_entry_cnt)
                {
                    inv_column = 1;
                }
                else if (inv_column && inventory_entry_cnt)
                {
                    if (death_spectator_active())
                    {
                        msg_print("You can no longer take that action.");
                        break;
                    }

                    if (inventory_page_delete_entry(&equip_entries[inv_entry_cur]))
                    {
                        acted = true;
                        refresh_after_close = true;
                        flag = true;
                    }
                }
                break;

            default:
                browser_cursor_with_rows(ch, &inv_column, &inv_grp_cur,
                    INVENTORY_BROWSER_GROUP_COUNT, &inv_entry_cur,
                    inventory_entry_cnt, entry_page_rows, true);
                break;
            }
            continue;
        }

        int entry_cnt;
        knowledge_browser_layout layout;
        knowledge_browser_layout draw_layout;
        knowledge_browser_layout full_layout;
        bool single_column;
        supply_list_columns split_cols;
        supply_list_columns full_cols;
        supply_list_columns draw_cols;
        int used_weight;
        int light_item_weight;
        int light_oil_weight;
        int light_weight;
        int lamp_oil;
        int oil_slots;
        int oil_slot_capacity;
        int max_weight;
        char weight_buf[128];
        char status_buf[96];
        int split_name_w;
        int full_name_w;
        int max_name_len;
        bool compact_width;
        bool compact_draw_names;
        int entry_page_rows;
        int entry_row_stride;
        cptr list_label;
        cptr title_label;

        prepare_supply_group_icons(group_icons, group_icon_kinds);
        compute_supply_group_totals(group_totals);
        knowledge_init_layout(&layout, max, true);
        used_weight = supplies_limit_weight();
        light_item_weight = supplies_carried_light_item_weight();
        light_oil_weight = player_lamp_oil_weight();
        light_weight = light_item_weight + light_oil_weight;
        lamp_oil = player_lamp_oil();
        oil_slots = player_oil_container_slots_used();
        oil_slot_capacity = player_oil_container_slot_capacity();
        max_weight = supplies_current_weight_cap();

        if (grp_cur >= grp_cnt)
            grp_cur = grp_cnt - 1;
        if (grp_cur < 0)
            grp_cur = 0;

        entry_cnt = collect_supply_entries(grp_idx[grp_cur], entries,
            entry_capacity);

        if (focus_floor_o_idx > 0)
        {
            for (i = 0; i < entry_cnt; i++)
            {
                if (entries[i].floor_idx != focus_floor_o_idx)
                    continue;

                entry_cur = i;
                column = 1;
                focus_floor_o_idx = -1;
                break;
            }
        }

        if (entry_cnt == 0)
        {
            entry_cur = 0;
            entry_top = 0;
            if (column)
                column = 0;
        }
        else
        {
            if (entry_cur >= entry_cnt)
                entry_cur = entry_cnt - 1;
            if (entry_cur < 0)
                entry_cur = 0;

            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + layout.list_rows)
                entry_top = entry_cur - layout.list_rows + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + layout.list_rows)
            grp_top = grp_cur - layout.list_rows + 1;
        if (grp_top < 0)
            grp_top = 0;

        full_layout = layout;
        knowledge_expand_active_column(&full_layout);
        supply_init_columns(&layout, grp_idx[grp_cur], &split_cols);
        supply_init_columns(&full_layout, grp_idx[grp_cur], &full_cols);
        split_name_w = split_cols.name_w;
        full_name_w = full_cols.name_w;
        compact_width = supply_use_compact_names_for_width(&layout);
        compact_draw_names = compact_width && op_ptr
            && op_ptr->opt[OPT_supply_menu_hide_flavor_compact];
        max_name_len = supply_max_name_len(grp_idx[grp_cur], entries,
            entry_cnt, compact_draw_names);

        if (split_name_w > 1)
            split_name_w--;

        single_column = knowledge_should_use_single_column_for_names(
            split_name_w, full_name_w, max_name_len);
        draw_layout = single_column ? full_layout : layout;
        draw_cols = single_column ? full_cols : split_cols;
        entry_row_stride = (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            ? jewelry_preset_display_rows(&draw_layout, &draw_cols)
            : 1;
        entry_page_rows = (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            ? jewelry_preset_entries_per_page(&draw_layout, &draw_cols)
            : draw_layout.list_rows;
        if (entry_page_rows < 1)
            entry_page_rows = 1;
        if (entry_cnt > 0)
        {
            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + entry_page_rows)
                entry_top = entry_cur - entry_page_rows + 1;
            if (entry_top < 0)
                entry_top = 0;
        }
        build_supply_weight_summary(weight_buf, sizeof(weight_buf),
            draw_layout.term_wid, used_weight, max_weight, light_weight,
            light_item_weight, light_oil_weight, lamp_oil,
            player_lamp_oil_capacity(), oil_slots, oil_slot_capacity);

        list_label = (single_column && column)
            ? supply_group_text[grp_idx[grp_cur]]
            : "Name";
        title_label = (draw_layout.term_wid <= 50)
            ? "Supplies - H/F/P/G/Oil/Jewelry/Supply"
            : "Supplies - Herbs, Food, Potions, Gems, Lights/Oil, Jewelry Sets, Supply";

        if (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            list_label = "Set";

        if (redraw || single_column
            || (single_column != prev_single_column)
            || (grp_idx[grp_cur] != prev_group)
            || (column != prev_column)
            || (draw_layout.term_wid != prev_term_wid)
            || (draw_layout.term_hgt != prev_term_hgt)
            || (draw_layout.divider_col != prev_divider_col))
        {
            Term_clear();
            Term_putstr(0, draw_layout.tabs_row, draw_layout.term_wid, TERM_SLATE,
                weight_buf);
            Term_erase(0, draw_layout.header_row, 255);

            if (single_column && !column)
            {
                Term_putstr(0, draw_layout.header_row, draw_layout.term_wid,
                    TERM_SLATE, "Group");
            }
            else
            {
                if (!single_column)
                    Term_putstr(0, draw_layout.header_row, draw_layout.group_w,
                        TERM_SLATE, "Group");
                if (draw_cols.show_sym)
                    Term_putstr(draw_cols.sym_hdr_col, draw_layout.header_row,
                        use_bigtile ? 2 : 1, TERM_SLATE, "S");
                Term_putstr(draw_cols.name_col, draw_layout.header_row,
                    draw_cols.name_w, TERM_SLATE, list_label);
                if (draw_cols.show_weight)
                    Term_putstr(draw_cols.weight_col, draw_layout.header_row, 5,
                        TERM_SLATE, "Wt");
                if (draw_cols.show_turns)
                    Term_putstr(draw_cols.turns_col, draw_layout.header_row, 5,
                        TERM_SLATE, "Turns");
                if (draw_cols.show_qty)
                    Term_putstr(draw_cols.qty_col, draw_layout.header_row, 3,
                        TERM_SLATE, "Qty");
            }

            for (i = 0; i < draw_layout.term_wid; i++)
                Term_putch(i, draw_layout.divider_row, TERM_L_DARK, '=');

            if (!single_column)
            {
                for (i = 0; i < draw_layout.list_rows; i++)
                {
                    Term_putch(draw_layout.divider_col, draw_layout.list_row + i,
                        TERM_L_DARK, '|');
                }
            }

            redraw = false;
        }

        prev_single_column = single_column;
        prev_group = grp_idx[grp_cur];
        prev_column = column;
        prev_term_wid = draw_layout.term_wid;
        prev_term_hgt = draw_layout.term_hgt;
        prev_divider_col = draw_layout.divider_col;

        (void)Term_set_extra_cursor(false, 0, 0, false);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_exit_button(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_SUPPLY);
        knowledge_begin_touch_scroll_area(&draw_layout,
            SDL_TOUCH_MENU_CATEGORY_SUPPLY);
        supply_draw_page_header(&draw_layout, page,
            supply_browser_hover_page(), title_label);
        supply_register_page_tabs(&draw_layout);

        if (!single_column || !column)
        {
            int group_list_w = (!single_column) ? draw_layout.group_w
                                                : layout.group_w;
            int group_selection_w = group_list_w;

            if (!single_column && draw_layout.divider_col > draw_layout.group_col)
                group_selection_w = draw_layout.divider_col
                    - draw_layout.group_col;

            display_supply_group_list(draw_layout.group_col, draw_layout.list_row,
                group_list_w, group_selection_w, draw_layout.list_rows,
                grp_idx, grp_cur, grp_top, group_totals, group_icons,
                column == 0);

            for (i = 0; i < draw_layout.list_rows; i++)
            {
                int grp_pos = grp_top + i;
                if (grp_pos >= grp_cnt || grp_idx[grp_pos] < 0)
                    break;

                ui_menu_click_add(SUPPLY_CLICK_GROUP_BASE + grp_pos,
                    draw_layout.group_col, draw_layout.list_row + i,
                    group_selection_w);
            }
        }
        if (!single_column || column)
        {
            display_supply_list(&draw_layout, draw_layout.list_row,
                draw_layout.list_rows, entries, entry_cnt, entry_cur, entry_top,
                grp_idx[grp_cur], column, &draw_cols, compact_draw_names);

            for (i = 0; i < entry_page_rows; i++)
            {
                int entry_pos = entry_top + i;
                int entry_y = draw_layout.list_row + (i * entry_row_stride);
                if (entry_pos >= entry_cnt)
                    break;

                for (int line = 0; line < entry_row_stride
                    && entry_y + line < draw_layout.list_row + draw_layout.list_rows;
                    line++)
                {
                    ui_menu_click_add(SUPPLY_CLICK_ENTRY_BASE + entry_pos,
                        draw_layout.list_col, entry_y + line, draw_layout.list_w);
                }
            }
        }

        if (draw_layout.status_row != draw_layout.prompt_row)
        {
            describe_supply_group_status(grp_idx[grp_cur],
                draw_layout.term_wid, status_buf, sizeof(status_buf));
            Term_erase(0, draw_layout.status_row, 255);
            if (status_buf[0] != '\0')
                Term_putstr(0, draw_layout.status_row, draw_layout.term_wid,
                    TERM_L_BLUE,
                    status_buf);
        }

        /* Bottom bar: grey text with white first letters */
        Term_erase(0, draw_layout.prompt_row, 255);
        if (steamdeck_controls_active()) {
            char prev_label[16];
            char next_label[16];
            char recall_label[16];
            char use_label[16];
            char confirm_label[16];
            char drop_label[16];
            char back_label[16];
            char prompt_buf[160];
            char prompt_full[160];
            char prompt_mid[128];
            char prompt_short[96];
            const char* variants[3];

            /* Steam Deck UI: RS Right=recall, X=use, A=confirm, B=drop, Start=back */
            controller_prompt_label(steamdeck_prev_page_key(), "L1",
                prev_label, sizeof(prev_label));
            controller_prompt_label(steamdeck_next_page_key(), "R1",
                next_label, sizeof(next_label));
            controller_prompt_label(steamdeck_info_key(), "RS Right", recall_label, sizeof(recall_label));
            controller_prompt_label(steamdeck_alt_action_key(), "X", use_label, sizeof(use_label));
            controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            controller_prompt_label('f', "B", drop_label, sizeof(drop_label));
            controller_prompt_label(ESCAPE, "Start", back_label, sizeof(back_label));

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad move  [%s/%s] page  [%s] recall  [%s/%s] use  [%s] drop  [%s] back",
                prev_label, next_label, recall_label, use_label, confirm_label,
                drop_label, back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid),
                "[%s/%s] page  [%s/%s] use  [%s] drop",
                prev_label, next_label, use_label, confirm_label, drop_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s/%s] use  [%s] drop", use_label, confirm_label,
                drop_label);
            variants[0] = prompt_full;
            variants[1] = prompt_mid;
            variants[2] = prompt_short;
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                draw_layout.term_wid, false, variants, N_ELEMENTS(variants));
            Term_putstr(0, draw_layout.prompt_row, draw_layout.term_wid,
                TERM_L_DARK, prompt_buf);
            supply_register_prompt_clicks(&draw_layout, prompt_buf,
                recall_label, use_label, confirm_label, drop_label,
                back_label);
            if (drop_click_mode)
                supply_highlight_prompt_token(&draw_layout, prompt_buf, "drop",
                    TERM_YELLOW);
        } else if (sdl_touch_only_device_active()) {
            char prompt_buf[160];
            const char* variants[] = {
                "Tap a row to use",
                "Tap to use",
                "Tap to use"
            };

            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                draw_layout.term_wid, false, variants, N_ELEMENTS(variants));
            Term_putstr(0, draw_layout.prompt_row, draw_layout.term_wid,
                TERM_SLATE, prompt_buf);
            supply_register_prompt_clicks(&draw_layout, prompt_buf,
                NULL, NULL, NULL, NULL, NULL);
            if (drop_click_mode)
                supply_highlight_prompt_token(&draw_layout, prompt_buf, "drop",
                    TERM_YELLOW);
        } else {
            char prompt_buf[160];
            const char* const* variants;
            size_t variant_count;
            static const char* jewelry_variants[] = {
                "Dir move  x preview  u equip  s save  c clear  Tab  Alt+1-5  Esc",
                "Dir move  u equip  s save  c clear  Tab  Esc",
                "u equip  s save  c clear  Esc"
            };
            static const char* letter_variants[] = {
                "letter use  Dir move  x preview  z drop  Tab  Esc",
                "letter use  x preview  z drop  Tab  Esc",
                "letter use  z drop  Esc"
            };
            static const char* move_variants[] = {
                "Dir move  x preview  u use  z drop  Tab  Esc",
                "x preview  u use  z drop  Tab  Esc",
                "u use  z drop  Esc"
            };

            if (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            {
                variants = jewelry_variants;
                variant_count = N_ELEMENTS(jewelry_variants);
            }
            else
            {
                if (indexed_menu_letters_enabled())
                {
                    variants = letter_variants;
                    variant_count = N_ELEMENTS(letter_variants);
                }
                else
                {
                    variants = move_variants;
                    variant_count = N_ELEMENTS(move_variants);
                }
            }
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                draw_layout.term_wid, false, variants, variant_count);
            Term_putstr(0, draw_layout.prompt_row, draw_layout.term_wid,
                TERM_SLATE, prompt_buf);
            supply_register_prompt_clicks(&draw_layout, prompt_buf,
                NULL, NULL, NULL, NULL, NULL);
            if (drop_click_mode)
                supply_highlight_prompt_token(&draw_layout, prompt_buf, "drop",
                    TERM_YELLOW);
        }

        if (desc_overlay_on && entry_cnt)
        {
            supply_overlay_avoid_selection(&draw_layout, column != 0,
                grp_cur, grp_top, entry_cur, entry_top, entry_row_stride);

            if (supply_overlay_cache_stale(&overlay_cache, page, entry_cur,
                    grp_cur, draw_layout.term_wid, draw_layout.term_hgt))
            {
                if (supplies_menu_overlay_entry(&entries[entry_cur]))
                    supply_overlay_cache_set(&overlay_cache, page, entry_cur,
                        grp_cur, draw_layout.term_wid, draw_layout.term_hgt);
                else
                    supply_overlay_cache_reset(&overlay_cache);
            }
        }
        else if (overlay_cache.active)
        {
            supply_overlay_cache_reset(&overlay_cache);
        }

        if (!column)
            Term_gotoxy(draw_layout.group_col,
                draw_layout.list_row + (grp_cur - grp_top));
        else if (entry_cnt)
            Term_gotoxy(draw_layout.list_col,
                draw_layout.list_row + ((entry_cur - entry_top)
                    * entry_row_stride));
        else
            Term_gotoxy(draw_layout.group_col,
                draw_layout.list_row + (grp_cur - grp_top));

        char ch = inkey();
        bool click_generated_command = false;
        {
            int clicked_choice = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == SUPPLY_CLICK_PAGE_EQUIPPED)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    page = SUPPLY_MENU_PAGE_EQUIPPED;
                    continue;
                }
                else if (clicked_choice == SUPPLY_CLICK_PAGE_INVENTORY)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    page = SUPPLY_MENU_PAGE_INVENTORY;
                    continue;
                }
                else if (clicked_choice == SUPPLY_CLICK_PAGE_SUPPLIES)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    continue;
                }
                else if (clicked_choice >= SUPPLY_CLICK_ENTRY_BASE)
                {
                    int clicked_entry = clicked_choice - SUPPLY_CLICK_ENTRY_BASE;

                    if (clicked_entry >= 0 && clicked_entry < entry_cnt)
                    {
                        entry_cur = clicked_entry;
                        column = 1;
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        ch = (click_action == UI_MENU_CLICK_SECONDARY) ? 'x'
                            : (drop_click_mode ? 'z' : 'u');
                        click_generated_command = true;
                    }
                }
                else if (clicked_choice >= SUPPLY_CLICK_GROUP_BASE)
                {
                    int clicked_group = clicked_choice - SUPPLY_CLICK_GROUP_BASE;

                    if (clicked_group >= 0 && clicked_group < grp_cnt)
                    {
                        grp_cur = clicked_group;
                        entry_cur = 0;
                        entry_top = 0;
                        column = single_column ? 1 : 0;
                        redraw = true;
                        continue;
                    }
                }
                else
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    switch (clicked_choice)
                    {
                    case SUPPLY_CLICK_BACK:
                        ch = ESCAPE;
                        click_generated_command = true;
                        break;
                    case SUPPLY_CLICK_RECALL:
                        ch = 'x';
                        click_generated_command = true;
                        break;
                    case SUPPLY_CLICK_PREVIEW:
                        ch = 'x';
                        click_generated_command = true;
                        break;
                    case SUPPLY_CLICK_USE:
                        ch = 'u';
                        click_generated_command = true;
                        break;
                    case SUPPLY_CLICK_DROP:
                        drop_click_mode = !drop_click_mode;
                        redraw = true;
                        continue;
                    case SUPPLY_CLICK_TAB:
                        ch = KTRL('I');
                        click_generated_command = true;
                        break;
                    default: break;
                    }
                }
            }
        }
        if (!click_generated_command)
            ch = (char)steamdeck_menu_key(ch, SUPPLY_BROWSER_PREV_PAGE_KEY,
                SUPPLY_BROWSER_NEXT_PAGE_KEY);

        if (steamdeck_controls_active() && ch == 'f')
            ch = 'z';

        if (desc_overlay_on && overlay_cache.active
            && supply_overlay_handle_scroll_key(ch))
        {
            continue;
        }

        if (ch == '-' && entry_cnt)
        {
            bool found_floor = false;

            for (i = 0; i < entry_cnt; i++)
            {
                if (entries[i].floor_idx <= 0)
                    continue;

                entry_cur = i;
                column = 1;
                found_floor = true;
                break;
            }

            if (!found_floor)
                bell("No floor item in this supply group.");
            continue;
        }

        if (!click_generated_command)
        {
            int hotkey_entry = browser_entry_index_from_label(ch, entry_cnt);

            if (hotkey_entry >= 0)
            {
                entry_cur = hotkey_entry;
                column = 1;
                ch = 'u';
            }
        }

        if ((ch == '\r' || ch == '\n' || (steamdeck_controls_active() && ch == steamdeck_confirm_key())) && column && entry_cnt)
        {
            if (forced_action == SUPPLY_MENU_ACTION_USE)
                ch = 'u';
            else if (forced_action == SUPPLY_MENU_ACTION_DROP)
                ch = 'z';
        }

        switch (ch)
        {
        case ESCAPE:
            flag = true;
            break;

        case SUPPLY_BROWSER_PREV_PAGE_KEY:
            page = supply_browser_turn_page(page, -1);
            redraw = true;
            break;

        case SUPPLY_BROWSER_NEXT_PAGE_KEY:
            page = supply_browser_turn_page(page, 1);
            redraw = true;
            break;

        case KTRL('I'):
            page = SUPPLY_MENU_PAGE_EQUIPPED;
            break;

        case 'e':
        case 'E':
            page = SUPPLY_MENU_PAGE_EQUIPPED;
            break;

        case 'i':
        case 'I':
            page = SUPPLY_MENU_PAGE_INVENTORY;
            break;

        case 'j':
        case 'J':
            break;

        case 'X':
        case 'x':
        case '6':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            if (!column && entry_cnt)
            {
                column = 1;
                desc_overlay_on = true;
            }
            else if (column && entry_cnt)
            {
                desc_overlay_on = !desc_overlay_on;
                if (!desc_overlay_on)
                    supply_overlay_cache_reset(&overlay_cache);
            }
            break;

        case 'u':
        case 'U':
        case ' ':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool handled = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if ((entry->item_idx == SUPPLIES_INDEX
                        && entry->supply_idx >= 0)
                    || (entry->floor_idx > 0 && entry->floor_idx < o_max))
                {
                    handled = supplies_menu_use_entry(entry, floor_action);
                }
                else if (entry->equip_idx == INVEN_LITE && entry->equipped)
                {
                    msg_print("That light source is already equipped.");
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_type* o_ptr = &inventory[entry->item_idx];

                    switch (o_ptr->tval)
                    {
                    case TV_FOOD:
                        do_cmd_eat_food(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_POTION:
                        do_cmd_quaff_potion(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_STAFF:
                        do_cmd_activate_staff(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_GEM:
                        do_cmd_use_gem(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_FLASK:
                        do_cmd_refuel_lamp(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_LIGHT:
                        do_cmd_wield(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    default:
                        bell("Cannot use that item here!");
                        break;
                    }

                    if (handled)
                        handle_stuff();
                }
                else
                {
                    bell("You do not have any of that item.");
                    msg_print("You do not have any of that item.");
                }

                if (handled)
                {
                    acted = true;
                    redraw = true;
                    refresh_after_close = true;
                    flag = true;
                }
            }
            break;

            case 'z':
            case 'Z':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool dropped = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    dropped = supplies_menu_drop_entry(entry);
                }
                else if (entry->equip_idx >= INVEN_WIELD && entry->equip_idx < INVEN_TOTAL)
                {
                    do_cmd_drop_item_by_index(entry->equip_idx);
                    dropped = true;
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    do_cmd_drop_item_by_index(entry->item_idx);
                    dropped = true;
                }
                else
                {
                    bell("Nothing to drop here.");
                    msg_print("Nothing to drop here.");
                }

                if (dropped)
                {
                    acted = true;
                    redraw = true;
                    handle_stuff();
                    refresh_after_close = true;
                    flag = true;
                }
            }
            break;

        case 's':
        case 'S':
            if (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            {
                if (!column && entry_cnt)
                {
                    column = 1;
                }
                else if (column && entry_cnt)
                {
                    supply_list_entry* entry = &entries[entry_cur];

                    if (entry->preset_idx >= 0
                        && do_cmd_jewelry_preset_store(entry->preset_idx))
                    {
                        redraw = true;
                    }
                }
            }
            else
            {
                    browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt,
                        &entry_cur, entry_cnt, entry_page_rows, true);
            }
            break;

        case 'c':
        case 'C':
            if (grp_idx[grp_cur] == SUPPLY_GROUP_JEWELRY_PRESETS)
            {
                if (!column && entry_cnt)
                {
                    column = 1;
                }
                else if (column && entry_cnt)
                {
                    supply_list_entry* entry = &entries[entry_cur];

                    if (entry->preset_idx >= 0
                        && do_cmd_jewelry_preset_clear(entry->preset_idx))
                    {
                        redraw = true;
                    }
                }
            }
            else
            {
                    browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt,
                        &entry_cur, entry_cnt, entry_page_rows, true);
            }
            break;

        default:
            browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt, &entry_cur,
                entry_cnt, entry_page_rows, true);
            break;
        }
    }

    object_info_overlay_clear();
    mem_free_null(entries);
    mem_free_null(equip_entries);
    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_pop_terminal_menu_scale();
    sdl_pop_description_overlay_main_anchor();
    screen_pop_supporting_panes_hidden();
    screen_load();

    if (refresh_after_close)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        Term_fresh();
    }

    (void)self_knowledge_display_pending();

    return acted;
}

void do_cmd_knowledge_objects(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_OBJECTS);
}

/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
    int n, i;

    SDL_IOStream* fff;

    char file_name[1024];

    u16b* who;
    //	u16b why = 4;

    /* Temporary file */
    fff = sdl_fopen_temp(file_name, sizeof(file_name));

    /* Failure */
    if (!fff)
        return;

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        // monster_race *r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Require non-unique monsters */
        // if (r_ptr->flags1 & RF1_UNIQUE) continue;

        /* Collect "appropriate" monsters */
        if (l_ptr->pkills > 0)
            who[n++] = i;
    }

    /* Select the sort method */
    // ang_sort_comp = ang_sort_comp_hook;
    // ang_sort_swap = ang_sort_swap_hook;

    /* Sort by kills (and level) */
    // ang_sort(who, &why, n);

    /* Print the monsters (highest kill counts first) */
    for (i = n - 1; i >= 0; i--)
    {
        monster_race* r_ptr = &r_info[who[i]];
        monster_lore* l_ptr = &l_list[who[i]];

        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Print a message */
            SDL_IOprintf(fff, "         %-40s\n", (r_name + r_ptr->name));
        }
        else
        {
            /* Print a message */
            SDL_IOprintf(
                fff, "  %5d  %-40s\n", l_ptr->pkills, (r_name + r_ptr->name));
        }
    }

    /* Free the "who" array */
    mem_free_null(who);

    /* Close the file */
    sdl_fclose(fff);

    /* Display the file contents */
    show_file(file_name, "Kill counts", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
    char ch;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Clear any active banner before opening knowledge menu */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Interact until done */
    while (1)
    {
        /* Clear screen */
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        /* Ask for a choice */
        prt("Display current knowledge", 2, 0);

        /* Give some choices */
        prt("(1) Display known lore browser", 4, 5);
        prt("(2) Display supplies overview", 5, 5);
        prt("(3) Display names of the fallen", 6, 5);
        prt("(4) Display kill counts", 7, 5);

        /*allow the player to see the notes taken if that option is selected*/
        c_put_str(TERM_WHITE, "(5) Display character notes file", 8, 5);
        prt("(6) Display oath status", 9, 5);
        for (int i = 1; i <= 6; i++)
            ui_menu_click_add_full_row(i, i + 3);

        /* Prompt */
        prt("Command: ", 11, 0);

        /* Prompt */
        ch = inkey();
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action)
                && clicked_choice >= 1 && clicked_choice <= 6)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                ch = I2D(clicked_choice);
            }
        }

        /* Done */
        if (ch == ESCAPE)
            break;

        ui_menu_click_clear();
        ui_scroll_area_clear();

        /* Known lore browser */
        if (ch == '1')
        {
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
        }

        /* Scores */
        else if (ch == '2')
        {
            do_cmd_knowledge_supplies(NULL);
        }

        /* Scores */
        else if (ch == '3')
        {
            sdl_push_terminal_menu_scale();
            show_scores_interactive(true);
            sdl_pop_terminal_menu_scale();
        }

        /* Kill counts */
        else if (ch == '4')
        {
            do_cmd_knowledge_kills();
        }

        /* Notes file, if one exists */
        else if (ch == '5')
        {
            /* Spawn */
            do_cmd_knowledge_notes();
        }

        /* Oath status */
        else if (ch == '6')
        {
            do_cmd_knowledge_oaths();
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for knowledge!");
        }

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_pop_supporting_panes_hidden();
    screen_load();
}
