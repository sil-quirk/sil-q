#!/usr/bin/env python3
"""
Calculate the distribution of smithable items and artefacts by difficulty.

For each difficulty value, counts:
1. Number of distinct craftable items (base items with all possible stat modifications + specials)
2. Number of artefacts

Base items can have:
- Attack bonus: typically +0 to +1 (weapons can go higher with specials/artefacts)
- Damage sides: +0 to +1 on weapons
- Evasion: +0 to +1 on armor
- Protection sides: +0 to +1 on armor
- Pval: depends on flags present

Specials add additional bonuses and flags on top of base items.
"""

import os
import sys
from collections import defaultdict

# Import from existing script
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)

from calc_artefact_difficulty import (
    parse_artefact_file, parse_special_file, dif_mod_calc,
    get_base_level, get_base_ds, get_base_protection, get_base_att, get_base_evn,
    get_base_pval, get_slot, get_tval_name
)


def get_base_flags_for_jewelry(tval, sval):
    """Get base flags for jewelry items from object.txt."""
    # Amulets (tval 40)
    amulet_flags = {
        0: ['CHEAT_DEATH'],  # Last Chances
        1: ['CON', 'SUST_CON'],  # Constitution
        2: ['GRA', 'SUST_GRA'],  # Grace
        3: ['REGEN'],  # Regeneration
        4: ['SUST_CON', 'SUST_GRA', 'SLOW_DIGEST'],  # Preservation
        5: ['GRA', 'SUST_GRA', 'LIGHT'],  # the Blessed Realm
        6: ['HAUNTED', 'SEE_INVIS'],  # Haunted Dreams
        7: ['PERCEPTION', 'RES_HALLU'],  # the Vigilant Eye (also has ability)
    }
    
    # Rings (tval 45)
    ring_flags = {
        0: ['PERCEPTION'],  # Secrets (also has ability)
        1: ['WILL', 'RES_FEAR', 'RES_CONFU'],  # Ered Luin
        2: [],  # Evasion (evn bonus only)
        3: [],  # Protection (ps bonus only)
        4: ['STR', 'SUST_STR'],  # Strength
        5: ['DEX', 'SUST_DEX'],  # Dexterity
        6: ['RES_FIRE'],  # Frost
        7: ['RES_COLD'],  # Warmth
        8: [],  # Accuracy (att bonus only)
        9: ['FREE_ACT'],  # Free Action
    }
    
    if tval == 40:
        return amulet_flags.get(sval, [])
    elif tval == 45:
        return ring_flags.get(sval, [])
    return []


def get_base_abilities_for_jewelry(tval, sval):
    """Get base abilities count for jewelry items from object.txt."""
    # These items have abilities in their base form
    jewelry_abilities = {
        (40, 7): 1,  # Vigilant Eye has ability 4/2 (Keen Senses)
        (45, 0): 1,  # Secrets has ability 4/4 (Alchemy)
    }
    return jewelry_abilities.get((tval, sval), 0)


# Base items that can be smithed (from object.txt, excluding NO_SMITHING and INSTA_ART items)
# Format: (tval, sval, name)
SMITHABLE_BASE_ITEMS = [
    # Soft Armor (tval 36)
    (36, 2, "Robe"),
    (36, 4, "Leather Armour"),
    (36, 7, "Studded Leather"),
    # (36, 11, "Galvorn Armour"),  # NO_SMITHING
    
    # Mail (tval 37)
    (37, 4, "Mail Corslet"),
    (37, 6, "Hauberk"),
    (37, 20, "Mithril Corslet"),
    
    # Shields (tval 34)
    (34, 3, "Round Shield"),
    (34, 5, "Kite Shield"),
    (34, 10, "Mithril Shield"),
    
    # Helms (tval 32)
    (32, 5, "Helm"),
    (32, 6, "Great Helm"),
    (32, 7, "Dwarf Mask"),
    (32, 8, "Mithril Helm"),
    
    # Cloaks (tval 35)
    (35, 1, "Cloak"),
    (35, 6, "Shadow Cloak"),
    
    # Boots (tval 30)
    (30, 1, "Boots"),
    (30, 2, "Greaves"),
    (30, 3, "Mithril Greaves"),
    
    # Gloves (tval 31)
    (31, 1, "Gloves"),
    (31, 2, "Gauntlets"),
    (31, 3, "Mithril Gauntlets"),
    
    # Swords (tval 23)
    (23, 4, "Dagger"),
    (23, 7, "Curved Sword"),
    (23, 10, "Shortsword"),
    (23, 17, "Longsword"),
    (23, 21, "Bastard Sword"),
    (23, 25, "Greatsword"),
    (23, 28, "Mithril Longsword"),
    (23, 30, "Mithril Greatsword"),
    
    # Polearms (tval 22)
    (22, 1, "Spear"),
    (22, 2, "Great Spear"),
    (22, 4, "Glaive"),
    (22, 11, "Hand Axe"),
    (22, 12, "Battle Axe"),
    (22, 13, "Great Axe"),
    
    # Hafted (tval 21)
    (21, 3, "Quarterstaff"),
    (21, 8, "War Hammer"),
    
    # Digging (tval 20)
    (20, 1, "Shovel"),
    (20, 3, "Mattock"),
    
    # Bows (tval 19)
    (19, 12, "Shortbow"),
    (19, 13, "Longbow"),
    # (19, 14, "Dragon-horn Bow"),  # NO_SMITHING
    
    # Arrows (tval 17)
    (17, 1, "Arrow"),
    
    # Light sources (tval 39) - Torch (sval 0) has NO_SMITHING flag
    (39, 1, "Brass Lantern"),
    (39, 2, "Lesser Jewel"),
    (39, 8, "Feanorian Lamp"),
    
    # Amulets (tval 40)
    (40, 0, "Last Chances"),
    (40, 1, "Constitution"),
    (40, 2, "Grace"),
    (40, 3, "Regeneration"),
    (40, 4, "Preservation"),
    (40, 5, "the Blessed Realm"),
    (40, 6, "Haunted Dreams"),
    (40, 7, "the Vigilant Eye"),
    
    # Rings (tval 45)
    (45, 0, "Secrets"),
    (45, 1, "Ered Luin"),
    (45, 2, "Evasion"),
    (45, 3, "Protection"),
    (45, 4, "Strength"),
    (45, 5, "Dexterity"),
    (45, 6, "Frost"),
    (45, 7, "Warmth"),
    (45, 8, "Accuracy"),
    (45, 9, "Free Action"),
]


def parse_full_special_file(filepath):
    """Parse special.txt and return list of special item data with full T: line info."""
    specials = []
    current = None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            if not line or line.startswith('#'):
                continue
            
            if line.startswith('N:'):
                if current:
                    specials.append(current)
                parts = line[2:].split(':')
                idx = int(parts[0])
                name = ':'.join(parts[1:])
                current = {
                    'idx': idx,
                    'name': name,
                    'tval_ranges': [],  # List of (tval, min_sval, max_sval)
                    'max_att': 0,
                    'to_dd': 0,
                    'to_ds': 0,
                    'max_evn': 0,
                    'to_pd': 0,
                    'to_ps': 0,
                    'max_pval': 0,
                    'abilities': 0,
                    'flags': [],
                }
            
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0])
            
            elif line.startswith('C:') and current:
                parts = line[2:].split(':')
                current['max_att'] = int(parts[0]) if parts[0] else 0
                current['to_dd'] = int(parts[1]) if parts[1] else 0
                current['to_ds'] = int(parts[2]) if parts[2] else 0
                current['max_evn'] = int(parts[3]) if parts[3] else 0
                current['to_pd'] = int(parts[4]) if parts[4] else 0
                current['to_ps'] = int(parts[5]) if parts[5] else 0
                if len(parts) > 6:
                    current['max_pval'] = int(parts[6]) if parts[6] else 0
            
            elif line.startswith('T:') and current:
                parts = line[2:].split(':')
                tval = int(parts[0])
                min_sval = int(parts[1])
                max_sval = int(parts[2]) if len(parts) > 2 else min_sval
                current['tval_ranges'].append((tval, min_sval, max_sval))
            
            elif line.startswith('B:') and current:
                abilities_str = line[2:]
                ability_pairs = abilities_str.split(':')
                valid_abilities = [a for a in ability_pairs if '/' in a]
                current['abilities'] += len(valid_abilities)
            
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)
    
    if current:
        specials.append(current)
    
    return specials


def special_applies_to_item(special, tval, sval):
    """Check if a special can apply to a given base item."""
    for t_tval, min_sval, max_sval in special['tval_ranges']:
        if t_tval == tval and min_sval <= sval <= max_sval:
            return True
    return False


def get_smithing_limits(tval, sval, is_special=False, special=None):
    """
    Get the smithing limits for a base item.
    Returns dict of (min, max) tuples for each stat.
    """
    base_att = get_base_att(tval, sval)
    base_evn = get_base_evn(tval, sval)
    base_ds = get_base_ds(tval, sval)
    base_ps = get_base_ps(tval, sval)
    base_pval = get_base_pval(tval, sval)
    
    limits = {}
    
    # Attack bonus limits
    if tval == 17:  # Arrow
        att_min = base_att
        att_max = 3 if not is_special else 0  # Arrows can't have attack bonus with specials
        if is_special and special and special['max_att'] > 0:
            att_min = base_att + 1
            att_max = base_att + special['max_att']
    elif tval in [19, 23, 22, 21, 20]:  # Weapons
        att_min = base_att
        att_max = base_att + 1
        if is_special and special:
            if special['max_att'] > 0:
                att_min = base_att + 1
                att_max = base_att + 1 + special['max_att']
    elif tval == 45 and sval == 8:  # Ring of Accuracy (SV_RING_ACCURACY)
        att_min = base_att
        att_max = 4
    else:  # Armor and other jewelry
        att_min = base_att
        att_max = min(0, base_att + 1)  # Armor att max is capped at 0
        if is_special and special and special['max_att'] > 0:
            att_min = base_att + 1
            att_max = base_att + special['max_att']
    
    limits['att'] = (att_min, att_max)
    
    # Damage sides limits (weapons only)
    if tval in [19, 23, 22, 21, 20]:
        ds_min = base_ds
        ds_max = base_ds + 1
        if is_special and special and special['to_ds'] > 0:
            ds_min = base_ds + 1
            ds_max = base_ds + 1 + special['to_ds']
        limits['ds'] = (ds_min, ds_max)
    else:
        limits['ds'] = (0, 0)
    
    # Evasion limits (armor only, plus Ring of Evasion)
    if tval in [30, 31, 32, 33, 34, 35, 36, 37]:
        evn_min = base_evn
        evn_max = base_evn + 1
        if is_special and special and special['max_evn'] > 0:
            evn_min = base_evn + 1
            evn_max = base_evn + 1 + special['max_evn']
        limits['evn'] = (evn_min, evn_max)
    elif tval == 45 and sval == 2:  # Ring of Evasion (SV_RING_EVASION)
        evn_min = base_evn
        evn_max = 4
        limits['evn'] = (evn_min, evn_max)
    else:
        limits['evn'] = (base_evn, base_evn)
    
    # Protection sides limits (armor only, plus Ring of Protection)
    if tval in [30, 31, 32, 33, 34, 36, 37]:
        base_ps_val = get_base_ps(tval, sval)
        # Cloaks and robes can't get extra protection
        if tval == 35 or (tval == 36 and sval == 2):
            ps_min = base_ps_val
            ps_max = base_ps_val
        elif tval == 37 and sval == 6:  # Hauberk gets +1 extra
            ps_min = base_ps_val
            ps_max = base_ps_val + 2
        else:
            ps_min = base_ps_val
            ps_max = base_ps_val + 1
        
        if is_special and special and special['to_ps'] > 0:
            ps_min = base_ps_val + 1
            ps_max = ps_max + special['to_ps']
        limits['ps'] = (ps_min, ps_max)
    elif tval == 45 and sval == 3:  # Ring of Protection (SV_RING_PROTECTION)
        ps_min = 1
        ps_max = 3
        limits['ps'] = (ps_min, ps_max)
    elif tval == 35:  # Cloak has no protection
        limits['ps'] = (0, 0)
    else:
        limits['ps'] = (0, 0)
    
    # Pval limits
    if tval == 45 or tval == 40:  # Rings and Amulets can have pval up to 4
        pval_min = base_pval
        pval_max = 4
        limits['pval'] = (pval_min, pval_max)
    elif is_special and special and special['max_pval'] > 0:
        pval_min = base_pval
        pval_max = base_pval + special['max_pval']
        limits['pval'] = (pval_min, pval_max)
    else:
        limits['pval'] = (base_pval, base_pval)
    
    return limits


def get_base_ps(tval, sval):
    """Get base protection sides for armor types."""
    base_ps = {
        (30, 1): 1,    # Boots: 1d1
        (30, 2): 2,    # Greaves: 1d2
        (30, 3): 2,    # Mithril Greaves: 1d2
        (31, 1): 0,    # Gloves: 1d0
        (31, 2): 1,    # Gauntlets: 1d1
        (31, 3): 1,    # Mithril Gauntlets: 1d1
        (32, 5): 2,    # Helm: 1d2
        (32, 6): 3,    # Great Helm: 1d3
        (32, 7): 2,    # Dwarf Mask: 1d2
        (32, 8): 3,    # Mithril Helm: 1d3
        (33, 11): 0,   # Crown: 1d0
        (34, 3): 3,    # Round Shield: 1d3
        (34, 5): 6,    # Kite Shield: 1d6
        (34, 10): 6,   # Mithril Shield: 1d6
        (35, 1): 0,    # Cloak: 1d0
        (35, 6): 0,    # Shadow Cloak: 1d0
        (36, 2): 0,    # Robe: 1d0
        (36, 4): 4,    # Leather: 1d4
        (36, 7): 6,    # Studded Leather: 1d6
        (36, 11): 8,   # Galvorn: 1d8
        (37, 4): 4,    # Mail Corslet: 2d4
        (37, 6): 5,    # Hauberk: 2d5
        (37, 20): 4,   # Mithril Corslet: 2d4
    }
    return base_ps.get((tval, sval), 0)


def get_base_pd(tval, sval):
    """Get base protection dice for armor types."""
    base_pd = {
        (30, 1): 1,    # Boots
        (30, 2): 1,    # Greaves
        (30, 3): 1,    # Mithril Greaves
        (31, 1): 1,    # Gloves
        (31, 2): 1,    # Gauntlets
        (31, 3): 1,    # Mithril Gauntlets
        (32, 5): 1,    # Helm
        (32, 6): 1,    # Great Helm
        (32, 7): 1,    # Dwarf Mask
        (32, 8): 1,    # Mithril Helm
        (33, 11): 1,   # Crown
        (34, 3): 1,    # Round Shield
        (34, 5): 1,    # Kite Shield
        (34, 10): 1,   # Mithril Shield
        (35, 1): 1,    # Cloak
        (35, 6): 1,    # Shadow Cloak
        (36, 2): 1,    # Robe
        (36, 4): 1,    # Leather
        (36, 7): 1,    # Studded Leather
        (36, 11): 1,   # Galvorn
        (37, 4): 2,    # Mail Corslet
        (37, 6): 2,    # Hauberk
        (37, 20): 2,   # Mithril Corslet
    }
    return base_pd.get((tval, sval), 0)


def calculate_item_difficulty(tval, sval, att_bonus=0, ds_bonus=0, evn_bonus=0, 
                               ps_bonus=0, pval_bonus=0, special=None):
    """
    Calculate smithing difficulty for a specific item configuration.
    """
    dif_inc = 0
    dif_mult = 100
    brands = 0
    
    base_level = get_base_level(tval, sval)
    base_att = get_base_att(tval, sval)
    base_evn = get_base_evn(tval, sval)
    base_ds = get_base_ds(tval, sval)
    base_ps = get_base_ps(tval, sval)
    base_pd = get_base_pd(tval, sval)
    base_pval = get_base_pval(tval, sval)
    
    # Base item level contribution
    # Jewelry (TV_LIGHT=39, TV_AMULET=40, TV_RING=45) does NOT add base level
    if tval not in [39, 40, 45]:  # Not light, amulet, or ring
        dif_inc += base_level // 2
    
    # For jewelry, get base flags and add them to the flag set
    flags = set()
    base_abilities = 0
    if tval in [40, 45]:  # Amulets and Rings
        base_jewelry_flags = get_base_flags_for_jewelry(tval, sval)
        flags.update(base_jewelry_flags)
        base_abilities = get_base_abilities_for_jewelry(tval, sval)
    
    # Add special flags if present
    if special:
        flags.update(special['flags'])
        base_abilities += special.get('abilities', 0)
    

    # Attack bonus contribution
    if att_bonus > 0:
        if tval == 17:  # Arrow
            dif_inc += dif_mod_calc(att_bonus, 5) // 2
        elif tval in [19, 23, 22, 21, 20]:  # Weapons
            dif_inc += dif_mod_calc(att_bonus, 3)
        else:
            val = dif_mod_calc(att_bonus, 6)
            if val > 0:
                val -= 1
            dif_inc += val
    
    # Evasion bonus
    if evn_bonus != 0:
        val = dif_mod_calc(abs(evn_bonus), 6)
        if evn_bonus > 0:
            val -= 1
        dif_inc += val
    
    # Damage sides bonus
    if ds_bonus > 0:
        dif_inc += dif_mod_calc(ds_bonus, 3 * ds_bonus + 2)
    
    # Protection bonus
    new_prot = ((base_ps + ps_bonus + 1) * base_pd) if (base_ps + ps_bonus) > 0 else 0
    base_prot = ((base_ps + 1) * base_pd) if base_ps > 0 else 0
    prot_bonus = new_prot - base_prot
    
    if prot_bonus > 0:
        if tval == 37 and sval == 6:  # Hauberk
            dif_inc += dif_mod_calc(prot_bonus, 1) + 2
        elif tval == 45:  # Ring
            dif_inc += dif_mod_calc(prot_bonus, 1) + 4
        else:
            dif_inc += dif_mod_calc(prot_bonus, 3)
    
    # Pval-dependent flags (use total pval including base)
    total_pval = base_pval + pval_bonus
    
    if total_pval > 0:
        if 'TUNNEL' in flags:
            dif_inc += dif_mod_calc(pval_bonus, 8)
        if 'DAMAGE_SIDES' in flags:
            dif_inc += dif_mod_calc(total_pval, 18)
        if 'STR' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'DEX' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'CON' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'GRA' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'ARCHERY' in flags or 'ARC' in flags:
            dif_inc += dif_mod_calc(total_pval, 4)
        if 'STEALTH' in flags or 'STL' in flags:
            dif_inc += dif_mod_calc(total_pval, 4)
        if 'PERCEPTION' in flags or 'PER' in flags:
            dif_inc += dif_mod_calc(total_pval, 3)
        if 'WILL' in flags or 'WIL' in flags:
            dif_inc += dif_mod_calc(total_pval, 3)
        if 'SONG' in flags or 'SNG' in flags:
            dif_inc += dif_mod_calc(total_pval, 4)
    
    # Sustains
    if 'SUST_STR' in flags:
        dif_inc += 2
    if 'SUST_DEX' in flags:
        dif_inc += 2
    if 'SUST_CON' in flags:
        dif_inc += 2
    if 'SUST_GRA' in flags:
        dif_inc += 2
    
    # Abilities/misc
    if 'SLOW_DIGEST' in flags:
        dif_inc += 2
    if 'RADIANCE' in flags:
        dif_inc += 6
    if 'LIGHT' in flags:
        dif_inc += 8
    if 'REGEN' in flags:
        dif_inc += 4
    if 'SEE_INVIS' in flags:
        dif_inc += 4
    if 'FREE_ACT' in flags:
        dif_inc += 7
    if 'SPEED' in flags:
        dif_inc += 40
    if 'CHEAT_DEATH' in flags:
        dif_inc += 13
    if 'STAND_FAST' in flags:
        dif_inc += 2
    if 'AVOID_TRAPS' in flags:
        dif_inc += 6
    if 'MEDIC' in flags:
        dif_inc += 4
    
    # Resistances
    if 'RES_COLD' in flags:
        dif_inc += 5
    if 'RES_FIRE' in flags:
        dif_inc += 5
    if 'RES_POIS' in flags:
        dif_inc += 5
    if 'RES_BLEED' in flags:
        dif_inc += 1
    if 'RES_BLIND' in flags:
        dif_inc += 2
    if 'RES_CONFU' in flags:
        dif_inc += 2
    if 'RES_STUN' in flags:
        dif_inc += 2
    if 'RES_FEAR' in flags:
        dif_inc += 2
    if 'RES_HALLU' in flags:
        dif_inc += 1
    
    # Abilities from B: lines and base abilities
    for _ in range(base_abilities):
        avg_ability_level = 5
        dif_inc += 5 + (avg_ability_level // 3)
    
    # Slays
    if 'SLAY_ORC' in flags:
        dif_inc += 3
    if 'SLAY_TROLL' in flags:
        dif_inc += 3
    if 'SLAY_WOLF' in flags:
        dif_inc += 3
    if 'SLAY_SPIDER' in flags:
        dif_inc += 4
    if 'SLAY_UNDEAD' in flags:
        dif_inc += 3
    if 'SLAY_RAUKO' in flags:
        dif_inc += 4
    if 'SLAY_DRAGON' in flags:
        dif_inc += 4
    if 'SLAY_MAN_OR_ELF' in flags:
        dif_inc += 5
    
    # Brands
    if 'BRAND_COLD' in flags:
        dif_inc += 18
        brands += 1
    if 'BRAND_FIRE' in flags:
        dif_inc += 14
        brands += 1
    if 'BRAND_POIS' in flags:
        if tval == 17:
            dif_inc += 12
        else:
            dif_inc += 16
            brands += 1
    
    if brands > 1:
        dif_inc += (brands - 1) * 20
    
    # Sharpness
    if 'SHARPNESS' in flags:
        dif_inc += 14 if tval == 17 else 24
    if 'SHARPNESS2' in flags:
        dif_inc += 40
    
    # Other weapon flags
    if 'VAMPIRIC' in flags:
        dif_inc += 6
    if 'ACCURATE' in flags:
        dif_inc += 15
    
    # Calculate base difficulty
    dif = dif_inc
    
    # Slot multiplier
    slot = get_slot(tval)
    if slot in ['ring', 'light', 'cloak', 'gloves', 'boots', 'arrow']:
        dif_mult += 20
    
    # Apply multiplier
    dif = dif * dif_mult // 100

    # Arrow difficulty halved
    if tval == 17:
        dif = dif // 2

    return dif


def get_item_category(tval):
    """Determine item category from tval."""
    if tval in [17, 19, 23, 22, 21, 20]:  # Arrow, Bow, Sword, Polearm, Hafted, Digging
        return 'Weapons'
    elif tval in [30, 31, 32, 33, 34, 35, 36, 37]:  # Boots, Gloves, Helm, Crown, Shield, Cloak, Armor, Mail
        return 'Armor'
    elif tval in [40, 45]:  # Light, Ring, Amulet
        return 'Jewelry'
    elif tval in [39]:  # Light sources
        return 'Jewelry'
    else:
        return 'Other'


def enumerate_item_variants(tval, sval, name, specials):
    """
    Enumerate all possible smithable variants of a base item.
    Returns list of (difficulty, description) tuples.
    """
    variants = []


def enumerate_item_variants(tval, sval, name, specials):
    """
    Enumerate all possible smithable variants of a base item.
    Returns list of (difficulty, description, category) tuples.
    """
    variants = []
    category = get_item_category(tval)
    
    # Get base limits for plain item
    limits = get_smithing_limits(tval, sval, is_special=False, special=None)
    
    # Generate all plain item variants
    att_range = range(limits['att'][0], limits['att'][1] + 1)
    ds_range = range(limits['ds'][0], limits['ds'][1] + 1)
    evn_range = range(limits['evn'][0], limits['evn'][1] + 1)
    ps_range = range(limits['ps'][0], limits['ps'][1] + 1)
    pval_range = range(limits['pval'][0], limits['pval'][1] + 1)
    
    base_att = get_base_att(tval, sval)
    base_ds = get_base_ds(tval, sval)
    base_evn = get_base_evn(tval, sval)
    base_ps = get_base_ps(tval, sval)
    base_pval = get_base_pval(tval, sval)
    
    # Plain item variants
    for att in att_range:
        for ds in ds_range:
            for evn in evn_range:
                for ps in ps_range:
                    for pval in pval_range:
                        att_bonus = att - base_att
                        ds_bonus = ds - base_ds
                        evn_bonus = evn - base_evn
                        ps_bonus = ps - base_ps
                        pval_bonus = pval - base_pval
                        
                        dif = calculate_item_difficulty(
                            tval, sval, att_bonus, ds_bonus, evn_bonus, ps_bonus, pval_bonus, None
                        )
                        
                        desc = f"{name} (+{att},{ds}ds,[{evn}],{ps}ps,pval:{pval})"
                        variants.append((dif, desc, category, 'base'))
    
    # Generate special item variants
    for special in specials:
        if not special_applies_to_item(special, tval, sval):
            continue
        
        # Skip cursed specials for counting purposes (they're still craftable but usually not wanted)
        cursed_flags = {'AGGRAVATE', 'DANGER', 'HUNGER', 'LIGHT_CURSE', 'HEAVY_CURSE',
                        'VUL_POIS', 'VUL_COLD', 'VUL_FIRE', 'FEAR', 'HAUNTED', 'CUMBERSOME'}
        is_cursed = bool(set(special['flags']) & cursed_flags)
        
        limits = get_smithing_limits(tval, sval, is_special=True, special=special)
        
        att_range = range(limits['att'][0], limits['att'][1] + 1)
        ds_range = range(limits['ds'][0], limits['ds'][1] + 1)
        evn_range = range(limits['evn'][0], limits['evn'][1] + 1)
        ps_range = range(limits['ps'][0], limits['ps'][1] + 1)
        pval_range = range(limits['pval'][0], limits['pval'][1] + 1)
        
        for att in att_range:
            for ds in ds_range:
                for evn in evn_range:
                    for ps in ps_range:
                        for pval in pval_range:
                            att_bonus = att - base_att
                            ds_bonus = ds - base_ds
                            evn_bonus = evn - base_evn
                            ps_bonus = ps - base_ps
                            pval_bonus = pval - get_base_pval(tval, sval)
                            
                            dif = calculate_item_difficulty(
                                tval, sval, att_bonus, ds_bonus, evn_bonus, 
                                ps_bonus, pval_bonus, special
                            )
                            
                            item_type = 'cursed_special' if is_cursed else 'special'
                            desc = f"{name} {special['name']} (+{att},{ds}ds,[{evn}],{ps}ps,pval:{pval})"
                            variants.append((dif, desc, category, item_type))
    
    return variants


def main():
    # Find data files
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        script_dir = os.getcwd()
    
    artefact_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'artefact.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\artefact.txt',
    ]
    special_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'special.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\special.txt',
    ]
    
    artefact_file = None
    for path in artefact_paths:
        if os.path.exists(path):
            artefact_file = path
            break
    
    special_file = None
    for path in special_paths:
        if os.path.exists(path):
            special_file = path
            break
    
    if not artefact_file:
        raise FileNotFoundError("Could not find artefact.txt")
    if not special_file:
        raise FileNotFoundError("Could not find special.txt")
    
    # Parse files
    artefacts = parse_artefact_file(artefact_file)
    specials = parse_full_special_file(special_file)
    
    # Filter out template artefacts
    artefacts = [a for a in artefacts if not (a['name'].startswith("'Ultimate") and a['idx'] >= 182)]
    
    # Calculate artefact difficulties
    from calc_artefact_difficulty import calculate_difficulty
    for art in artefacts:
        art['type'] = 'artefact'
        art['difficulty'] = calculate_difficulty(art)
    
    # Collect difficulty distribution
    difficulty_counts = defaultdict(lambda: {
        'artefacts': 0, 
        'base_items': 0, 
        'special_items': 0,
        'cursed_specials': 0,
        'artefact_names': [],
        'sample_items': [],
        'by_category': defaultdict(lambda: {
            'artefacts': 0,
            'base_items': 0,
            'special_items': 0,
            'cursed_specials': 0,
            'sample_items': []
        })
    })
    
    # Count artefacts
    for art in artefacts:
        dif = art['difficulty']
        difficulty_counts[dif]['artefacts'] += 1
        difficulty_counts[dif]['artefact_names'].append(art['name'])
    
    # Count craftable items
    total_base = 0
    total_special = 0
    total_cursed = 0
    category_totals = defaultdict(lambda: {'base': 0, 'special': 0, 'cursed': 0})
    
    for tval, sval, name in SMITHABLE_BASE_ITEMS:
        variants = enumerate_item_variants(tval, sval, name, specials)
        for dif, desc, category, item_type in variants:
            if item_type == 'base':
                difficulty_counts[dif]['base_items'] += 1
                difficulty_counts[dif]['by_category'][category]['base_items'] += 1
                total_base += 1
                category_totals[category]['base'] += 1
            elif item_type == 'special':
                difficulty_counts[dif]['special_items'] += 1
                difficulty_counts[dif]['by_category'][category]['special_items'] += 1
                total_special += 1
                category_totals[category]['special'] += 1
            else:  # cursed_special
                difficulty_counts[dif]['cursed_specials'] += 1
                difficulty_counts[dif]['by_category'][category]['cursed_specials'] += 1
                total_cursed += 1
                category_totals[category]['cursed'] += 1
            
            # Store a few samples
            if len(difficulty_counts[dif]['sample_items']) < 3:
                difficulty_counts[dif]['sample_items'].append(desc)
            if len(difficulty_counts[dif]['by_category'][category]['sample_items']) < 2:
                difficulty_counts[dif]['by_category'][category]['sample_items'].append(desc)
    
    print()
    print("=" * 100)
    print("BY DIFFICULTY (All Categories):")
    print("=" * 100)
    
    # Print by difficulty
    print(f"{'Diff':>4} {'Artefacts':>10} {'Base':>8} {'Special':>10} {'Cursed':>8} {'Total':>8}  Samples/Artefacts")
    print("-" * 100)
    
    for dif in sorted(difficulty_counts.keys()):
        data = difficulty_counts[dif]
        total = data['base_items'] + data['special_items'] + data['cursed_specials']
        
        samples = data['sample_items'][:2]
        arts = data['artefact_names'][:2]
        
        extra = ""
        if arts:
            extra = f"Arts: {', '.join(arts[:2])}"
        if samples:
            if extra:
                extra += " | "
            extra += f"Items: {samples[0][:40]}"
        
        print(f"{dif:>4} {data['artefacts']:>10} {data['base_items']:>8} {data['special_items']:>10} {data['cursed_specials']:>8} {total:>8}  {extra[:50]}")
    
    print()
    print("=" * 100)
    print("BY CATEGORY:")
    print("=" * 100)
    print()
    
    # Print by category
    for category in ['Weapons', 'Armor', 'Jewelry']:
        if category not in category_totals:
            continue
        
        print(f"\n{category.upper()}")
        print("-" * 100)
        print(f"{'Diff':>4} {'Artefacts':>10} {'Base':>8} {'Special':>10} {'Cursed':>8} {'Total':>8}")
        print("-" * 100)
        
        for dif in sorted(difficulty_counts.keys()):
            cat_data = difficulty_counts[dif]['by_category'][category]
            total = cat_data['base_items'] + cat_data['special_items'] + cat_data['cursed_specials']
            
            if total + cat_data['artefacts'] == 0:
                continue
            
            print(f"{dif:>4} {cat_data['artefacts']:>10} {cat_data['base_items']:>8} {cat_data['special_items']:>10} {cat_data['cursed_specials']:>8} {total:>8}")
    
    print()
    print("=" * 100)
    print("CATEGORY TOTALS:")
    print("-" * 60)
    
    for category in ['Weapons', 'Armor', 'Jewelry']:
        if category not in category_totals:
            continue
        totals = category_totals[category]
        grand_total = totals['base'] + totals['special'] + totals['cursed']
        print(f"{category:<15}: {totals['base']:>5} base + {totals['special']:>5} special + {totals['cursed']:>5} cursed = {grand_total:>5} total")
    
    print()
    
    # Summary by tiers
    print("\nDIFFICULTY TIER SUMMARY (All Items):")
    print("-" * 60)
    tiers = [
        (0, 4, "Trivial (0-4)"),
        (5, 9, "Very Easy (5-9)"),
        (10, 14, "Easy (10-14)"),
        (15, 19, "Moderate (15-19)"),
        (20, 24, "Challenging (20-24)"),
        (25, 34, "Hard (25-34)"),
        (35, 44, "Very Hard (35-44)"),
        (45, 59, "Epic (45-59)"),
        (60, 999, "Legendary (60+)"),
    ]
    
    for low, high, label in tiers:
        arts = sum(difficulty_counts[d]['artefacts'] for d in difficulty_counts if low <= d <= high)
        base = sum(difficulty_counts[d]['base_items'] for d in difficulty_counts if low <= d <= high)
        special = sum(difficulty_counts[d]['special_items'] for d in difficulty_counts if low <= d <= high)
        cursed = sum(difficulty_counts[d]['cursed_specials'] for d in difficulty_counts if low <= d <= high)
        total = base + special + cursed
        print(f"  {label:<25}: {arts:>3} artefacts, {total:>5} craftable ({base} base + {special} special + {cursed} cursed)")
    
    print()
    
    # Summary by category and tier
    print("\nDIFFICULTY TIER SUMMARY (By Category):")
    print("-" * 100)
    print()
    
    for category in ['Weapons', 'Armor', 'Jewelry']:
        if category not in category_totals:
            continue
        
        print(f"{category.upper()}:")
        for low, high, label in tiers:
            arts = sum(difficulty_counts[d]['by_category'][category]['artefacts'] for d in difficulty_counts if low <= d <= high)
            base = sum(difficulty_counts[d]['by_category'][category]['base_items'] for d in difficulty_counts if low <= d <= high)
            special = sum(difficulty_counts[d]['by_category'][category]['special_items'] for d in difficulty_counts if low <= d <= high)
            cursed = sum(difficulty_counts[d]['by_category'][category]['cursed_specials'] for d in difficulty_counts if low <= d <= high)
            total = base + special + cursed
            if total + arts > 0:
                print(f"  {label:<25}: {arts:>3} artefacts, {total:>5} craftable ({base} base + {special} special + {cursed} cursed)")
        print()
    
    # Export to CSV
    import csv
    csv_file = os.path.join(script_dir, 'difficulty_distribution.csv')
    with open(csv_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Write overall summary
        writer.writerow(['OVERALL SUMMARY'])
        writer.writerow(['Difficulty', 'Artefacts', 'Base Items', 'Special Items', 'Cursed Specials', 'Total Craftable'])
        for dif in sorted(difficulty_counts.keys()):
            data = difficulty_counts[dif]
            total = data['base_items'] + data['special_items'] + data['cursed_specials']
            writer.writerow([dif, data['artefacts'], data['base_items'], data['special_items'], data['cursed_specials'], total])
        
        # Write category breakdowns
        for category in ['Weapons', 'Armor', 'Jewelry']:
            writer.writerow([])
            writer.writerow([f'{category.upper()} BREAKDOWN'])
            writer.writerow(['Difficulty', 'Artefacts', 'Base Items', 'Special Items', 'Cursed Specials', 'Total Craftable'])
            
            for dif in sorted(difficulty_counts.keys()):
                cat_data = difficulty_counts[dif]['by_category'][category]
                total = cat_data['base_items'] + cat_data['special_items'] + cat_data['cursed_specials']
                writer.writerow([dif, cat_data['artefacts'], cat_data['base_items'], cat_data['special_items'], cat_data['cursed_specials'], total])
        
        # Write category totals summary
        writer.writerow([])
        writer.writerow(['CATEGORY TOTALS'])
        writer.writerow(['Category', 'Base Items', 'Special Items', 'Cursed Specials', 'Total Craftable'])
        for category in ['Weapons', 'Armor', 'Jewelry']:
            totals = category_totals[category]
            grand_total = totals['base'] + totals['special'] + totals['cursed']
            writer.writerow([category, totals['base'], totals['special'], totals['cursed'], grand_total])
    
    print(f"Results exported to: {csv_file}")
    

if __name__ == '__main__':
    main()
