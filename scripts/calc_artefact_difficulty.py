#!/usr/bin/env python3
"""
Calculate smithing difficulty for each artefact in artefact.txt and special.txt

Based on the REAL object_difficulty() function in src/cmd4.c (lines 3980+)
which includes proper costs for flags like slays, brands, sharpness, etc.
"""

import re
import os
import sys


def parse_artefact_file(filepath):
    """Parse artefact.txt and return list of artefact data."""
    artefacts = []
    current = None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # New artefact entry
            if line.startswith('N:'):
                if current:
                    artefacts.append(current)
                parts = line[2:].split(':')
                idx = int(parts[0])
                name = ':'.join(parts[1:])
                current = {
                    'idx': idx,
                    'name': name,
                    'tval': 0,
                    'sval': 0,
                    'pval': 0,
                    'att': 0,
                    'evn': 0,
                    'dd': 0,
                    'ds': 0,
                    'pd': 0,
                    'ps': 0,
                    'abilities': 0,
                    'flags': [],
                    'depth': 0,
                    'rarity': 0,
                }
            
            # Item type info
            elif line.startswith('I:') and current:
                parts = line[2:].split(':')
                current['tval'] = int(parts[0])
                current['sval'] = int(parts[1])
                if len(parts) > 2:
                    current['pval'] = int(parts[2])
            
            # Depth/rarity info
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0])
                current['rarity'] = int(parts[1])
            
            # Combat stats: att:dam_dice:evn:prot_dice
            elif line.startswith('P:') and current:
                parts = line[2:].split(':')
                # Attack bonus
                att_str = parts[0].replace('+', '')
                current['att'] = int(att_str) if att_str else 0
                
                # Damage dice (XdY)
                if len(parts) > 1 and 'd' in parts[1]:
                    dam = parts[1].split('d')
                    current['dd'] = int(dam[0])
                    current['ds'] = int(dam[1])
                
                # Evasion bonus
                if len(parts) > 2:
                    evn_str = parts[2].replace('+', '')
                    current['evn'] = int(evn_str) if evn_str else 0
                
                # Protection dice (XdY)
                if len(parts) > 3 and 'd' in parts[3]:
                    # Handle cases like "1d1:0" where there might be extra parts
                    prot_part = parts[3].split(':')[0]
                    prot = prot_part.split('d')
                    current['pd'] = int(prot[0])
                    current['ps'] = int(prot[1])
            
            # Abilities (B: line format: B:skilltype/abilitynum:skilltype/abilitynum:...)
            elif line.startswith('B:') and current:
                abilities_str = line[2:]
                # B: lines should only contain ability pairs like 7/3 or 2/6:4/2
                # Note: If flags appear here (like B:STR | RES_FEAR), that's a bug in artefact.txt
                ability_pairs = abilities_str.split(':')
                # Filter out empty strings and count valid ability pairs
                valid_abilities = [a for a in ability_pairs if '/' in a]
                current['abilities'] += len(valid_abilities)
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)
    
    # Don't forget the last artefact
    if current:
        artefacts.append(current)
    
    return artefacts


def parse_special_file(filepath):
    """Parse special.txt and return list of special item data."""
    specials = []
    current = None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # New special entry
            if line.startswith('N:'):
                if current:
                    specials.append(current)
                parts = line[2:].split(':')
                idx = int(parts[0])
                name = ':'.join(parts[1:])
                current = {
                    'idx': idx,
                    'name': name,
                    'tvals': [],  # Can have multiple tval ranges
                    'min_svals': [],
                    'max_svals': [],
                    'max_pval': 0,
                    'max_att': 0,
                    'max_evn': 0,
                    'to_dd': 0,
                    'to_ds': 0,
                    'to_pd': 0,
                    'to_ps': 0,
                    'abilities': 0,
                    'flags': [],
                    'depth': 0,
                    'rarity': 0,
                }
            
            # Depth/rarity info
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0])
                current['rarity'] = int(parts[1])
            
            # Creation bonuses: C:max_att:+dd:+ds:max_evn:+pd:+ps:max_pval
            # These are MAXIMUM bonuses that get ADDED to the base item
            # When smithing, you get +1 to each stat if its max > 0
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
            
            # Tval/sval range - can have multiple T: lines
            elif line.startswith('T:') and current:
                parts = line[2:].split(':')
                current['tvals'].append(int(parts[0]))
                current['min_svals'].append(int(parts[1]))
                current['max_svals'].append(int(parts[2]) if len(parts) > 2 else int(parts[1]))
            
            # Abilities
            elif line.startswith('B:') and current:
                abilities_str = line[2:]
                ability_pairs = abilities_str.split(':')
                valid_abilities = [a for a in ability_pairs if '/' in a]
                current['abilities'] += len(valid_abilities)
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)
    
    # Don't forget the last special
    if current:
        specials.append(current)
    
    return specials


def calculate_difficulty(art):
    """
    Calculate smithing difficulty using the REAL formula from src/cmd4.c object_difficulty().
    This accounts for all flags, slays, brands, sharpness, etc.
    
    For normal items: Use the actual stats from the generated variant.
    For specials: Use the actual stats from the generated variant.
    For artefacts: Use the stats as-is from artefact.txt.
    """
    dif_inc = 0
    dif_dec = 0
    dif_mult = 100  # Percentage multiplier
    brands = 0
    
    flags = set(art['flags'])
    tval = art['tval']
    sval = art['sval']
    is_special = (art['type'] == 'special')
    is_normal = (art['type'] == 'normal')
    
    # Get base item stats
    if is_special or is_normal:
        # For variants, we have base stats stored
        base_att = art.get('base_att', 0)
        base_evn = art.get('base_evn', 0)
        base_ds = art.get('base_ds', 0)
        base_ps = art.get('base_ps', 0)
        base_pd = art.get('base_pd', 0)
        base_dd = art.get('base_dd', 0)
        base_pval = art.get('base_pval', 0)
        base_level = art.get('base_level', 0)
        base_prot = (base_ps + 1) * base_pd if base_ps > 0 else 0
    else:
        # For artefacts, look up base stats
        base_att = get_base_att(tval, sval)
        base_evn = get_base_evn(tval, sval)
        base_ds = get_base_ds(tval, sval)
        base_pval = get_base_pval(tval, sval)
        base_level = get_base_level(tval, sval)
        base_prot = get_base_protection(tval, sval)
    
    # Calculate bonuses (difference from base)
    smithed_att_bonus = art['att'] - base_att
    smithed_evn_bonus = art['evn'] - base_evn
    smithed_ds_bonus = art['ds'] - base_ds
    smithed_pval_bonus = art['pval'] - base_pval
    total_pval = art['pval']
    
    # For protection, calculate new_prot from variant's pd/ps
    new_prot = (art['ps'] + 1) * art['pd'] if art['ps'] > 0 else 0
    prot_bonus = new_prot - base_prot
    # For protection, calculate new_prot from variant's pd/ps
    new_prot = (art['ps'] + 1) * art['pd'] if art['ps'] > 0 else 0
    prot_bonus = new_prot - base_prot
    
    # Base item level contribution (k_ptr->level / 2)
    # For non-jewelry items: dif_inc += k_ptr->level / 2
    if tval not in [45, 40]:  # Not ring or amulet
        dif_inc += base_level // 2
    
    # Attack bonus contribution (bonus above base)
    att_bonus = smithed_att_bonus
    if att_bonus > 0:
        if tval == 17:  # Arrow - different formula
            dif_inc += dif_mod_calc(att_bonus, 5) // 2
        elif tval in [19, 23, 22, 21, 20]:  # Bow, Sword, Polearm, Hafted, Digging
            dif_inc += dif_mod_calc(att_bonus, 3)
        else:
            val = dif_mod_calc(att_bonus, 6)
            if val > 0:
                val -= 1
            dif_inc += val
    
    # Evasion bonus (bonus above base): dif_mod(x, 6, &dif_inc); if (x > 0) dif_inc -= 1
    evn_bonus = smithed_evn_bonus
    if evn_bonus != 0:
        val = dif_mod_calc(abs(evn_bonus), 6)
        if evn_bonus > 0:
            val -= 1
        dif_inc += val
    
    # Damage sides bonus: dif_mod(x, 3*x+2, &dif_inc)
    ds_bonus = smithed_ds_bonus
    if ds_bonus > 0:
        # The formula is: dif_mod(x, 3*x+2) which means each ds costs more
        dif_inc += dif_mod_calc(ds_bonus, 3 * ds_bonus + 2)
    
    # Protection bonus
    if prot_bonus > 0:
        if tval == 37 and sval == 6:  # Hauberk
            dif_inc += dif_mod_calc(prot_bonus, 1) + 2
        elif tval == 45:  # Ring
            dif_inc += dif_mod_calc(prot_bonus, 1) + 4
        else:
            dif_inc += dif_mod_calc(prot_bonus, 3)
    
    # === WEAPON MODIFIERS (SLAYS) ===
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
    
    # === BRANDS ===
    if 'BRAND_COLD' in flags:
        dif_inc += 18
        brands += 1
    if 'BRAND_FIRE' in flags:
        dif_inc += 14
        brands += 1
    if 'BRAND_POIS' in flags:
        if art['tval'] == 17:  # Arrow
            dif_inc += 12
        else:
            dif_inc += 16
            brands += 1
    if 'BRAND_ELEC' in flags:  # If exists
        dif_inc += 18
        brands += 1
    
    # Multiple brands penalty
    if brands > 1:
        dif_inc += (brands - 1) * 20
    
    # === SHARPNESS ===
    if 'SHARPNESS' in flags:
        base = 14 if art['tval'] == 17 else 24  # Arrow vs other
        dif_inc += base
    if 'SHARPNESS2' in flags:
        dif_inc += 40
    
    # === OTHER WEAPON FLAGS ===
    if 'VAMPIRIC' in flags:
        dif_inc += 6
    if 'ACCURATE' in flags:
        dif_inc += 15
    
    # === PVAL-DEPENDENT BONUSES ===
    # For specials: total_pval = base_pval + smithed bonus
    # For artefacts: total_pval = art['pval']
    # pval_bonus = total_pval - base_pval (used for TUNNEL)
    pval_bonus = smithed_pval_bonus if smithed_pval_bonus > 0 else 0
    
    # TUNNEL: uses pval_bonus (difference from base)
    # Special case: digging tools already have TUNNEL, so only extra pval counts
    if 'TUNNEL' in flags:
        if pval_bonus > 0:
            dif_inc += dif_mod_calc(pval_bonus, 8)
    
    # For stats and skills, use absolute pval (the total pval of the item)
    x_abs = total_pval if total_pval > 0 else 0
    if x_abs > 0:
        if 'DAMAGE_SIDES' in flags:
            dif_inc += dif_mod_calc(x_abs, 18)
        if 'STR' in flags:
            dif_inc += dif_mod_calc(x_abs, 14)
        if 'DEX' in flags:
            dif_inc += dif_mod_calc(x_abs, 14)
        if 'CON' in flags:
            dif_inc += dif_mod_calc(x_abs, 14)
        if 'GRA' in flags:
            dif_inc += dif_mod_calc(x_abs, 14)
        if 'ARCHERY' in flags or 'ARC' in flags:
            dif_inc += dif_mod_calc(x_abs, 4)
        if 'STEALTH' in flags or 'STL' in flags:
            dif_inc += dif_mod_calc(x_abs, 4)
        if 'PERCEPTION' in flags or 'PER' in flags:
            dif_inc += dif_mod_calc(x_abs, 3)
        if 'WILL' in flags or 'WIL' in flags:
            dif_inc += dif_mod_calc(x_abs, 3)
        if 'SONG' in flags or 'SNG' in flags:
            dif_inc += dif_mod_calc(x_abs, 4)
    
    # Negative pval penalties (reduce difficulty)
    x_neg = -total_pval if total_pval < 0 else 0
    if x_neg > 0:
        if 'NEG_STR' in flags:
            dif_inc += dif_mod_calc(x_neg, 12)
        if 'NEG_DEX' in flags:
            dif_inc += dif_mod_calc(x_neg, 12)
        if 'NEG_CON' in flags:
            dif_inc += dif_mod_calc(x_neg, 12)
        if 'NEG_GRA' in flags:
            dif_inc += dif_mod_calc(x_neg, 12)
    
    # === SUSTAINS ===
    if 'SUST_STR' in flags:
        dif_inc += 2
    if 'SUST_DEX' in flags:
        dif_inc += 2
    if 'SUST_CON' in flags:
        dif_inc += 2
    if 'SUST_GRA' in flags:
        dif_inc += 2
    
    # === ABILITIES/MISC ===
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
    
    # === ELEMENTAL RESISTANCES ===
    if 'RES_COLD' in flags:
        dif_inc += 5
    if 'RES_FIRE' in flags:
        dif_inc += 5
    if 'RES_POIS' in flags:
        dif_inc += 5
    
    # === OTHER RESISTANCES ===
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
    
    # === PENALTY FLAGS (reduce difficulty for non-artefacts, but artefacts don't benefit) ===
    # For artefacts, these penalties are NOT applied per the code:
    # if (!o_ptr->name1) { ... penalty flags ... }
    # So we skip dif_dec for artefacts
    
    # === ABILITIES (granted abilities) ===
    # dif_inc += 5 + (level / 3) per ability
    # We approximate ability level as ~5 on average
    for _ in range(art['abilities']):
        avg_ability_level = 5
        dif_inc += 5 + (avg_ability_level // 3)
    
    # Calculate base difficulty
    dif = dif_inc - dif_dec
    
    # === SLOT MULTIPLIER ===
    # Minor slots get +20% difficulty
    slot = get_slot(art['tval'])
    if slot in ['ring', 'light', 'cloak', 'gloves', 'boots', 'arrow']:
        dif_mult += 20
    
    # Apply multiplier
    dif = dif * dif_mult // 100
    
    # Artefact arrows are easier (halved)
    if art['tval'] == 17:  # Arrow
        dif = dif // 2

    return dif


def dif_mod_calc(value, positive_base):
    """
    Calculates difficulty modification using the triangular number formula.
    From cmd4.c dif_mod():
        int mod = 1 + ((positive_base - 1) / 5);
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2)
    """
    if value <= 0:
        return 0
    mod = 1 + ((positive_base - 1) // 5)
    return positive_base * value + mod * (value * (value - 1) // 2)


def get_base_level(tval, sval):
    """Get base item level for difficulty calculation from object.txt."""
    # These are the k_ptr->level values from object.txt W: lines
    base_levels = {
        # Swords (tval 23)
        (23, 4): 1,    # Dagger
        (23, 7): 2,    # Curved Sword
        (23, 10): 1,   # Shortsword
        (23, 17): 4,   # Longsword
        (23, 21): 6,   # Bastard Sword
        (23, 25): 4,   # Greatsword
        (23, 28): 5,   # Mithril Longsword
        (23, 30): 6,   # Mithril Greatsword
        # Polearms (tval 22)
        (22, 1): 1,    # Spear
        (22, 2): 4,    # Great Spear
        (22, 4): 8,    # Glaive
        (22, 11): 2,   # Hand Axe
        (22, 12): 4,   # Battle Axe
        (22, 13): 8,   # Great Axe
        # Hafted (tval 21)
        (21, 3): 1,    # Quarterstaff
        (21, 8): 6,    # War Hammer
        (21, 50): 20,  # Grond
        # Digging (tval 20)
        (20, 1): 5,    # Shovel
        (20, 3): 10,   # Mattock
        # Bows (tval 19)
        (19, 12): 1,   # Shortbow
        (19, 13): 6,   # Longbow
        (19, 14): 15,  # Dragon-horn Bow
        # Arrow (tval 17)
        (17, 1): 1,    # Arrow
        # Boots (tval 30)
        (30, 1): 1,    # Boots
        (30, 2): 4,    # Greaves
        (30, 3): 6,    # Mithril Greaves
        # Gloves (tval 31)
        (31, 1): 1,    # Gloves
        (31, 2): 3,    # Gauntlets
        (31, 3): 5,    # Mithril Gauntlets
        # Helm (tval 32)
        (32, 5): 3,    # Helm
        (32, 6): 5,    # Great Helm
        (32, 7): 10,   # Dwarf Mask
        (32, 8): 7,    # Mithril Helm
        # Crown (tval 33)
        (33, 11): 7,   # Crown
        (33, 50): 20,  # Morgoth Crown
        # Shield (tval 34)
        (34, 3): 3,    # Round Shield
        (34, 5): 6,    # Kite Shield
        (34, 10): 8,   # Mithril Shield
        # Cloak (tval 35)
        (35, 1): 2,    # Cloak
        (35, 6): 12,   # Shadow Cloak
        (35, 20): 20,  # Wolf-Hame
        (35, 21): 23,  # Bat-Fell
        # Soft Armor (tval 36)
        (36, 2): 1,    # Robe
        (36, 4): 1,    # Leather Armour
        (36, 7): 2,    # Studded Leather
        (36, 11): 17,  # Galvorn Armour
        # Mail (tval 37)
        (37, 4): 5,    # Mail Corslet
        (37, 6): 7,    # Hauberk
        (37, 20): 7,   # Mithril Corslet
        # Light (tval 39)
        (39, 2): 3,    # Lesser Jewel
        # Amulet (tval 40) - sval varies, default level
        (40, 10): 1,
        (40, 12): 1,
        (40, 13): 1,
        (40, 16): 1,
        # Ring (tval 45) - sval varies
        (45, 30): 1,
        (45, 31): 1,
        (45, 32): 1,
    }
    return base_levels.get((tval, sval), 3)


def get_base_ds(tval, sval):
    """Get base damage sides for weapon types from object.txt."""
    # These are the k_ptr->ds values from object.txt P: lines
    base_ds = {
        # Swords (tval 23)
        (23, 4): 5,    # Dagger: 1d5
        (23, 7): 5,    # Curved Sword: 2d5
        (23, 10): 7,   # Shortsword: 1d7
        (23, 17): 5,   # Longsword: 2d5
        (23, 21): 3,   # Bastard Sword: 3d3
        (23, 25): 5,   # Greatsword: 3d5
        (23, 28): 5,   # Mithril Longsword: 2d5
        (23, 30): 6,   # Mithril Greatsword: 3d6
        # Polearms (tval 22)
        (22, 1): 9,    # Spear: 1d9
        (22, 2): 13,   # Great Spear: 1d13
        (22, 4): 9,    # Glaive: 2d9
        (22, 11): 2,   # Hand Axe: 4d2
        (22, 12): 4,   # Battle Axe: 3d4
        (22, 13): 4,   # Great Axe: 4d4
        # Hafted (tval 21)
        (21, 3): 5,    # Quarterstaff: 2d5
        (21, 8): 1,    # War Hammer: 4d1
        (21, 50): 5,   # Grond: 6d5
        # Digging (tval 20)
        (20, 1): 2,    # Shovel: 2d2
        (20, 3): 2,    # Mattock: 5d2
        # Bows (tval 19)
        (19, 12): 7,   # Shortbow: 1d7
        (19, 13): 4,   # Longbow: 2d4
        (19, 14): 2,   # Dragon-horn Bow: 4d2
    }
    return base_ds.get((tval, sval), 0)


def get_base_protection(tval, sval):
    """Get base protection value (ps+1)*pd for armor types from object.txt."""
    # These are calculated from k_ptr->pd and k_ptr->ps from object.txt P: lines
    # Formula: (ps > 0) ? ((ps + 1) * pd) : 0
    base_prot = {
        # Boots (tval 30)
        (30, 1): 2,    # Boots: 1d1 -> (1+1)*1=2
        (30, 2): 3,    # Greaves: 1d2 -> (2+1)*1=3
        (30, 3): 3,    # Mithril Greaves: 1d2 -> 3
        # Gloves (tval 31)
        (31, 1): 0,    # Gloves: 1d0 -> 0
        (31, 2): 2,    # Gauntlets: 1d1 -> 2
        (31, 3): 2,    # Mithril Gauntlets: 1d1 -> 2
        # Helm (tval 32)
        (32, 5): 3,    # Helm: 1d2 -> 3
        (32, 6): 4,    # Great Helm: 1d3 -> 4
        (32, 7): 3,    # Dwarf Mask: 1d2 -> 3
        (32, 8): 4,    # Mithril Helm: 1d3 -> 4
        # Crown (tval 33)
        (33, 11): 0,   # Crown: 1d0 -> 0
        (33, 50): 0,   # Morgoth Crown: 0d0 -> 0
        # Shield (tval 34)
        (34, 3): 4,    # Round Shield: 1d3 -> 4
        (34, 5): 7,    # Kite Shield: 1d6 -> 7
        (34, 10): 7,   # Mithril Shield: 1d6 -> 7
        # Cloak (tval 35)
        (35, 1): 0,    # Cloak: 1d0 -> 0
        (35, 6): 0,    # Shadow Cloak: 1d0 -> 0
        (35, 20): 7,   # Wolf-Hame: 1d6 -> 7
        (35, 21): 0,   # Bat-Fell: 0d0 -> 0
        # Soft Armor (tval 36)
        (36, 2): 0,    # Robe: 1d0 -> 0
        (36, 4): 5,    # Leather: 1d4 -> 5
        (36, 7): 7,    # Studded Leather: 1d6 -> 7
        (36, 11): 9,   # Galvorn: 1d8 -> 9
        # Mail (tval 37)
        (37, 4): 10,   # Mail Corslet: 2d4 -> (4+1)*2=10
        (37, 6): 12,   # Hauberk: 2d5 -> (5+1)*2=12
        (37, 20): 10,  # Mithril Corslet: 2d4 -> 10
        # Ring (tval 45) - Ring of Barahir has 1d1
        (45, 32): 2,   # Ring: 1d1 -> 2
    }
    return base_prot.get((tval, sval), 0)


def get_base_att(tval, sval):
    """Get base attack bonus for weapon types from object.txt."""
    base_att = {
        # Swords (tval 23)
        (23, 4): 0,    # Dagger
        (23, 7): -1,   # Curved Sword
        (23, 10): 0,   # Shortsword
        (23, 17): 0,   # Longsword
        (23, 21): -2,  # Bastard Sword
        (23, 25): -2,  # Greatsword
        (23, 28): 1,   # Mithril Longsword
        (23, 30): -2,  # Mithril Greatsword
        # Polearms (tval 22)
        (22, 1): 0,    # Spear
        (22, 2): 1,    # Great Spear
        (22, 4): -1,   # Glaive
        (22, 11): -1,  # Hand Axe
        (22, 12): -3,  # Battle Axe
        (22, 13): -4,  # Great Axe
        # Hafted (tval 21)
        (21, 3): 0,    # Quarterstaff
        (21, 8): -2,   # War Hammer
        (21, 50): -7,  # Grond
        # Digging (tval 20)
        (20, 1): -3,   # Shovel
        (20, 3): -5,   # Mattock
        # Bows (tval 19)
        (19, 12): 0,   # Shortbow
        (19, 13): 0,   # Longbow
        (19, 14): 0,   # Dragon-horn Bow
    }
    return base_att.get((tval, sval), 0)


def get_base_evn(tval, sval):
    """Get base evasion bonus for items from object.txt."""
    base_evn = {
        # Swords (tval 23)
        (23, 7): 1,    # Curved Sword
        (23, 10): 1,   # Shortsword
        (23, 17): 1,   # Longsword
        (23, 21): 1,   # Bastard Sword
        (23, 25): 1,   # Greatsword
        (23, 28): 1,   # Mithril Longsword
        (23, 30): 1,   # Mithril Greatsword
        # Polearms (tval 22)
        (22, 2): 1,    # Great Spear
        (22, 4): 1,    # Glaive
        # Soft Armor (tval 36)
        (36, 2): 1,    # Robe
        (36, 4): -1,   # Leather
        (36, 7): -2,   # Studded Leather
        (36, 11): -1,  # Galvorn
        # Mail (tval 37)
        (37, 4): -3,   # Mail Corslet
        (37, 6): -4,   # Hauberk
        (37, 20): -2,  # Mithril Corslet
        # Helm (tval 32)
        (32, 5): -1,   # Helm
        (32, 6): -2,   # Great Helm
        (32, 7): -2,   # Dwarf Mask
        (32, 8): -1,   # Mithril Helm
        # Boots (tval 30)
        (30, 2): -1,   # Greaves
        # Gloves (tval 31)
        (31, 2): -1,   # Gauntlets
        # Cloak (tval 35)
        (35, 1): 1,    # Cloak
        (35, 6): 3,    # Shadow Cloak
        (35, 20): -3,  # Wolf-Hame
        (35, 21): 2,   # Bat-Fell
        # Shield (tval 34)
        (34, 5): -2,   # Kite Shield
        (34, 10): -1,  # Mithril Shield
        # Quarterstaff
        (21, 3): 2,    # Quarterstaff
    }
    return base_evn.get((tval, sval), 0)


def get_base_pval(tval, sval):
    """Get base pval for items from object.txt."""
    base_pval = {
        # Digging (tval 20)
        (20, 1): 1,    # Shovel: pval 1
        (20, 3): 2,    # Mattock: pval 2
        # Most items have pval 0 by default
    }
    return base_pval.get((tval, sval), 0)


def parse_object_file(filepath):
    """Parse object.txt to get all base item kinds with their stats."""
    objects = []
    current = None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            if not line or line.startswith('#'):
                continue
            
            # New object entry
            if line.startswith('N:'):
                if current and 'INSTA_ART' not in current['flags']:
                    # Skip INSTA_ART items - they're artefact-only templates
                    objects.append(current)
                parts = line[2:].split(':')
                idx = int(parts[0])
                name = ':'.join(parts[1:])
                current = {
                    'k_idx': idx,
                    'name': name,
                    'tval': 0,
                    'sval': 0,
                    'pval': 0,
                    'att': 0,
                    'evn': 0,
                    'dd': 0,
                    'ds': 0,
                    'pd': 0,
                    'ps': 0,
                    'level': 0,
                    'flags': [],
                }
            
            # Item info (contains tval:sval:pval)
            elif line.startswith('I:') and current:
                parts = line[2:].split(':')
                current['tval'] = int(parts[0])
                current['sval'] = int(parts[1])
                if len(parts) > 2 and parts[2]:
                    current['pval'] = int(parts[2])
            
            # Weight/level info
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['level'] = int(parts[0])
            
            # Combat stats: att:dam_dice:evn:prot_dice
            elif line.startswith('P:') and current:
                parts = line[2:].split(':')
                # Attack bonus
                att_str = parts[0].replace('+', '').strip()
                current['att'] = int(att_str) if att_str and att_str != '' else 0
                
                # Damage dice (XdY)
                if len(parts) > 1 and 'd' in parts[1]:
                    dam = parts[1].split('d')
                    current['dd'] = int(dam[0])
                    current['ds'] = int(dam[1])
                
                # Evasion bonus
                if len(parts) > 2:
                    evn_str = parts[2].replace('+', '').strip()
                    current['evn'] = int(evn_str) if evn_str and evn_str != '' else 0
                
                # Protection dice (XdY)
                if len(parts) > 3 and 'd' in parts[3]:
                    prot_part = parts[3].split(':')[0]
                    prot = prot_part.split('d')
                    current['pd'] = int(prot[0])
                    current['ps'] = int(prot[1])
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)
    
    if current and 'INSTA_ART' not in current['flags']:
        objects.append(current)
    
    return objects


def has_pval_mask(flags):
    """Check if item has any pval-dependent flags (TR1_PVAL_MASK)."""
    pval_flags = {'STR', 'DEX', 'CON', 'GRA', 'TUNNEL', 'STEALTH', 'PERCEPTION', 
                  'WILL', 'SMITHING', 'SONG', 'ARCHERY', 'DAMAGE_SIDES',
                  'STL', 'PER', 'WIL', 'SMT', 'SNG', 'ARC'}
    return any(f in pval_flags for f in flags)


def generate_special_variants(special, objects):
    """
    Generate all possible variants of a special item (ego) based on game logic.
    Mirrors build_ego_variants() from drop_system.c lines 1149-1410.
    """
    variants = []
    
    # For each tval range in the special
    for t_idx in range(len(special['tvals'])):
        tval = special['tvals'][t_idx]
        min_sval = special['min_svals'][t_idx]
        max_sval = special['max_svals'][t_idx]
        
        # Find all base objects that match this tval/sval range
        for obj in objects:
            if obj['tval'] != tval:
                continue
            if obj['sval'] < min_sval or obj['sval'] > max_sval:
                continue
            
            # Calculate stat ranges for this special + base combination
            # (mirrors lines 1249-1374 in drop_system.c)
            att_min = obj['att'] + (1 if special['max_att'] > 0 else 0)
            att_max = obj['att']
            ds_min = obj['ds'] + (1 if special['to_ds'] > 0 else 0)
            ds_max = obj['ds']
            evn_min = obj['evn'] + (1 if special['max_evn'] > 0 else 0)
            evn_max = obj['evn']
            ps_min = obj['ps'] + (1 if special['to_ps'] > 0 else 0)
            ps_max = obj['ps']
            pval_min = obj['pval'] + (1 if special['max_pval'] > 0 else 0)
            pval_max = obj['pval'] + special['max_pval']
            dd_min = obj['dd'] + (1 if special['to_dd'] > 0 else 0)
            dd_max = obj['dd'] + special['to_dd']
            pd_min = obj['pd'] + (1 if special['to_pd'] > 0 else 0)
            pd_max = obj['pd'] + special['to_pd']
            
            # Check if pval is allowed (base has pval flags or pval != 0 or ego grants pval)
            base_flags = set(obj['flags'])
            pval_allowed = has_pval_mask(base_flags) or obj['pval'] != 0 or special['max_pval'] > 0
            
            # Apply item-specific smithing rules (lines 1265-1350)
            if tval in [23, 22, 21, 20, 19]:  # Sword, Polearm, Hafted, Digging, Bow
                att_max = obj['att'] + 1 + special['max_att']
                ds_max = obj['ds'] + 1 + special['to_ds']
                evn_max = obj['evn'] + special['max_evn']
            elif tval in [30, 31, 32, 33, 34, 35, 36, 37]:  # Armor pieces
                att_max = obj['att'] + 1 + special['max_att']
                if att_max > 0:
                    att_max = 0
                evn_max = obj['evn'] + 1 + special['max_evn']
                ps_max = obj['ps'] + 1 + special['to_ps']
                # Cloaks and robes can't have protection
                if tval == 35 or (tval == 36 and obj['sval'] == 2):  # SV_ROBE = 2
                    ps_max = 0
                # Long Corslet gets +2 ps
                if tval == 37 and obj['sval'] == 7:  # SV_LONG_CORSLET = 7
                    ps_max = obj['ps'] + 2 + special['to_ps']
            elif tval == 45:  # Ring
                if obj['sval'] == 30:  # SV_RING_ACCURACY
                    att_max = 4
                    att_min = 1
                else:
                    att_max = obj['att'] + special['max_att']
                if obj['sval'] == 31:  # SV_RING_EVASION
                    evn_max = 4
                    evn_min = 1
                else:
                    evn_max = obj['evn'] + special['max_evn']
                if obj['sval'] == 32:  # SV_RING_PROTECTION
                    if pd_min < 1:
                        pd_min = 1
                    if pd_max < 1:
                        pd_max = 1
                    ps_max = 3
                    ps_min = 1
                if pval_allowed:
                    # Merge base and ego pval flags to check
                    combined_flags = base_flags | set(special['flags'])
                    if has_pval_mask(combined_flags):
                        if pval_min < 1:
                            pval_min = 1
                        if pval_max < 4:
                            pval_max = 4
                    if pval_max > 4:
                        pval_max = 4
            elif tval == 40:  # Amulet
                if pval_allowed:
                    combined_flags = base_flags | set(special['flags'])
                    if has_pval_mask(combined_flags):
                        if pval_min < 1:
                            pval_min = 1
                        if pval_max < 4:
                            pval_max = 4
                    if pval_max > 4:
                        pval_max = 4
            else:
                att_max = obj['att'] + special['max_att']
                ds_max = obj['ds'] + special['to_ds']
                evn_max = obj['evn'] + special['max_evn']
                ps_max = obj['ps'] + special['to_ps']
            
            # Clamp mins to maxs
            if att_min > att_max:
                att_min = att_max
            if ds_min > ds_max:
                ds_min = ds_max
            if evn_min > evn_max:
                evn_min = evn_max
            if ps_min > ps_max:
                ps_min = ps_max
            if pval_min > pval_max:
                pval_min = pval_max
            if dd_min > dd_max:
                dd_min = dd_max
            if pd_min > pd_max:
                pd_min = pd_max
            
            # Generate all combinations (lines 1377-1407)
            for att in range(att_min, att_max + 1):
                for ds in range(ds_min, ds_max + 1):
                    for evn in range(evn_min, evn_max + 1):
                        for ps in range(ps_min, ps_max + 1):
                            pval_hi = pval_max if pval_allowed else pval_min
                            for pval in range(pval_min, pval_hi + 1):
                                for dd in range(dd_min, dd_max + 1):
                                    for pd in range(pd_min, pd_max + 1):
                                        variant = {
                                            'type': 'special',
                                            'special_idx': special['idx'],
                                            'name': f"{special['name']} {obj['name']}",
                                            'base_name': obj['name'],
                                            'special_name': special['name'],
                                            'tval': obj['tval'],
                                            'sval': obj['sval'],
                                            'att': att,
                                            'ds': ds,
                                            'dd': dd,
                                            'evn': evn,
                                            'ps': ps,
                                            'pd': pd,
                                            'pval': pval,
                                            'abilities': special['abilities'],
                                            'flags': special['flags'][:],  # Copy flags
                                            'depth': special['depth'],
                                            'rarity': special['rarity'],
                                            # Store base stats for difficulty calculation
                                            'base_att': obj['att'],
                                            'base_evn': obj['evn'],
                                            'base_ds': obj['ds'],
                                            'base_ps': obj['ps'],
                                            'base_pd': obj['pd'],
                                            'base_dd': obj['dd'],
                                            'base_pval': obj['pval'],
                                            'base_level': obj['level'],
                                        }
                                        variants.append(variant)
    
    return variants


def get_base_flags(tval, sval):
    """Get base flags for items from object.txt that should be subtracted."""
    # For non-jewelry items, base flags don't count toward difficulty
    # But TUNNEL and STEALTH are added back in (see cmd4.c lines 4072-4077)
    base_flags = {
        # Digging tools have TUNNEL flag
        (20, 1): {'TUNNEL'},  # Shovel
        (20, 3): {'TUNNEL'},  # Mattock
    }
    return base_flags.get((tval, sval), set())


def get_slot(tval):
    """Determine equipment slot category for difficulty multiplier."""
    slots = {
        45: 'ring',
        40: 'amulet',
        39: 'light',
        35: 'cloak',
        31: 'gloves',
        30: 'boots',
        17: 'arrow',
    }
    return slots.get(tval, 'major')


def get_tval_name(tval):
    """Return human-readable name for tval."""
    tval_names = {
        17: 'Arrow',
        19: 'Bow',
        20: 'Digging',
        21: 'Hafted',
        22: 'Polearm',
        23: 'Sword',
        30: 'Boots',
        31: 'Gloves',
        32: 'Helm',
        33: 'Crown',
        34: 'Shield',
        35: 'Cloak',
        36: 'Soft Armor',
        37: 'Mail',
        39: 'Light',
        40: 'Amulet',
        45: 'Ring',
    }
    return tval_names.get(tval, f'Unknown({tval})')


def generate_normal_variants(objects):
    """
    Generate all possible variants of normal (non-ego, non-artefact) items.
    Mirrors build_normal_variants() from drop_system.c lines 974-1144.
    This includes rings and amulets with all stat combinations.
    """
    variants = []
    
    for obj in objects:
        # Skip supplies (no smithing variants)
        if obj['tval'] in [75, 70, 66, 80, 82]:  # Potion, Staff, Gem, Food, Flask
            continue
        
        # Skip lights (handled differently)
        if obj['tval'] == 39:  # Light
            continue
            
        # Determine if pval is allowed
        base_flags = set(obj['flags'])
        pval_allowed = has_pval_mask(base_flags) or obj['pval'] != 0
        
        # Set up smithing caps based on item type
        att_min = obj['att']
        att_max = obj['att']
        ds_min = obj['ds']
        ds_max = obj['ds']
        evn_min = obj['evn']
        evn_max = obj['evn']
        ps_min = obj['ps']
        ps_max = obj['ps']
        pval_min = obj['pval']
        pval_max = obj['pval']
        pd_min = obj['pd']
        pd_max = obj['pd']
        dd_min = obj['dd']
        dd_max = obj['dd']
        
        tval = obj['tval']
        sval = obj['sval']
        
        if tval in [23, 22, 21, 20, 19]:  # Sword, Polearm, Hafted, Digging, Bow
            att_max = obj['att'] + 1
            ds_max = obj['ds'] + 1
        elif tval in [30, 31, 32, 33, 34, 35, 36, 37]:  # Armor pieces
            att_max = obj['att'] + 1
            if att_max > 0:
                att_max = 0
            evn_max = obj['evn'] + 1
            ps_max = obj['ps'] + 1
            if tval == 35 or (tval == 36 and sval == 2):  # Cloak or Robe
                ps_max = 0
            if tval == 37 and sval == 7:  # Long Corslet (SV_LONG_CORSLET = 7)
                ps_max = obj['ps'] + 2
        elif tval == 45:  # Ring
            if sval == 8:  # SV_RING_ACCURACY
                att_max = 4
                att_min = 1
            else:
                att_max = obj['att']
            if sval == 2:  # SV_RING_EVASION
                evn_max = 4
                evn_min = 1
            else:
                evn_max = obj['evn']
            if sval == 3:  # SV_RING_PROTECTION
                # Protection rings are always 1dX (never 0dX)
                pd_min = 1
                pd_max = 1
                ps_max = 3
                ps_min = 1
            if pval_allowed:
                if pval_min < 1:
                    pval_min = 1
                pval_max = 4  # smithing caps ring/amulet pval at 4
        elif tval == 40:  # Amulet
            if pval_allowed:
                if pval_min < 1:
                    pval_min = 1
                pval_max = 4  # smithing caps ring/amulet pval at 4
        
        # Generate all combinations
        for att in range(att_min, att_max + 1):
            for ds in range(ds_min, ds_max + 1):
                for evn in range(evn_min, evn_max + 1):
                    for ps in range(ps_min, ps_max + 1):
                        for dd in range(dd_min, dd_max + 1):
                            for pd in range(pd_min, pd_max + 1):
                                pval_hi = pval_max if pval_allowed else pval_min
                                for pval in range(pval_min, pval_hi + 1):
                                    variant = {
                                        'type': 'normal',
                                        'k_idx': obj['k_idx'],
                                        'name': obj['name'],
                                        'tval': obj['tval'],
                                        'sval': obj['sval'],
                                        'att': att,
                                        'ds': ds,
                                        'dd': dd,
                                        'evn': evn,
                                        'ps': ps,
                                        'pd': pd,
                                        'pval': pval,
                                        'abilities': 0,
                                        'flags': obj['flags'][:],
                                        'depth': obj['level'],
                                        'rarity': 1,  # Default rarity
                                        # Store base stats for difficulty calculation
                                        'base_att': obj['att'],
                                        'base_evn': obj['evn'],
                                        'base_ds': obj['ds'],
                                        'base_ps': obj['ps'],
                                        'base_pd': obj['pd'],
                                        'base_dd': obj['dd'],
                                        'base_pval': obj['pval'],
                                        'base_level': obj['level'],
                                    }
                                    variants.append(variant)
    
    return variants


def export_csv(items, output_file):
    """Export items to CSV file."""
    import csv
    
    items_sorted = sorted(items, key=lambda x: x['difficulty'])
    
    with open(output_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Header
        writer.writerow([
            'Rank', 'Name', 'Type', 'Category', 'Difficulty', 'Depth', 'Rarity',
            'Attack', 'Evasion', 'DD', 'DS', 'PD', 'PS', 'Pval', 'Abilities', 'Flags'
        ])
        
        # Data rows
        for i, item in enumerate(items_sorted, 1):
            category = get_tval_name(item['tval'])
            flags_str = '|'.join(item['flags']) if item['flags'] else ''
            
            writer.writerow([
                i,
                item['name'],
                item['type'].upper(),
                category,
                item['difficulty'],
                item['depth'],
                item['rarity'],
                item.get('att', 0),
                item.get('evn', 0),
                item.get('dd', 0),
                item.get('ds', 0),
                item.get('pd', 0),
                item.get('ps', 0),
                item.get('pval', 0),
                item.get('abilities', 0),
                flags_str
            ])
    
    return len(items_sorted)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Calculate artefact/special smithing difficulty')
    parser.add_argument('--csv', '-c', metavar='FILE', 
                        help='Export results to CSV file')
    args = parser.parse_args()
    
    # Try to determine script location, fallback to current working directory
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        script_dir = os.getcwd()
    
    # Try multiple possible locations for artefact.txt
    possible_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'artefact.txt'),
        os.path.join(script_dir, 'lib', 'edit', 'artefact.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\artefact.txt',
    ]
    
    artefact_file = None
    for path in possible_paths:
        if os.path.exists(path):
            artefact_file = path
            break
    
    if not artefact_file:
        raise FileNotFoundError("Could not find artefact.txt")
    
    # Find special.txt
    special_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'special.txt'),
        os.path.join(script_dir, 'lib', 'edit', 'special.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\special.txt',
    ]
    
    special_file = None
    for path in special_paths:
        if os.path.exists(path):
            special_file = path
            break
    
    # Find object.txt
    object_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'object.txt'),
        os.path.join(script_dir, 'lib', 'edit', 'object.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\object.txt',
    ]
    
    object_file = None
    for path in object_paths:
        if os.path.exists(path):
            object_file = path
            break
    
    artefacts = parse_artefact_file(artefact_file)
    specials_raw = parse_special_file(special_file) if special_file else []
    objects = parse_object_file(object_file) if object_file else []
    
    # Filter out "Ultimate" template artefacts (idx 182-198)
    artefacts = [a for a in artefacts if not (a['name'].startswith("'Ultimate") and a['idx'] >= 182)]
    
    # Generate all normal item variants (including rings/amulets)
    print(f"Generating normal item variants from {len(objects)} base items...")
    normals = generate_normal_variants(objects)
    print(f"Total normal variants: {len(normals)}")
    
    # Generate all special item variants (matching game logic)
    print(f"\nGenerating special item variants from {len(specials_raw)} special types and {len(objects)} base items...")
    specials = []
    for spec in specials_raw:
        variants = generate_special_variants(spec, objects)
        specials.extend(variants)
        if variants:
            print(f"  {spec['name']}: {len(variants)} variants")
    
    print(f"\nTotal special variants: {len(specials)}")
    print(f"Total artefacts: {len(artefacts)}")
    
    # Calculate difficulties
    all_items = []
    for art in artefacts:
        art['type'] = 'artefact'
        art['difficulty'] = calculate_difficulty(art)
        all_items.append(art)
    
    for norm in normals:
        norm['difficulty'] = calculate_difficulty(norm)
        all_items.append(norm)
    
    for spec in specials:
        spec['difficulty'] = calculate_difficulty(spec)
        all_items.append(spec)
    
    # CSV export mode
    if args.csv:
        count = export_csv(all_items, args.csv)
        print(f"\nExported {count} items to {args.csv}")
        print(f"  Artefacts: {len(artefacts)}")
        print(f"  Normal variants: {len(normals)}")
        print(f"  Special variants: {len(specials)}")
        return
    
    # Sort by difficulty (descending)
    items_by_diff = sorted(all_items, key=lambda x: x['difficulty'], reverse=True)
    artefacts_by_diff = sorted(artefacts, key=lambda x: x['difficulty'], reverse=True)
    
    print("=" * 110)
    print("ARTEFACT & SPECIAL SMITHING DIFFICULTY ANALYSIS")
    print("=" * 110)
    print()
    
    # Summary by difficulty tiers
    print("DIFFICULTY DISTRIBUTION:")
    print("-" * 50)
    tiers = [
        (60, 255, "Legendary (60+)"),
        (45, 59, "Epic (45-59)"),
        (35, 44, "Very Hard (35-44)"),
        (25, 34, "Hard (25-34)"),
        (15, 24, "Moderate (15-24)"),
        (0, 14, "Simple (0-14)"),
    ]
    for low, high, label in tiers:
        art_count = len([a for a in artefacts if low <= a['difficulty'] <= high])
        norm_count = len([n for n in normals if low <= n['difficulty'] <= high])
        spec_count = len([s for s in specials if low <= s['difficulty'] <= high])
        print(f"  {label}: {art_count} artefacts, {norm_count} normals, {spec_count} specials")
    print()
    
    # Full listing sorted by difficulty
    print("FULL ITEM LIST (by Smithing Difficulty):")
    print("-" * 110)
    print(f"{'Idx':>4} {'Diff':>4} {'Lvl':>3} {'D+L':>4} {'Type':<12} {'Name':<35} {'Stats':<25}")
    print("-" * 110)
    
    for item in items_by_diff:
        tval_name = get_tval_name(item['tval'])
        if item['type'] == 'artefact':
            item_type = 'ART'
        elif item['type'] == 'special':
            item_type = 'SPC'
        else:
            item_type = 'NRM'
        tval_name = f"{item_type}:{tval_name}"
        stats = []
        if item['att'] != 0 or item['dd'] > 0:
            stats.append(f"+{item['att']},{item['dd']}d{item['ds']}")
        if item['evn'] != 0 or item['pd'] > 0:
            stats.append(f"+{item['evn']},{item['pd']}d{item['ps']}")
        if item['pval'] != 0:
            stats.append(f"pval:{item['pval']}")
        if item['abilities'] > 0:
            stats.append(f"abil:{item['abilities']}")
        stats_str = ' '.join(stats)
        
        diff_plus_lvl = item['difficulty'] + item['depth']
        idx = item.get('idx', item.get('special_idx', item.get('k_idx', 0)))
        print(f"{idx:>4} {item['difficulty']:>4} {item['depth']:>3} {diff_plus_lvl:>4} {tval_name:<12} {item['name']:<35} {stats_str:<25}")
    
    print()
    print("=" * 100)
    print()
    
    # Group by item type
    print("ARTEFACTS BY TYPE:")
    print("-" * 100)
    
    by_type = {}
    for art in artefacts:
        tval_name = get_tval_name(art['tval'])
        if tval_name not in by_type:
            by_type[tval_name] = []
        by_type[tval_name].append(art)
    
    for tval_name in sorted(by_type.keys()):
        arts = sorted(by_type[tval_name], key=lambda x: x['difficulty'], reverse=True)
        avg_diff = sum(a['difficulty'] for a in arts) / len(arts)
        print(f"\n{tval_name} ({len(arts)} artefacts, avg diff: {avg_diff:.1f}):")
        for art in arts:
            print(f"  {art['difficulty']:>3}: {art['name']}")
    
    print()
    print("=" * 110)
    print("DIFFICULTY FORMULA (from src/cmd4.c object_difficulty()):")
    print("  - Base item level / 2")
    print("  - Attack bonus: weapons +3/point, others +6/point")
    print("  - Evasion bonus: +6/point")
    print("  - Damage sides: +3*x+2 per extra side (triangular)")
    print("  - Protection: +3/point")
    print("  - Slays: +3-5 each (Spider/Rauko/Dragon +4, Man/Elf +5, others +3)")
    print("  - Brands: Cold +18, Fire +14, Poison +16 (+20 per extra brand)")
    print("  - Sharpness: +24 (arrows +14), Sharpness2: +40")
    print("  - Stats (STR/DEX/CON/GRA): +14/pval")
    print("  - Skills: Perception/Will +3/pval, Stealth/Song/Archery +4/pval")
    print("  - Sustains: +2 each")
    print("  - Resistances: Fire/Cold/Poison +5, Fear/Blind/Confu/Stun +2, Bleed/Hallu +1")
    print("  - Free Action: +7, Light: +8, Radiance: +6, Regen: +4, See Invis: +4")
    print("  - Speed: +40, Cheat Death: +13, Accurate: +15, Vampiric: +6")
    print("  - Abilities: +6 each (5 + level/3)")
    print("  - Minor slots (ring/cloak/gloves/boots/light/arrow): +20% multiplier")
    print("  - Artefact arrows: difficulty halved")
    print("=" * 110)
    print()
    input("Press Enter to exit...")


if __name__ == '__main__':
    main()
