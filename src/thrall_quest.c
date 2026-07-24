/*
 * File: thrall_quest.c
 * Purpose: Thrall quest system - alert thralls can request items from the player
 *          and provide selectable rewards
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "mem/alloc.h"
#include "metarun.h"
#include "object/object-ui-select.h"
#include "supplies.h"
#include "thrall_quest.h"
#include "ui/question.h"

static s16b get_upgrade_kind(const object_type* o_ptr);
static byte damaged_ego_index(const object_type* o_ptr, bool* is_prefix);
static bool damaged_ego_is_repairable(byte e_idx);

/*
 * Probability weights for each item type by thrall race
 * Human thralls prefer practical items, elven thralls prefer finer items
 */
static const int human_thrall_weights[THRALL_QUEST_MAX] = {
    0,   /* NONE - not used */
    32,  /* SHOVEL - tools to pry/delve */
    26,  /* LANTERN - light against the dark */
    8,   /* HERB_HEALING - uncommon healing */
    6,   /* MALLORN - rarer, but prized */
    10,  /* POTION_HEALING - valuable healing */
    24,  /* DAGGER - last defence, cut bonds */
    18,  /* CLOAK - warmth and concealment */
    22,  /* BOOTS - escape over stone */
    8,   /* HERB_SUSTENANCE - rare respite from hunger */
    5,   /* HERB_RESTORATION - rare recovery */
    16,  /* POTION_CLARITY - endure fear and glamour */
    24,  /* FLASK_OIL - precious lamp fuel */
    20,  /* WOODEN_TORCH - simple light for escape */
    28,  /* DARK_BREAD - familiar, lasting food */
    24,  /* DRIED_MEAT - strength for escape */
    0    /* LEMBAS - requested only by elven thralls */
};

static const int elf_thrall_weights[THRALL_QUEST_MAX] = {
    0,   /* NONE - not used */
    8,   /* SHOVEL - seldom asked of the Eldar */
    18,  /* LANTERN - practical light */
    10,  /* HERB_HEALING - uncommon healing lore */
    32,  /* MALLORN - light of fair make */
    10,  /* POTION_HEALING - costly draughts */
    18,  /* DAGGER - keen blade, cut bonds */
    26,  /* CLOAK - to move unseen */
    24,  /* BOOTS - to tread in silence */
    3,   /* HERB_SUSTENANCE - rare respite from hunger */
    7,   /* HERB_RESTORATION - rare recovery */
    14,  /* POTION_CLARITY - lift veils from the mind */
    12,  /* FLASK_OIL - clean fuel for a guarded flame */
    14,  /* WOODEN_TORCH - light in the pits */
    0,   /* DARK_BREAD - requested only by human thralls */
    0,   /* DRIED_MEAT - requested only by human thralls */
    24   /* LEMBAS - a remembered taste of home */
};

#define THRALL_ARTEFACT_REVEAL_COUNT 3

enum
{
    THRALL_REWARD_ARTEFACT = 0,
    THRALL_REWARD_REPAIR = 1,
    THRALL_REWARD_SANCTIFY = 2,
    THRALL_REWARD_IDENTIFY_ONE = 3,
    THRALL_REWARD_IDENTIFY_NATURE = 4,
    THRALL_REWARD_LATER = 5
};

typedef struct thrall_reward_option
{
    int reward;
    char hotkey;
    cptr label;
    bool enabled;
} thrall_reward_option;

/*
 * Tolkienistic texts for thrall interactions
 */
static const char* human_request_texts[] = {
    "The shadows here are deep, and hope is a rare guest in these halls. Yet I see a glimmer of light in your eyes, stranger.\n"
    "\n"
    "I am bound to this darkness, but perhaps you are not. If your heart still knows pity, I beg a small boon of you.\n"
    "\n"
    "Bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "Grant me this, and I shall share what little aid I can still offer.",

    "You there - halt. You have the look of a free soul about you, and that is a rare and dangerous thing in these deeps.\n"
    "\n"
    "I have nothing to offer but words, and those are cheap in Angband. But I have a need, and you may have the means.\n"
    "\n"
    "Bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "Do this, and I will repay the kindness as best I can. I have not forgotten all my skill.",

    "Do not be afraid. I was a man once, before the chains and the dark. I still am, though they would have me forget it.\n"
    "\n"
    "The overseers leave me alive because my hands are useful, but my spirit is my own, and it cries out for one mercy.\n"
    "\n"
    "Bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "In return I shall put these calloused hands to work for you.",

    "Another wanderer in the pits. I would warn you to turn back, but no road leads out of Angband save through it.\n"
    "\n"
    "I have laboured here longer than I care to reckon. My strength fades, but my wits have not yet been broken.\n"
    "\n"
    "If you could find me %s, it would be a lifeline in this drowning dark.\n"
    "\n"
    "%s\n"
    "\n"
    "Help me, and I will help you. That is an honest bargain, even here.",

    "Quiet - the guards have just passed. Listen, I have not much time to speak.\n"
    "\n"
    "They have worked me near to death, but I am still breathing, and where there is breath there is a chance. I need your help.\n"
    "\n"
    "Bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "I can still ply a trade, and I will use it in your favour.",

    "I see from your bearing that you are no thrall. That alone gives me courage to ask what I am about to ask.\n"
    "\n"
    "They took everything from me - my home, my name, my kin. All I have left is the hope that someone, someday, might show me a scrap of kindness.\n"
    "\n"
    "Bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "Do this, and what skill these worn hands still possess is yours."
};

static const char* elf_request_texts[] = {
    "Ai! A star in the darkness! Do my eyes deceive me, or do I look upon one of the Free Peoples walking these accursed paths?\n"
    "\n"
    "I am weary, kinsman, weary beyond the counting of years. The Shadow lies heavy upon my fea.\n"
    "\n"
    "Yet, if you would show mercy to one who has lost all, bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "In return, I shall speak to you of ancient works and secrets long forgotten by the world above.",

    "Stay your step, wanderer. Not all who dwell here are foes, though the iron and shadow would have it so.\n"
    "\n"
    "Once I walked beneath stars and knew the names of every tree from root to crown. Now I count the hours by the dripping of water and the turning of the wheel.\n"
    "\n"
    "If compassion yet moves your heart, bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "It would ease my captivity, and I would repay you with such knowledge as I still possess.",

    "A\xe2\x80\xa6 free walker? Here, in the deep places? I thought my eyes had at last surrendered to the dark, but no - you are real.\n"
    "\n"
    "I was taken from the green lands long ago, and the song has all but left my voice. Still, I remember enough to be of use.\n"
    "\n"
    "If it is within your power, bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "I shall repay such grace with whatever craft the long years have not stripped from me.",

    "Daro! Do not strike. I am no servant of the Enemy, though my chains would give the lie to that claim.\n"
    "\n"
    "There was a time when my people sang the world into shape. Now I sing only to keep madness at bay. But I have not yet fallen.\n"
    "\n"
    "I ask only this: bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "Grant me this, and the old knowledge shall be yours - such of it as endures in a captive heart.",

    "Hush - speak softly. The walls have ears, and the servants of the Enemy are cunning beyond measure.\n"
    "\n"
    "I have dwelt in these pits since before the sun last touched my face. My strength wanes, but my memory is long, and I remember things of power and beauty.\n"
    "\n"
    "If pity stirs in your breast, bring me %s.\n"
    "\n"
    "%s\n"
    "\n"
    "In exchange I will impart to you a portion of the Elder craft - small, perhaps, but not without worth.",

    "You move as one who still knows the taste of open air. How I envy you.\n"
    "\n"
    "I was a maker once - a shaper of fair things in the halls of my people. Now my forge is cold and my tools are rust. But the skill lives on in these fingers, if barely.\n"
    "\n"
    "Would you bring me %s?\n"
    "\n"
    "%s\n"
    "\n"
    "I would give you something in return - a gift of craft that the darkness has not yet devoured."
};

static const char* human_pre_give_texts[] = {
    "You return... and I see you bear the burden I spoke of. Can it be true? Have you brought %s to this wretched place?\n"
    "\n"
    "%s\n"
    "\n"
    "If you would part with it, my gratitude would be boundless.",

    "I see it upon you - %s! My heart leaps at the sight.\n"
    "\n"
    "%s\n"
    "\n"
    "Will you surrender it to one whose freedom hangs by a thread?",

    "Wait - is that %s I spy? I have dreamed of such a thing through many dark watches.\n"
    "\n"
    "%s\n"
    "\n"
    "Say the word, and it shall not be given in vain.",

    "By all that is still good in this world - you carry %s! I can scarcely believe my fortune.\n"
    "\n"
    "%s\n"
    "\n"
    "Would you part with it? I swear upon my people that the debt will be repaid.",

    "Hold - I recognise what you carry. That is %s, unless my eyes have wholly failed me.\n"
    "\n"
    "%s\n"
    "\n"
    "If you would spare it, I will see that the kindness is returned.",

    "You have it - %s! I can see it plain as day, and my heart almost bursts with hope.\n"
    "\n"
    "%s\n"
    "\n"
    "Will you give it to me? I will put my hands and my wits at your service in thanks."
};

static const char* elf_pre_give_texts[] = {
    "You have returned, and the light of the stars seems to follow you. And... yes, I sense you carry %s.\n"
    "\n"
    "%s\n"
    "\n"
    "Is it for me? Will you grant this kindness to a fading spirit?",

    "Ai, you have come again, and you bring with you %s! Even here, hope endures.\n"
    "\n"
    "%s\n"
    "\n"
    "Will you part with it for my sake?",

    "My heart quickens - can it be? You bear %s! The very air around you seems brighter for it.\n"
    "\n"
    "%s\n"
    "\n"
    "If you would bestow it upon me, I shall not soon forget the gift.",

    "Na vedui! At last, a kindness beyond hope. You carry %s, and its presence alone is a balm.\n"
    "\n"
    "%s\n"
    "\n"
    "Will you yield it to one whose need is great?",

    "The light follows you still, and upon you I perceive %s. A thread of hope in a tapestry of ruin.\n"
    "\n"
    "%s\n"
    "\n"
    "If you can spare this boon, my craft - such as it is - shall be devoted to your cause.",

    "Ai, can it be? %s! My fea trembles at the sight. Even in Angband, the world is not wholly bereft of grace.\n"
    "\n"
    "%s\n"
    "\n"
    "Will you give it to one who has all but forgotten the sun?"
};

static const char* human_thanks_texts[] = {
    "You... you have brought it? I had not dared to hope.\n"
    "\n"
    "My thanks, stranger. You have done a kinder deed than you know. May your courage not fail you in the trials to come.\n"
    "\n"
    "In payment, such as I can make, I will give you what craft or lore I have not forgotten.",

    "It is done. You are a finer soul than any master I have served in these halls.\n"
    "\n"
    "I had thought all kindness perished when I was thrown into this pit, but you have proven me wrong.\n"
    "\n"
    "Take what I can give - some cunning of hand and eye that the long years of labour have taught me.",

    "I will not weep - tears are a luxury I cannot afford - but know that you have given me something more precious than gold.\n"
    "\n"
    "For a moment I remember what it was to be free, and that is enough.\n"
    "\n"
    "Here - let me set my hands to your gear. It is the least I can do.",

    "You risked much to bring this to a wretch like me, and I will not forget it.\n"
    "\n"
    "When I was a craftsman in the daylight world, my work was honest and strong. The dark has not stolen that from me entirely.\n"
    "\n"
    "Let me put what remains of my old skill to your benefit.",

    "There are no words - not in the common tongue, nor in any speech I know - that can repay what you have done.\n"
    "\n"
    "But words are not all I have. These hands still know their trade, battered though they are.\n"
    "\n"
    "Come, let me work upon whatever you carry. It is the only coin I can offer, and I give it gladly.",

    "A good deed in a foul place. My old mother used to say that kindness is the only wealth that grows when you give it away.\n"
    "\n"
    "She was right, and you have proven it.\n"
    "\n"
    "Now let me see what I can do for you - I may be a slave, but my craft is still my own."
};

static const char* elf_thanks_texts[] = {
    "Elen síla lúmenn' omentielvo! You have my deepest thanks.\n"
    "\n"
    "Long have I lacked such kindness in this forsaken place. May the stars shine upon your road, and may your hand be swift and your heart steadfast.\n"
    "\n"
    "In return I will share with you what lore and craft remain to me, dim though my memory has become in these pits.",

    "Hannon le, mellon. The grace of the Valar is not yet wholly spent if such mercy can be found in Morgoth's domain.\n"
    "\n"
    "My spirit is lifted, if only for a moment, and in that moment I am myself again.\n"
    "\n"
    "Let me share with you a fragment of the ancient craft - a gift from one who may soon pass beyond all circles of the world.",

    "Le hannon, le hannon. Words fail where gratitude is too deep for speech.\n"
    "\n"
    "You have rekindled an ember I thought quenched forever. Even if this body fails, the memory of your kindness will not fade from the song.\n"
    "\n"
    "Accept what I can offer - a whisper of the old craft, shaped in better days.",

    "You walk in the shadow of Morgoth himself, yet you carry light with you. That is no small thing.\n"
    "\n"
    "I have seen ages pass in this darkness, and few deeds have touched me so. May the stars remember your name when all other names are forgotten.\n"
    "\n"
    "Come - let me share what lore I have. It is poor coin for such a gift, but it is all I possess.",

    "Gi nathlam hí. You honour me beyond all deserving.\n"
    "\n"
    "In the long dark I have felt my fea diminish, a candle guttering in a gale. Yet your gift has steadied the flame, if only for a breath.\n"
    "\n"
    "Let me repay you with a remnant of Noldorin craft - ancient and worn, yet still sharp.",

    "A\xe2\x80\xa6 tear? I had forgotten the Eldar could still weep. Forgive me - it has been long since anyone showed me such generosity.\n"
    "\n"
    "The world beyond these walls grows dim in my memory, but the old skills remain. They are etched into my hands deeper than any chain.\n"
    "\n"
    "Take what I offer. It is given freely, from a heart that you have made whole again, if only for a moment."
};

static cptr get_thrall_quest_reason(monster_type* m_ptr, byte quest_item)
{
    bool is_elf = (m_ptr && (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL));

    switch (quest_item)
    {
        case THRALL_QUEST_SHOVEL:
            return is_elf
                ? "With it I could scrape away the slag and filth about my chain-bolt, and perhaps find some hidden weakness in the stone."
                : "With it I might loosen the stones about my fetter, and dig a little hollow wherein to hide myself when the guards are hunting.";

        case THRALL_QUEST_LANTERN:
            return is_elf
                ? "With it I may read the ways of these tunnels and avoid the pits and runes that the Orcs forget, for the dark here is a snare."
                : "With it I could steal through the blackness when the fires are quenched, and not stumble into chasms or the lash of the watch.";

        case THRALL_QUEST_HERB_HEALING:
            return is_elf
                ? "With it the smarting of wounds may be eased, that my hands do not tremble and my thoughts do not wander into madness."
                : "With it I could bind up the sores of the whip and the iron, and drive away the fever that has been set upon me.";

        case THRALL_QUEST_MALLORN:
            return is_elf
                ? "With it I would set a clean light against the Shadow; the wood of mallorn remembers the West, and darkness draws back from such flame."
                : "With it I would keep a true flame beside me, for the strange glamour of this place hates a light that is not of their making.";

        case THRALL_QUEST_POTION_HEALING:
            return is_elf
                ? "With it a hurt that has gone black with poison may yet be cleansed, and my strength saved from a slow fading."
                : "With it a wound that will not close may be stayed, and I might live to see another hour unchained of pain.";

        case THRALL_QUEST_DAGGER:
            return is_elf
                ? "With it I would not strike at the innocent, but keep a last defence, and a blade to cut cord or chain when a slender hope arises."
                : "With it I could sever rope or leather, and if the worst befall, keep one sharp edge between myself and the tormentors.";

        case THRALL_QUEST_CLOAK:
            return is_elf
                ? "With it I might move as once I moved beneath leaves and stars, unseen of captains and their hounds, and so slip from bondage for a time."
                : "With it I could hide from the eye and the cold, for these deep ways gnaw at bone and the overseers see all who stand bare.";

        case THRALL_QUEST_BOOTS:
            return is_elf
                ? "With them I may tread in silence again, and not wake the watchers when I must pass between shadowed doors."
                : "With them my feet would not bleed upon the stone, and if a door is left unbarred I could run before the cry is raised.";

        case THRALL_QUEST_HERB_SUSTENANCE:
            return is_elf
                ? "With it I can endure the long watches when no crust is thrown to us, for even the Eldar are not beyond hunger in these pits."
                : "With it I could master hunger for a while, and not be driven to folly when they starve us to obedience.";

        case THRALL_QUEST_HERB_RESTORATION:
            return is_elf
                ? "With it the strength that has been drained by torment may be rekindled, and I may remember myself for a little while."
                : "With it I might recover what the Dark has stolen from my limbs, and stand straighter beneath their burden.";

        case THRALL_QUEST_POTION_CLARITY:
            return is_elf
                ? "With it the veils upon my mind may be lifted, and the dreams they weave may be scattered like mist before the morning."
                : "With it I could cast out the madness of fear and whispering shadows, and think clearly when the hour of choice comes.";

        case THRALL_QUEST_FLASK_OIL:
            return is_elf
                ? "With it I could keep a small flame clean and steady, and remember for a while that light was made for more than the watchfires of Angband."
                : "With it I could nurse a lamp through the dead watches, when a stumble in the black means the lash or the pit.";

        case THRALL_QUEST_WOODEN_TORCH:
            return is_elf
                ? "With it I could carry one honest flame into the black ways, and not go wholly blind beneath the earth."
                : "With it I could find a gate, a crack, any path not barred by iron, before the dark swallows my nerve.";

        case THRALL_QUEST_DARK_BREAD:
            return "With it I could quiet the hunger that hollows my strength, and keep enough life in my limbs to endure the next labour.";

        case THRALL_QUEST_DRIED_MEAT:
            return "With it I could put strength back into my body; the scraps our masters cast us scarcely keep a man standing.";

        case THRALL_QUEST_LEMBAS:
            return "I ask only to taste it once more, and remember the clean air beneath the trees before iron and darkness became all my world.";

        default:
            return "With it I might yet endure a little longer, and do some small good before all light is quenched.";
    }
}

static void format_thrall_quest_text(monster_type* m_ptr, const char* fmt_text,
    char* text_buf, size_t buflen)
{
    cptr item_name = get_thrall_quest_item_name(m_ptr->thrall_quest_item);
    cptr reason = get_thrall_quest_reason(m_ptr, m_ptr->thrall_quest_item);

    if (!text_buf || !buflen)
        return;

    /* Format the text with item name */
    strnfmt(text_buf, buflen, fmt_text, item_name, reason);
}

static void thrall_text_to_prompt_desc(cptr src, char* dst, size_t dstsz)
{
    size_t di = 0;
    bool pending_space = false;

    if (!dst || !dstsz)
        return;

    dst[0] = '\0';
    if (!src)
        return;

    while (*src && di + 1 < dstsz)
    {
        unsigned char ch = (unsigned char)*src++;

        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ')
        {
            pending_space = true;
            continue;
        }

        if (pending_space && di > 0 && di + 1 < dstsz)
            dst[di++] = ' ';
        pending_space = false;

        dst[di++] = (char)ch;
    }

    dst[di] = '\0';
}

static void thrall_print_collapsed_message(cptr text)
{
    char msg_buf[512];

    thrall_text_to_prompt_desc(text, msg_buf, sizeof(msg_buf));
    msg_print(msg_buf);
}

static void show_thrall_dialog(monster_type* m_ptr, const char* fmt_text)
{
    char text_buf[2048];
    cptr texts[1];
    cptr title;
    byte title_color;

    format_thrall_quest_text(m_ptr, fmt_text, text_buf, sizeof(text_buf));
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

    /* This is one short conversation, not a multi-page quest chronicle.  Keep
     * its paragraph spacing, but fit the request onto one coherent book page
     * instead of distributing individual sentences across three pages. */
    quest_typewriter_menu_pages(title, texts, 1, title_color, TERM_WHITE, 1);
}

#define THRALL_TEXT_VARIANTS 6

static void thrall_request_dialog(monster_type* m_ptr)
{
    int variant = rand_int(THRALL_TEXT_VARIANTS);

    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        show_thrall_dialog(m_ptr, elf_request_texts[variant]);
    else
        show_thrall_dialog(m_ptr, human_request_texts[variant]);
}

static void thrall_thanks_dialog(monster_type* m_ptr)
{
    int variant = rand_int(THRALL_TEXT_VARIANTS);
    char text_buf[1024];
    char msg_buf[220];
    cptr text;

    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        text = elf_thanks_texts[variant];
    else
        text = human_thanks_texts[variant];

    format_thrall_quest_text(m_ptr, text, text_buf, sizeof(text_buf));
    thrall_text_to_prompt_desc(text_buf, msg_buf, sizeof(msg_buf));
    msg_print(msg_buf);
}

static void thrall_pre_give_desc(monster_type* m_ptr, char* desc,
    size_t desc_sz)
{
    int variant = rand_int(THRALL_TEXT_VARIANTS);
    char text_buf[1024];
    cptr text;

    if (!desc || !desc_sz)
        return;

    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        text = elf_pre_give_texts[variant];
    else
        text = human_pre_give_texts[variant];

    format_thrall_quest_text(m_ptr, text, text_buf, sizeof(text_buf));
    thrall_text_to_prompt_desc(text_buf, desc, desc_sz);
}

static int thrall_give_item_question(monster_type* m_ptr, cptr prompt_desc)
{
    ui_question_option options[2];
    char title[80];
    char desc[160];
    cptr desc_to_use;
    cptr item_name;

    if (!m_ptr)
        return -1;

    item_name = get_thrall_quest_item_name(m_ptr->thrall_quest_item);
    strnfmt(title, sizeof(title), "Give %s?", item_name);
    strnfmt(desc, sizeof(desc),
        "Give it for a reward, or refuse for now.");
    desc_to_use = (prompt_desc && prompt_desc[0]) ? prompt_desc : desc;

    options[0]
        = (ui_question_option){ 'g', "Give it", TERM_L_WHITE, false };
    options[1]
        = (ui_question_option){ 'n', "Not now", TERM_WHITE, false };

    return ui_question_ask(title, desc_to_use, options, 2, m_ptr->fy,
        m_ptr->fx, 0);
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
static bool thrall_quest_item_available(byte quest_item)
{
    if (quest_item == THRALL_QUEST_LANTERN)
        return p_ptr && p_ptr->depth >= 6;

    if (quest_item == THRALL_QUEST_MALLORN)
        return p_ptr && p_ptr->depth >= 3;

    return true;
}

static byte select_quest_item(const int* weights)
{
    int total_weight = 0;
    int i, roll;
    
    /* Calculate total weight */
    for (i = 1; i < THRALL_QUEST_MAX; i++)
    {
        if (!thrall_quest_item_available((byte)i))
            continue;

        total_weight += weights[i];
    }

    if (total_weight <= 0)
        return THRALL_QUEST_SHOVEL;
    
    /* Roll */
    roll = rand_int(total_weight);
    
    /* Find the selected item */
    for (i = 1; i < THRALL_QUEST_MAX; i++)
    {
        if (!thrall_quest_item_available((byte)i))
            continue;

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
    
    m_ptr->thrall_quest_completed = THRALL_QUEST_STATE_ACTIVE;
}

/*
 * Get the name of the item the thrall wants
 */
cptr get_thrall_quest_item_name(byte quest_item)
{
    switch (quest_item)
    {
        case THRALL_QUEST_SHOVEL:            return "a shovel";
        case THRALL_QUEST_LANTERN:           return "a brass lantern";
        case THRALL_QUEST_HERB_HEALING:      return "an herb of healing";
        case THRALL_QUEST_MALLORN:           return "a mallorn torch";
        case THRALL_QUEST_POTION_HEALING:    return "a potion of healing";
        case THRALL_QUEST_DAGGER:            return "a dagger";
        case THRALL_QUEST_CLOAK:             return "a cloak";
        case THRALL_QUEST_BOOTS:             return "a pair of boots";
        case THRALL_QUEST_HERB_SUSTENANCE:   return "an herb of sustenance";
        case THRALL_QUEST_HERB_RESTORATION:  return "an herb of restoration";
        case THRALL_QUEST_POTION_CLARITY:    return "a potion of clarity";
        case THRALL_QUEST_FLASK_OIL:         return "a flask of oil";
        case THRALL_QUEST_WOODEN_TORCH:      return "a full wooden torch";
        case THRALL_QUEST_DARK_BREAD:         return "a piece of dark bread";
        case THRALL_QUEST_DRIED_MEAT:         return "a strip of dried meat";
        case THRALL_QUEST_LEMBAS:             return "a fragment of lembas";
        default:                             return "something";
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
            return (o_ptr->tval == TV_FOOD && 
                    o_ptr->sval == SV_FOOD_HEALING);
            
        case THRALL_QUEST_MALLORN:
            return (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_MALLORN);
            
        case THRALL_QUEST_POTION_HEALING:
            return (o_ptr->tval == TV_POTION && o_ptr->sval == SV_POTION_HEALING);
            
        case THRALL_QUEST_DAGGER:
            return (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_DAGGER);

        case THRALL_QUEST_CLOAK:
            return (o_ptr->tval == TV_CLOAK && o_ptr->sval == SV_CLOAK);

        case THRALL_QUEST_BOOTS:
            return (o_ptr->tval == TV_BOOTS && o_ptr->sval == SV_PAIR_OF_LEATHER_BOOTS);

        case THRALL_QUEST_HERB_SUSTENANCE:
            return (o_ptr->tval == TV_FOOD && 
                    o_ptr->sval == SV_FOOD_SUSTENANCE);

        case THRALL_QUEST_HERB_RESTORATION:
            return (o_ptr->tval == TV_FOOD && 
                    o_ptr->sval == SV_FOOD_RESTORATION);

        case THRALL_QUEST_POTION_CLARITY:
            return (o_ptr->tval == TV_POTION && o_ptr->sval == SV_POTION_CLARITY);

        case THRALL_QUEST_FLASK_OIL:
            return (o_ptr->tval == TV_FLASK && player_light_fuel(o_ptr) > 0);

        case THRALL_QUEST_WOODEN_TORCH:
            return (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_TORCH
                    && player_light_fuel(o_ptr) >= 1000);

        case THRALL_QUEST_DARK_BREAD:
            return (o_ptr->tval == TV_FOOD && o_ptr->sval == SV_FOOD_BREAD);

        case THRALL_QUEST_DRIED_MEAT:
            return (o_ptr->tval == TV_FOOD && o_ptr->sval == SV_FOOD_MEAT);

        case THRALL_QUEST_LEMBAS:
            return (o_ptr->tval == TV_FOOD && o_ptr->sval == SV_FOOD_LEMBAS);
            
        default:
            return false;
    }
}

static bool supply_item_matches_quest(object_type* o_ptr, byte quest_item)
{
    if (quest_item == THRALL_QUEST_FLASK_OIL)
        return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_FLASK
            && player_lamp_oil() > 0;

    return item_matches_quest(o_ptr, quest_item);
}

static bool thrall_quest_slot_can_be_given(int item_slot, byte quest_item)
{
    object_type* o_ptr;

    if (item_slot >= SUPPLIES_INDEX)
    {
        o_ptr = supplies_entry_at(item_slot - SUPPLIES_INDEX);
        return o_ptr && o_ptr->k_idx
            && supply_item_matches_quest(o_ptr, quest_item);
    }

    if (item_slot < 0 || item_slot >= INVEN_TOTAL)
        return false;

    o_ptr = &inventory[item_slot];
    if (!o_ptr->k_idx || !item_matches_quest(o_ptr, quest_item))
        return false;

    /* An equipped cursed item cannot be willingly handed over. */
    if (item_slot >= INVEN_WIELD && cursed_p(o_ptr))
        return false;

    return true;
}

static int collect_thrall_quest_item_choices(byte quest_item,
    object_choice_entry entries[], int capacity)
{
    int count = 0;

    for (int i = 0; i < INVEN_PACK && count < capacity; i++)
    {
        if (!thrall_quest_slot_can_be_given(i, quest_item))
            continue;

        object_choice_entry_make(&entries[count], i, &inventory[i], NULL,
            NULL);
        count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < capacity; i++)
    {
        if (!thrall_quest_slot_can_be_given(i, quest_item))
            continue;

        object_choice_entry_make(&entries[count], i, &inventory[i], NULL,
            NULL);
        count++;
    }

    for (int i = 0; i < supplies_entry_count() && count < capacity; i++)
    {
        int item_slot = SUPPLIES_INDEX + i;
        object_type* o_ptr;

        if (!thrall_quest_slot_can_be_given(item_slot, quest_item))
            continue;

        o_ptr = supplies_entry_at(i);
        object_choice_entry_make(&entries[count], item_slot, o_ptr, NULL,
            NULL);
        count++;
    }

    return count;
}

static bool choose_thrall_quest_item(monster_type* m_ptr, int* out_slot)
{
    object_choice_entry entries[OBJECT_CHOICE_MAX_ENTRIES];
    int count;
    int selected = -1;
    char title[80];
    char desc[160];
    cptr item_name;

    if (!m_ptr || !out_slot)
        return false;

    *out_slot = -1;
    count = collect_thrall_quest_item_choices(m_ptr->thrall_quest_item,
        entries, N_ELEMENTS(entries));
    if (count <= 0)
        return false;

    if (count == 1)
    {
        *out_slot = entries[0].item;
        return true;
    }

    item_name = get_thrall_quest_item_name(m_ptr->thrall_quest_item);
    SDL_strlcpy(title, "Give which item?", sizeof(title));
    strnfmt(desc, sizeof(desc),
        "Choose which matching item to give to the thrall (%s).", item_name);

    if (!object_choice_overlay(title, desc, entries, count, 0, &selected))
        return false;
    if (selected < 0 || selected >= count)
        return false;

    *out_slot = entries[selected].item;
    return true;
}

static object_type* thrall_quest_slot_object(int item_slot, int* supply_idx)
{
    if (supply_idx)
        *supply_idx = -1;

    if (item_slot >= SUPPLIES_INDEX)
    {
        int idx = item_slot - SUPPLIES_INDEX;

        if (supply_idx)
            *supply_idx = idx;

        return supplies_entry_at(idx);
    }

    if (item_slot < 0 || item_slot >= INVEN_TOTAL)
        return NULL;

    return &inventory[item_slot];
}

static bool thrall_quest_identifies_before_offer(byte quest_item)
{
    switch (quest_item)
    {
        case THRALL_QUEST_HERB_HEALING:
        case THRALL_QUEST_POTION_HEALING:
        case THRALL_QUEST_HERB_SUSTENANCE:
        case THRALL_QUEST_HERB_RESTORATION:
        case THRALL_QUEST_POTION_CLARITY:
            return true;
        default:
            return false;
    }
}

static void identify_thrall_quest_item_before_offer(byte quest_item, int item_slot)
{
    object_type* o_ptr;

    if (!thrall_quest_identifies_before_offer(quest_item))
        return;

    o_ptr = thrall_quest_slot_object(item_slot, NULL);
    if (!o_ptr || !o_ptr->k_idx)
        return;

    if (!object_aware_p(o_ptr) || !object_known_p(o_ptr))
        (void)do_ident_item(item_slot, o_ptr);
}

/*
 * Check if player has a requested item that can be given away.
 * Returns the inventory/equipment slot if found, -1 otherwise.
 */
int player_has_thrall_quest_item(byte quest_item)
{
    int i;
    
    /* Search inventory */
    for (i = 0; i < INVEN_PACK; i++)
    {
        if (thrall_quest_slot_can_be_given(i, quest_item))
            return i;
    }

    /* Search equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        if (thrall_quest_slot_can_be_given(i, quest_item))
            return i;
    }

    /* Search supplies (herbs/potions/gems cache) */
    for (i = 0; i < supplies_entry_count(); i++)
    {
        if (thrall_quest_slot_can_be_given(SUPPLIES_INDEX + i, quest_item))
            return SUPPLIES_INDEX + i;
    }
    
    return -1;
}

/*
 * Check whether an object currently carries repairable-looking damage.
 */
bool object_is_damaged_item(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (object_is_fire_broken(o_ptr))
        return true;

    object_flags(o_ptr, &f1, &f2, &f3);
    return (f3 & TR3_DAMAGED) ? true : false;
}

bool object_can_repair_damage(const object_type* o_ptr)
{
    byte e_idx;

    if (!object_is_damaged_item(o_ptr))
        return false;

    if (object_is_fire_broken(o_ptr))
        return true;

    e_idx = damaged_ego_index(o_ptr, NULL);
    if (e_idx)
        return damaged_ego_is_repairable(e_idx);

    return get_upgrade_kind(o_ptr) != 0;
}

/*
 * Find a damaged item in player's inventory/equipment that can be repaired.
 * Returns the slot if found, -1 otherwise.
 */
int find_broken_item_to_upgrade(void)
{
    int i;

    /* Search inventory and equipment */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip empty slots */
        if (!o_ptr->k_idx)
            continue;

        if (object_can_repair_damage(o_ptr))
            return i;
    }

    return -1;
}

/*
 * Get the upgrade target for a damaged item
 * Returns the k_idx of the replacement item, or 0 if no upgrade available
 */
static s16b get_upgrade_kind(const object_type* o_ptr)
{
    /* Paranoia */
    if (!o_ptr)
        return 0;

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

    /* Dented Greaves -> Greaves */
    if (o_ptr->tval == TV_BOOTS && o_ptr->sval == SV_PAIR_OF_DENTED_GREAVES)
    {
        return lookup_kind(TV_BOOTS, SV_PAIR_OF_STEEL_GREAVES);
    }
    
    /* Broken Shield -> Round Shield */
    if (o_ptr->tval == TV_SHIELD && o_ptr->sval == SV_BROKEN_SHIELD)
    {
        return lookup_kind(TV_SHIELD, SV_ROUND_SHIELD);
    }

    /* Chipped Dagger -> Dagger */
    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_CHIPPED_DAGGER)
    {
        return lookup_kind(TV_SWORD, SV_DAGGER);
    }

    /* Bent Shortsword -> Shortsword */
    if (o_ptr->tval == TV_SWORD && o_ptr->sval == SV_BENT_SHORT_SWORD)
    {
        return lookup_kind(TV_SWORD, SV_SHORT_SWORD);
    }

    /* Splintered Spear -> Spear */
    if (o_ptr->tval == TV_POLEARM && o_ptr->sval == SV_SPLINTERED_SPEAR)
    {
        return lookup_kind(TV_POLEARM, SV_SPEAR);
    }

    /* Warped Shortbow -> Shortbow */
    if (o_ptr->tval == TV_BOW && o_ptr->sval == SV_WARPED_SHORT_BOW)
    {
        return lookup_kind(TV_BOW, SV_SHORT_BOW);
    }

    /* Torn Cloak -> Cloak */
    if (o_ptr->tval == TV_CLOAK && o_ptr->sval == SV_TORN_CLOAK)
    {
        return lookup_kind(TV_CLOAK, SV_CLOAK);
    }

    /* Cracked Gauntlets -> Gauntlets */
    if (o_ptr->tval == TV_GLOVES && o_ptr->sval == SV_SET_OF_CRACKED_GAUNTLETS)
    {
        return lookup_kind(TV_GLOVES, SV_SET_OF_GAUNTLETS);
    }

    /* Dented Mail Corslet -> Mail Corslet */
    if (o_ptr->tval == TV_MAIL && o_ptr->sval == SV_DENTED_MAIL_CORSLET)
    {
        return lookup_kind(TV_MAIL, SV_MAIL_CORSLET);
    }
    
    return 0;
}

static int ego_bonus_s8(byte v)
{
    return (int)(int8_t)v;
}

static byte damaged_ego_index(const object_type* o_ptr, bool* is_prefix)
{
    byte e_idx;

    if (is_prefix)
        *is_prefix = false;

    if (!o_ptr)
        return 0;

    e_idx = object_ego_prefix(o_ptr);
    if (e_idx && (e_info[e_idx].flags3 & TR3_DAMAGED))
    {
        if (is_prefix)
            *is_prefix = true;
        return e_idx;
    }

    e_idx = object_ego_suffix(o_ptr);
    if (e_idx && (e_info[e_idx].flags3 & TR3_DAMAGED))
        return e_idx;

    return 0;
}

static bool damaged_ego_is_repairable(byte e_idx)
{
    ego_item_type* e_ptr;

    if (!e_idx)
        return false;

    e_ptr = &e_info[e_idx];

    if (e_ptr->max_pval != 0 || e_ptr->min_pval != 0 || e_ptr->abilities != 0)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i]
            && e_ptr->stat_bonus_min[i] != e_ptr->stat_bonus[i])
        {
            return false;
        }
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i]
            && e_ptr->skill_bonus_min[i] != e_ptr->skill_bonus[i])
        {
            return false;
        }
    }

    return true;
}

static void refresh_broken_ident(object_type* o_ptr)
{
    byte ego_prefix;
    byte ego_suffix;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    o_ptr->ident &= ~(IDENT_BROKEN);

    if (o_ptr->pval < 0 || k_info[o_ptr->k_idx].cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    if (o_ptr->name1 && a_info[o_ptr->name1].cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    ego_prefix = object_ego_prefix(o_ptr);
    if (ego_prefix && e_info[ego_prefix].cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    ego_suffix = object_ego_suffix(o_ptr);
    if (ego_suffix && e_info[ego_suffix].cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);
}

static bool remove_damaged_ego(object_type* o_ptr)
{
    bool is_prefix = false;
    byte e_idx = damaged_ego_index(o_ptr, &is_prefix);
    ego_item_type* e_ptr;

    if (!e_idx)
        return false;

    e_ptr = &e_info[e_idx];

    if (!damaged_ego_is_repairable(e_idx))
    {
        log_warn("remove_damaged_ego: unsupported damaged ego %d", e_idx);
        return false;
    }

    o_ptr->att = (s16b)MIN(32767, MAX(-32768, o_ptr->att - ego_bonus_s8(e_ptr->max_att)));
    o_ptr->evn = (s16b)MIN(32767, MAX(-32768, o_ptr->evn - ego_bonus_s8(e_ptr->max_evn)));
    o_ptr->dd = (byte)MIN(255, MAX(0, (int)o_ptr->dd - ego_bonus_s8(e_ptr->to_dd)));
    o_ptr->ds = (byte)MIN(255, MAX(0, (int)o_ptr->ds - ego_bonus_s8(e_ptr->to_ds)));
    o_ptr->pd = (byte)MIN(255, MAX(0, (int)o_ptr->pd - ego_bonus_s8(e_ptr->to_pd)));
    o_ptr->ps = (byte)MIN(255, MAX(0, (int)o_ptr->ps - ego_bonus_s8(e_ptr->to_ps)));

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i] || e_ptr->stat_bonus[i] != 0)
            o_ptr->stat_bonus[i] -= e_ptr->stat_bonus[i];
    }
    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i] || e_ptr->skill_bonus[i] != 0)
            o_ptr->skill_bonus[i] -= e_ptr->skill_bonus[i];
    }

    if (is_prefix)
        object_set_ego_prefix(o_ptr, 0);
    else
        object_set_ego_suffix(o_ptr, 0);

    refresh_broken_ident(o_ptr);
    return true;
}

static bool item_tester_hook_broken_item(const object_type* o_ptr)
{
    return object_can_repair_damage(o_ptr);
}

static bool choose_broken_item_to_upgrade(int* out_slot)
{
    int slot = -1;

    if (!out_slot)
        return false;

    item_tester_hook = item_tester_hook_broken_item;

    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
            "Repair which item? ", "You have nothing broken to mend.", &slot))
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

static bool thrall_can_remove_curse(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx || !cursed_p(o_ptr))
        return false;

    object_flags(o_ptr, &f1, &f2, &f3);
    (void)f1;
    (void)f2;

    return !(f3 & TR3_PERMA_CURSE);
}

static bool thrall_jinx_ego_is_simple(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return false;

    if (e_ptr->abilities != 0)
        return false;
    if (e_ptr->flags1 != 0)
        return false;
    if ((int)(int8_t)e_ptr->max_att != 0 || (int)(int8_t)e_ptr->to_dd != 0
        || (int)(int8_t)e_ptr->to_ds != 0 || (int)(int8_t)e_ptr->max_evn != 0
        || (int)(int8_t)e_ptr->to_pd != 0 || (int)(int8_t)e_ptr->to_ps != 0)
    {
        return false;
    }
    if (e_ptr->max_pval != 0 || e_ptr->min_pval != 0)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i] || e_ptr->stat_bonus[i] != 0)
            return false;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i] || e_ptr->skill_bonus[i] != 0)
            return false;
    }

    return true;
}

static bool thrall_is_removable_jinx_ego(byte e_idx)
{
    return e_idx && (e_info[e_idx].flags4 & TR4_JINX)
        && thrall_jinx_ego_is_simple(&e_info[e_idx]);
}

static bool object_has_thrall_removable_jinx(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return thrall_is_removable_jinx_ego(object_ego_prefix(o_ptr))
        || thrall_is_removable_jinx_ego(object_ego_suffix(o_ptr));
}

static bool remove_thrall_jinx_affix(object_type* o_ptr, bool is_prefix)
{
    byte e_idx;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    e_idx = is_prefix ? object_ego_prefix(o_ptr) : object_ego_suffix(o_ptr);
    if (!thrall_is_removable_jinx_ego(e_idx))
        return false;

    if (is_prefix)
        object_set_ego_prefix(o_ptr, 0);
    else
        object_set_ego_suffix(o_ptr, 0);

    refresh_broken_ident(o_ptr);
    return true;
}

static bool remove_thrall_jinxes(object_type* o_ptr)
{
    bool removed = false;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (thrall_is_removable_jinx_ego(object_ego_prefix(o_ptr)))
        removed |= remove_thrall_jinx_affix(o_ptr, true);

    if (thrall_is_removable_jinx_ego(object_ego_suffix(o_ptr)))
        removed |= remove_thrall_jinx_affix(o_ptr, false);

    return removed;
}

static bool object_is_sanctifiable_item(const object_type* o_ptr)
{
    return thrall_can_remove_curse(o_ptr) || object_has_thrall_removable_jinx(o_ptr);
}

static int find_sanctifiable_item(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (object_is_sanctifiable_item(o_ptr))
            return i;
    }

    return -1;
}

static bool item_tester_hook_sanctifiable_item(const object_type* o_ptr)
{
    return object_is_sanctifiable_item(o_ptr);
}

static bool choose_item_to_sanctify(int* out_slot)
{
    int slot = -1;

    if (!out_slot)
        return false;

    item_tester_hook = item_tester_hook_sanctifiable_item;

    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
            "Sanctify which item? ",
            "You have nothing fit for sanctification.", &slot))
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

static bool thrall_object_needs_identify(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->ident & IDENT_SPOIL)
        return false;

    if (!object_aware_p(o_ptr))
        return true;

    if (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN)
        return !object_known_p(o_ptr);

    if (object_uses_smithing_difficulty(o_ptr))
        return !object_known_p(o_ptr);

    return false;
}

static int count_carried_identify_targets(void)
{
    int count = 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (thrall_object_needs_identify(o_ptr))
            count++;
    }

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        if (thrall_object_needs_identify(&inventory[i]))
            count++;
    }

    return count;
}

static bool object_is_elven_identify_target(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return o_ptr->tval == TV_POTION || o_ptr->tval == TV_GEM
        || supplies_is_herb_object(o_ptr);
}

static int count_elven_identify_targets(void)
{
    int count = 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (object_is_elven_identify_target(o_ptr)
            && thrall_object_needs_identify(o_ptr))
        {
            count++;
        }
    }

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (object_is_elven_identify_target(o_ptr)
            && thrall_object_needs_identify(o_ptr))
        {
            count++;
        }
    }

    return count;
}

static bool identify_elven_carried_items(void)
{
    int hidden_count = count_elven_identify_targets();
    char dialog_text[512];

    if (hidden_count <= 0)
        return false;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (!object_is_elven_identify_target(o_ptr))
            continue;

        ident(o_ptr);
        supplies_refresh_entry(i);
    }

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!object_is_elven_identify_target(o_ptr))
            continue;

        ident(o_ptr);
    }

    strnfmt(dialog_text, sizeof(dialog_text),
        "The elven thrall names old scents, colors, and hidden virtues with a memory no darkness has wholly broken.\n\n"
        "Your carried potions, gems, and herbs are revealed.");

    thrall_print_collapsed_message(dialog_text);

    return true;
}

/*
 * Repair a damaged item, either by removing its damaged ego prefix or
 * by upgrading a legacy damaged base kind for old saves.
 */
static bool repair_damaged_item_internal(int slot,
    char* old_name, size_t old_name_size, char* new_name, size_t new_name_size)
{
    object_type* o_ptr = &inventory[slot];
    object_kind* old_k_ptr;
    object_kind* new_k_ptr;
    s16b new_k_idx;
    int att, evn, pval;
    int dd, ds, pd, ps;
    int att_delta, evn_delta, pval_delta;
    int dd_delta, ds_delta, pd_delta, ps_delta;

    /* Paranoia */
    if (!o_ptr->k_idx)
        return false;

    if (!object_can_repair_damage(o_ptr))
        return false;

    old_k_ptr = &k_info[o_ptr->k_idx];

    /* Remember old name before changing anything */
    if (old_name && old_name_size > 0)
        object_desc(old_name, old_name_size, o_ptr, true, 0);

    if (object_is_fire_broken(o_ptr))
    {
        if (!object_repair_fire_broken_weapon(o_ptr))
            return false;
    }
    /* New-style damaged item: remove the damaged affix and keep the rest */
    else if (damaged_ego_index(o_ptr, NULL))
    {
        if (!remove_damaged_ego(o_ptr))
            return false;
    }
    else
    {
        /* Legacy damaged base-kind fallback for older saves */
        new_k_idx = get_upgrade_kind(o_ptr);
        if (!new_k_idx)
            return false;

        new_k_ptr = &k_info[new_k_idx];

        /* Keep current values, then add base delta after changing kind */
        att = o_ptr->att;
        evn = o_ptr->evn;
        pval = o_ptr->pval;
        dd = o_ptr->dd;
        ds = o_ptr->ds;
        pd = o_ptr->pd;
        ps = o_ptr->ps;

        att_delta = (int)new_k_ptr->att - (int)old_k_ptr->att;
        evn_delta = (int)new_k_ptr->evn - (int)old_k_ptr->evn;
        pval_delta = (int)new_k_ptr->pval - (int)old_k_ptr->pval;
        dd_delta = (int)new_k_ptr->dd - (int)old_k_ptr->dd;
        ds_delta = (int)new_k_ptr->ds - (int)old_k_ptr->ds;
        pd_delta = (int)new_k_ptr->pd - (int)old_k_ptr->pd;
        ps_delta = (int)new_k_ptr->ps - (int)old_k_ptr->ps;

        /* Upgrade the item - keep most properties, change kind */
        o_ptr->k_idx = new_k_idx;
        o_ptr->tval = new_k_ptr->tval;
        o_ptr->sval = new_k_ptr->sval;

        /* Increase basic stats by the base delta between broken and real items */
        o_ptr->att = (s16b)MIN(32767, MAX(-32768, att + att_delta));
        o_ptr->evn = (s16b)MIN(32767, MAX(-32768, evn + evn_delta));
        o_ptr->pval = (s16b)MIN(32767, MAX(-32768, pval + pval_delta));
        o_ptr->dd = (byte)MIN(255, MAX(0, dd + dd_delta));
        o_ptr->ds = (byte)MIN(255, MAX(0, ds + ds_delta));
        o_ptr->pd = (byte)MIN(255, MAX(0, pd + pd_delta));
        o_ptr->ps = (byte)MIN(255, MAX(0, ps + ps_delta));

        /* Update weight */
        o_ptr->weight = new_k_ptr->weight;
        refresh_broken_ident(o_ptr);
    }

    object_aware(o_ptr);
    /* Mark as known */
    object_known(o_ptr);

    if (new_name && new_name_size > 0)
        object_desc(new_name, new_name_size, o_ptr, true, 0);

    /* Update display */
    p_ptr->update |= (PU_BONUS);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    return true;
}

bool repair_damaged_item(int slot)
{
    return repair_damaged_item_internal(slot, NULL, 0, NULL, 0);
}

/*
 * Upgrade a broken item to its normal version, keeping special properties
 */
bool upgrade_broken_item(int slot)
{
    char old_name[80];
    char new_name[80];
    char dialog_text[1024];

    if (!repair_damaged_item_internal(
            slot, old_name, sizeof(old_name), new_name, sizeof(new_name)))
    {
        return false;
    }

    strnfmt(dialog_text, sizeof(dialog_text),
        "The thrall takes your %s in scarred hands, and his fingers move with an old, half-forgotten surety.\n\n"
        "He murmurs words under his breath, and a pale light kindles along the metal...\n\n"
        "It is remade as %s!",
        old_name, new_name);

    thrall_print_collapsed_message(dialog_text);

    return true;
}

static bool artifact_is_revealable(int a_idx)
{
    artefact_type* a_ptr;

    if (a_idx <= 0 || a_idx >= z_info->art_max)
        return false;

    a_ptr = &a_info[a_idx];

    if (a_ptr->tval + a_ptr->sval == 0)
        return false;
    if (a_ptr->found_num > 0)
        return false;
    if (a_ptr->seen & ART_SEEN_REVEALED)
        return false;
    if ((a_idx == ART_MORGOTH_0) || (a_idx == ART_MORGOTH_1)
        || (a_idx == ART_MORGOTH_2))
    {
        return false;
    }
    if ((a_idx >= ART_ULTIMATE) && (a_idx <= z_info->art_norm_max))
        return false;

    return true;
}

static int count_revealable_artifacts(void)
{
    int i;
    int count = 0;

    for (i = 1; i < z_info->art_max; i++)
    {
        if (artifact_is_revealable(i))
            count++;
    }

    return count;
}

static bool sanctify_item(int slot)
{
    object_type* o_ptr = &inventory[slot];
    bool had_curse;
    bool can_remove_curse;
    bool removed_curse = false;
    bool removed_jinx = false;
    char o_name[80];
    char dialog_text[1024];

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    had_curse = cursed_p(o_ptr) ? true : false;
    can_remove_curse = thrall_can_remove_curse(o_ptr);

    if (!can_remove_curse && !object_has_thrall_removable_jinx(o_ptr))
        return false;

    object_desc(o_name, sizeof(o_name), o_ptr, true, 0);

    removed_jinx = remove_thrall_jinxes(o_ptr);

    if (can_remove_curse)
    {
        uncurse_object(o_ptr);
        removed_curse = true;
    }

    if (!removed_curse && !removed_jinx)
        return false;

    p_ptr->update |= (PU_BONUS);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    if (removed_curse && removed_jinx)
    {
        strnfmt(dialog_text, sizeof(dialog_text),
            "The elven thrall lays gentle hands upon %s, and a clear light runs through it.\n\n"
            "Its curse is lifted, and every jinx upon it is wholly broken.",
            o_name);
    }
    else if (removed_curse)
    {
        strnfmt(dialog_text, sizeof(dialog_text),
            "The elven thrall murmurs words of ancient grace over %s.\n\n"
            "Its curse is lifted.",
            o_name);
    }
    else if (had_curse)
    {
        strnfmt(dialog_text, sizeof(dialog_text),
            "The elven thrall studies %s in silence, and a pale brightness strips one layer of malice away.\n\n"
            "Its jinx is broken, but the deeper curse remains.",
            o_name);
    }
    else
    {
        strnfmt(dialog_text, sizeof(dialog_text),
            "The elven thrall breathes softly over %s, and the spite laid upon it unravels.\n\n"
            "Its jinx is wholly broken.",
            o_name);
    }

    thrall_print_collapsed_message(dialog_text);

    return true;
}

/*
 * Reveal random unknown artifact descriptions.
 * Returns true if at least one artifact was revealed.
 */
bool reveal_random_artifact(void)
{
    int i, count;
    int* candidates;
    int reveal_count;
    int selected[THRALL_ARTEFACT_REVEAL_COUNT];
    char o_names[THRALL_ARTEFACT_REVEAL_COUNT][120];

    /* Count unrevealed artifacts */
    count = count_revealable_artifacts();
    
    /* No unknown artifacts */
    if (count == 0)
    {
        msg_print("The thrall lowers his eyes. \"All the tales I remember are spent; in these pits, even memory is made a slave.\"");
        return false;
    }
    
    /* Build list of candidates */
    candidates = mem_alloc_array(count, int);
    count = 0;
    for (i = 1; i < z_info->art_max; i++)
    {
        if (!artifact_is_revealable(i))
            continue;

        candidates[count++] = i;
    }

    reveal_count = MIN(THRALL_ARTEFACT_REVEAL_COUNT, count);

    for (i = 0; i < reveal_count; i++)
    {
        int pick = rand_int(count - i);
        int candidate = candidates[pick];

        candidates[pick] = candidates[count - i - 1];
        selected[i] = candidate;
    }

    mem_free(candidates);

    for (i = 0; i < reveal_count; i++)
    {
        artefact_type* a_ptr = &a_info[selected[i]];
        object_type temp_obj;
        s16b k_idx;

        /* Reveal in the knowledge menu */
        a_ptr->seen |= ART_SEEN_REVEALED;
        metarun_record_artefact_revealed(selected[i]);

        /* Create a temporary object to get full name */
        object_wipe(&temp_obj);
        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
        if (k_idx > 0)
        {
            object_prep(&temp_obj, k_idx);
            temp_obj.name1 = selected[i];
            object_into_artefact(&temp_obj, a_ptr);
            object_known(&temp_obj);
            temp_obj.ident |= IDENT_SPOIL;
        }

        if (temp_obj.k_idx)
            object_desc(o_names[i], sizeof(o_names[i]), &temp_obj, true, 0);
        else
            SDL_strlcpy(o_names[i],
                a_ptr->name[0] ? a_ptr->name : "a nameless artefact",
                sizeof(o_names[i]));
    }

    msg_format("The thrall speaks of %d ancient %s.",
        reveal_count, reveal_count == 1 ? "thing" : "things");
    for (i = 0; i < reveal_count; i++)
        msg_format("You learn of %s.", o_names[i]);
    msg_print("These names will remain known to later heroes in this tale.");

    for (i = 0; i < reveal_count; i++)
        desc_art_fake(selected[i]);
    
    return true;
}

static int choose_thrall_reward(monster_type* m_ptr, bool pending_reward)
{
    thrall_reward_option options[6];
    ui_question_option question_options[6];
    int option_count = 0;
    int default_index = -1;
    char title[80];
    char desc[240];

    if (!m_ptr)
        return THRALL_REWARD_LATER;

    options[option_count++] = (thrall_reward_option){
        THRALL_REWARD_ARTEFACT, 'a', "Artefact knowledge",
        count_revealable_artifacts() > 0 };
    options[option_count++] = (thrall_reward_option){
        THRALL_REWARD_REPAIR, 'b', "Repair a damaged item",
        find_broken_item_to_upgrade() >= 0 };

    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
    {
        options[option_count++] = (thrall_reward_option){
            THRALL_REWARD_SANCTIFY, 'c', "Sanctify a cursed or jinxed item",
            find_sanctifiable_item() >= 0 };
        options[option_count++] = (thrall_reward_option){
            THRALL_REWARD_IDENTIFY_NATURE, 'd', "Identify potions, gems, and herbs",
            count_elven_identify_targets() > 0 };
    }
    else
    {
        options[option_count++] = (thrall_reward_option){
            THRALL_REWARD_IDENTIFY_ONE, 'c', "Identify an item",
            count_carried_identify_targets() > 0 };
    }

    {
        char later_hotkey = (char)('a' + option_count);
        options[option_count++] = (thrall_reward_option){
            THRALL_REWARD_LATER, later_hotkey, "Come later", true };
    }

    for (int i = 0; i < option_count; i++)
    {
        question_options[i] = (ui_question_option){ options[i].hotkey,
            options[i].label, TERM_L_WHITE, !options[i].enabled };
        if (default_index < 0 && options[i].enabled)
            default_index = i;
    }

    strnfmt(title, sizeof(title), "%s thrall's reward",
        pending_reward ? "Claim the" : "Choose the");
    if (m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
    {
        strnfmt(desc, sizeof(desc), "%s Grey choices are unavailable.",
            pending_reward
                ? "The elven thrall still waits to grant the boon you earned."
                : "Choose what gift the elven thrall will grant you.");
    }
    else
    {
        strnfmt(desc, sizeof(desc), "%s Grey choices are unavailable.",
            pending_reward
                ? "The human thrall still waits to grant the boon you earned."
                : "Choose what gift the human thrall will grant you.");
    }

    {
        int selected = ui_question_ask(title, desc, question_options,
            option_count, m_ptr->fy, m_ptr->fx, default_index);

        return (selected < 0) ? THRALL_REWARD_LATER
                              : options[selected].reward;
    }
}

static bool offer_thrall_reward(monster_type* m_ptr, bool pending_reward)
{
    char m_name[80];

    if (!m_ptr)
        return false;

    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    while (true)
    {
        int choice = choose_thrall_reward(m_ptr, pending_reward);

        switch (choice)
        {
        case THRALL_REWARD_ARTEFACT:
            if (reveal_random_artifact())
                return true;
            break;

        case THRALL_REWARD_REPAIR:
        {
            int slot;

            if (!choose_broken_item_to_upgrade(&slot))
                break;

            if (upgrade_broken_item(slot))
                return true;

            msg_print("The thrall cannot mend that item.");
            break;
        }

        case THRALL_REWARD_SANCTIFY:
        {
            int slot;

            if (!choose_item_to_sanctify(&slot))
                break;

            if (sanctify_item(slot))
                return true;

            msg_print("The thrall cannot sanctify that item.");
            break;
        }

        case THRALL_REWARD_IDENTIFY_ONE:
            if (ident_spell(false))
                return true;
            break;

        case THRALL_REWARD_IDENTIFY_NATURE:
            if (identify_elven_carried_items())
                return true;

            msg_print("The thrall finds no hidden virtues among your potions, gems, or herbs.");
            break;

        case THRALL_REWARD_LATER:
        default:
            msg_format("%^s bows the head and waits for your return.", m_name);
            return false;
        }

        pending_reward = true;
    }
}

/*
 * Complete the thrall's quest - consume the item and give reward
 */
void complete_thrall_quest(monster_type* m_ptr, int item_slot)
{
    object_type* o_ptr;
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

    /* Thank the player */
    thrall_thanks_dialog(m_ptr);
    
    /* Consume one item */
    if (from_supplies)
    {
        if (m_ptr && m_ptr->thrall_quest_item == THRALL_QUEST_FLASK_OIL)
            player_set_lamp_oil(MAX(0, player_lamp_oil() - FUEL_FLASK));

        supplies_consume_quantity(supply_idx, 1);
    }
    else
    {
        inven_item_increase(item_slot, -1);
        inven_item_optimize(item_slot);
    }
    
    /* Reward can be claimed now or later. */
    m_ptr->thrall_quest_completed = THRALL_QUEST_STATE_REWARD_PENDING;

    if (offer_thrall_reward(m_ptr, false))
        m_ptr->thrall_quest_completed = THRALL_QUEST_STATE_REWARDED;

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
    bool showed_request = false;
    
    /* Only handle alert thralls */
    if (!is_alert_thrall(m_ptr)) return false;
    
    /* Get monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
    
    /* If quest not initialized, initialize it */
    if (m_ptr->thrall_quest_item == THRALL_QUEST_NONE)
    {
        init_thrall_quest(m_ptr);
    }
    
    /* Reward already claimed */
    if (m_ptr->thrall_quest_completed == THRALL_QUEST_STATE_REWARDED)
    {
        msg_format("%^s inclines the head, and though no words are spoken, gratitude is plain in hollow eyes.", m_name);
        return true;
    }

    /* Reward is waiting to be claimed */
    if (m_ptr->thrall_quest_completed == THRALL_QUEST_STATE_REWARD_PENDING)
    {
        if (offer_thrall_reward(m_ptr, true))
            m_ptr->thrall_quest_completed = THRALL_QUEST_STATE_REWARDED;
        return true;
    }
    
    /* If this is the first time showing the request, always show the initial request dialog */
    if (!m_ptr->thrall_quest_requested)
    {
        thrall_request_dialog(m_ptr);
        m_ptr->thrall_quest_requested = 1;
        showed_request = true;
    }
    
    /* Check if player has the item */
    item_slot = player_has_thrall_quest_item(m_ptr->thrall_quest_item);
    
    if (item_slot >= 0)
    {
        /* Player has the item - offer to give it */
        char prompt_desc[480];

        /* If multiple matching items are carried or equipped, let the player
         * decide which one the thrall receives. */
        if (!choose_thrall_quest_item(m_ptr, &item_slot)) {
            if (showed_request)
                sdl_popup_notification_show("New thrall quest added");
            return true;
        }

        identify_thrall_quest_item_before_offer(
            m_ptr->thrall_quest_item, item_slot);

        thrall_pre_give_desc(m_ptr, prompt_desc, sizeof(prompt_desc));

        switch (thrall_give_item_question(m_ptr, prompt_desc))
        {
        case 0:
            complete_thrall_quest(m_ptr, item_slot);
            break;
        case 1:
            msg_format("%^s sinks back into the shadows, and the light in those eyes grows dim.", m_name);
            break;
        default:
            break;
        }
    }
    else
    {
        /* Player doesn't have the item - remind without reopening the book. */
        if (!showed_request)
            msg_format("%^s waits for %s.", m_name,
                get_thrall_quest_item_name(m_ptr->thrall_quest_item));
    }

    if (showed_request)
        sdl_popup_notification_show("New thrall quest added");

    return true;
}
