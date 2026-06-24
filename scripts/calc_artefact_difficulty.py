#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Calculate smithing difficulty for each artefact in artefact.txt and special.txt

Based on the REAL object_difficulty() function in src/cmd4.c (lines 3980+)
which includes proper costs for flags like slays, brands, sharpness, etc.
"""

import re
import os
import sys


BONUS_TOKEN_ALIASES = {
    # Skills sometimes appear abbreviated in tools/scripts.
    'ARC': 'ARCHERY',
    'STL': 'STEALTH',
    'PER': 'PERCEPTION',
    'WIL': 'WILL',
    'SMT': 'SMITHING',
    'SNG': 'SONG',
}

STAT_TOKENS = ('STR', 'DEX', 'CON', 'GRA')
SKILL_TOKENS = ('ARCHERY', 'STEALTH', 'PERCEPTION', 'WILL', 'SMITHING', 'SONG')

# Global dictionary mapping (skill_number, ability_value) -> level
# Populated by parse_ability_file()
ABILITY_LEVELS = {}

# Global dictionary mapping (tval, sval) -> object data (including weight)
# Populated by populate_objects_dict()
OBJECTS_BY_TYPE = {}


def parse_ability_file(filepath):
    """Parse ability.txt to get ability levels.

    Format:
        N: ability_number : ability_name
        I: skill_number : ability_value : level_requirement

    Returns dict mapping (skill_number, ability_value) -> level
    """
    global ABILITY_LEVELS
    ABILITY_LEVELS = {}

    current_ability_num = None

    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()

            if not line or line.startswith('#'):
                continue

            if line.startswith('N:'):
                parts = line[2:].split(':')
                current_ability_num = int(parts[0])

            elif line.startswith('I:') and current_ability_num is not None:
                parts = line[2:].split(':')
                skill_num = int(parts[0])
                ability_val = int(parts[1])
                level = int(parts[2])
                ABILITY_LEVELS[(skill_num, ability_val)] = level

    return ABILITY_LEVELS


def get_ability_level(skill_num, ability_val):
    """Get the level for an ability, defaulting to 5 if not found."""
    return ABILITY_LEVELS.get((skill_num, ability_val), 5)


def normalize_bonus_token(token: str) -> str:
    t = token.strip().upper()
    if t.startswith('NEG_'):
        base = t[4:]
        base = BONUS_TOKEN_ALIASES.get(base, base)
        return f"NEG_{base}"
    return BONUS_TOKEN_ALIASES.get(t, t)


def is_prefix_ego(name: str) -> bool:
    """
    Detect if an ego name is a prefix type.
    Prefix ego names are wrapped in parentheses: (Protective), (Stealthy), etc.
    Suffix ego names start with "of": of Nogrod, of Resilience, etc.

    Matches the C code in angband.h ego_name_is_prefix().
    """
    if not name or len(name) < 2:
        return False
    return name[0] == '(' and name[-1] == ')'


def has_alignment_conflict(*flag_collections) -> bool:
    """Return True when NOBLE_ITEM and EVIL_ITEM appear across combined components."""
    has_noble = False
    has_evil = False

    for flags in flag_collections:
        if not flags:
            continue
        if 'NOBLE_ITEM' in flags:
            has_noble = True
        if 'EVIL_ITEM' in flags:
            has_evil = True
        if has_noble and has_evil:
            return True

    return False


def suffix_forbids_prefix_combo(special: dict) -> bool:
    """Match drop_system.c: suffix egos with NO_PREFIX cannot form dual-ego combos."""
    return 'NO_PREFIX' in special.get('flags', [])


def compute_stat_skill_bonuses(flags: set, total_pval: int, overrides: dict | None):
    """
    Derive per-stat/per-skill bonus values from flags + pval and apply any M: overrides.

    Mirrors the C logic added for per-stat/per-skill bonuses: defaults come from pval,
    then M:<TOKEN>:<VALUE> can override individual stats/skills.
    """
    stat_bonus = {k: 0 for k in STAT_TOKENS}
    skill_bonus = {k: 0 for k in SKILL_TOKENS}

    flags_norm = {normalize_bonus_token(f) for f in flags}

    # Defaults from flags and pval.
    if total_pval != 0:
        for stat in STAT_TOKENS:
            if stat in flags_norm:
                stat_bonus[stat] += total_pval
            if f"NEG_{stat}" in flags_norm:
                stat_bonus[stat] -= total_pval

        for skill in SKILL_TOKENS:
            if skill in flags_norm:
                skill_bonus[skill] = total_pval

    # Apply overrides (overwrite the derived value).
    if overrides:
        for raw_token, value in overrides.items():
            token = normalize_bonus_token(raw_token)
            normalized = int(value)
            if token.startswith('NEG_') and normalized > 0:
                normalized = -normalized

            base = token[4:] if token.startswith('NEG_') else token

            if base in stat_bonus:
                stat_bonus[base] = normalized
            elif base in skill_bonus:
                skill_bonus[base] = normalized

    return stat_bonus, skill_bonus


def c_trunc_div(numer: int, denom: int) -> int:
    """Match C integer division semantics (truncate toward zero)."""
    if denom == 0:
        raise ZeroDivisionError("division by zero")
    return abs(numer) // abs(denom) * (1 if numer == 0 or numer * denom > 0 else -1)


def parse_artefact_file(filepath):
    """Parse artefact.txt and return list of artefact data."""
    artefacts = []
    current = None
    sval_order = {}  # Track the order (tval, sval) pairs appear in the file
    order_counter = 0
    
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
                    'ability_list': [],  # List of (skill_num, ability_val) tuples
                    'flags': [],
                    'bonus_overrides': {},
                    'depth': 0,
                    'rarity': 0,
                    'weight': 0,
                }

            # Item type info
            elif line.startswith('I:') and current:
                parts = line[2:].split(':')
                current['tval'] = int(parts[0])
                current['sval'] = int(parts[1])
                if len(parts) > 2:
                    current['pval'] = int(parts[2])
                # Track the order this (tval, sval) appears
                key = (current['tval'], current['sval'])
                if key not in sval_order:
                    sval_order[key] = order_counter
                    order_counter += 1

            # Depth/rarity/weight/cost info (W:depth:rarity:weight:cost)
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0])
                current['rarity'] = int(parts[1])
                if len(parts) > 2:
                    current['weight'] = int(parts[2])

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
                for pair in ability_pairs:
                    if '/' in pair:
                        parts = pair.split('/')
                        skill_num = int(parts[0])
                        ability_val = int(parts[1])
                        current['ability_list'].append((skill_num, ability_val))
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f for f in re.split(r'[\s|]+', line[2:].strip()) if f]
                current['flags'].extend(flags)

            # Per-stat/per-skill overrides (M:<TOKEN>:<VALUE>)
            elif line.startswith('M:') and current:
                parts = line[2:].split(':')
                if len(parts) >= 2:
                    token = parts[0].strip()
                    value = int(parts[1].strip())
                    current['bonus_overrides'][token] = value
    
    # Don't forget the last artefact
    if current:
        artefacts.append(current)
    
    return artefacts, sval_order


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
                    'is_prefix': is_prefix_ego(name),  # Track if this is a prefix or suffix ego
                    'tvals': [],  # Can have multiple tval ranges
                    'min_svals': [],
                    'max_svals': [],
                    'max_pval': 0,
                    'min_pval': 0,
                    'max_att': 0,
                    'max_evn': 0,
                    'to_dd': 0,
                    'to_ds': 0,
                    'to_pd': 0,
                    'to_ps': 0,
                    'ability_list': [],  # List of (skill_num, ability_val) tuples
                    'flags': [],
                    'bonus_overrides': {},
                    'alloc_pairs': [],  # A: allocation schedule [(depth, rarity), ...]
                    'depth': 0,
                    'rarity': 0,
                }
            
            # Depth/rarity info
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0])
                current['rarity'] = int(parts[1])
            
            # Creation bonuses: C:max_att:+dd:+ds:max_evn:+pd:+ps:max_pval[:min_pval]
            # These are MAXIMUM bonuses that get ADDED to the base item
            # When smithing, you get +1 to each stat if its max > 0
            # Optional 8th field min_pval sets a minimum pval contribution for this ego
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
                if len(parts) > 7:
                    current['min_pval'] = int(parts[7]) if parts[7] else 0
                # If max_pval > 0 and min_pval is 0 (default), set min_pval to 1
                if current['max_pval'] > 0 and current['min_pval'] == 0:
                    current['min_pval'] = 1
            
            # Tval/sval range - can have multiple T: lines
            elif line.startswith('T:') and current:
                parts = line[2:].split(':')
                current['tvals'].append(int(parts[0]))
                current['min_svals'].append(int(parts[1]))
                current['max_svals'].append(int(parts[2]) if len(parts) > 2 else int(parts[1]))
            
            # Abilities (A: line format in special.txt: A:count:skill:ability:level)
            # Note: special.txt uses A: not B: for abilities, but we handle B: too for consistency
            elif line.startswith('B:') and current:
                abilities_str = line[2:]
                ability_pairs = abilities_str.split(':')
                for pair in ability_pairs:
                    if '/' in pair:
                        parts = pair.split('/')
                        skill_num = int(parts[0])
                        ability_val = int(parts[1])
                        current['ability_list'].append((skill_num, ability_val))

            # A: line in special.txt has two formats:
            # 1. A:count:skill:ability:level (grants abilities to item) - e.g., A:1:0:13:5
            # 2. A:depth/rarity:depth/rarity:... (allocation schedule) - e.g., A:1/3:10/5:15/10
            elif line.startswith('A:') and current:
                parts = line[2:].split(':')
                # Presence of '/' in the first segment identifies an allocation schedule
                if '/' in parts[0]:
                    current['alloc_pairs'] = parse_alloc_pairs(line[2:])
                elif len(parts) >= 4:
                    # A:count:skill:ability:level
                    try:
                        count = int(parts[0])
                        skill_num = int(parts[1])
                        ability_val = int(parts[2])
                        # level is in parts[3] but we get it from ability.txt
                        for _ in range(count):
                            current['ability_list'].append((skill_num, ability_val))
                    except ValueError:
                        pass  # Skip if parsing fails
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)

            # Per-stat/per-skill overrides (M:<TOKEN>:<VALUE>)
            elif line.startswith('M:') and current:
                parts = line[2:].split(':')
                if len(parts) >= 2:
                    token = parts[0].strip()
                    value = int(parts[1].strip())
                    current['bonus_overrides'][token] = value
    
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
    is_dual_ego = (art['type'] == 'dual_ego')

    # For non-jewelry items (special/dual-ego/artefact), strip base flags and add back specific ones
    # This mirrors smithing-difficulty.c lines 139-180
    is_artefact = (art['type'] == 'artefact')
    if (is_special or is_dual_ego or is_artefact) and tval not in [45, 40]:  # Not ring or amulet
        # Get base item flags from the OBJECTS_BY_TYPE lookup
        base_key = (tval, sval)
        if base_key in OBJECTS_BY_TYPE:
            base_obj_flags = set(OBJECTS_BY_TYPE[base_key].get('flags', []))
            # For artefacts, ADD base flags first (special/ego already have them combined at line 1315)
            # This mirrors how object_flags4() in C code combines base + artefact flags
            if is_artefact:
                flags = flags | base_obj_flags
            # Strip base item flags
            flags = flags - base_obj_flags
            # Add back specific flags that should always count
            add_back_flags = {'TUNNEL', 'STL', 'STEALTH', 'ACCURATE', 'SHARPNESS', 'SHARPNESS2',
                              'DAMAGE_SIDES', 'REGEN', 'RES_COLD', 'RES_FIRE',
                              'CHEAT_DEATH', 'STAND_FAST', 'ENCHANTABLE'}
            for flag in add_back_flags:
                if flag in base_obj_flags:
                    flags.add(flag)

    # Get base item stats
    if is_special or is_normal or is_dual_ego:
        # For all generated variants, base stats are stored directly
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

    if tval in [45, 40]:  # Ring / amulet combat bonuses always pay from zero
        base_att = 0
        base_evn = 0
        base_ds = 0
        base_prot = 0
    
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

    # Horn items add (level - 1) difficulty (cmd4.c:4593-4613)
    if tval == 47:  # TV_HORN
        dif_inc += base_level - 1

    # Weight factor calculation (cmd4.c:4663-4671)
    # Unusual weight items (lighter or heavier than base) get difficulty bonus
    item_weight = art.get('weight', 0)
    base_weight = get_base_weight(tval, sval)
    if base_weight > 0:  # Only calculate if we have base weight data
        if item_weight == 0:
            weight_factor = 1100
        elif item_weight > base_weight:
            weight_factor = 100 * item_weight // base_weight
        else:
            weight_factor = 100 * base_weight // item_weight
        dif_inc += (weight_factor - 100) // 20
    if 'WEIGHT' in flags or 'NEG_WEIGHT' in flags:
        dif_inc += 5

    # Attack bonus contribution (bonus above base)
    att_bonus = smithed_att_bonus
    if att_bonus != 0:
        if tval in [17, 19, 23, 22, 21]:  # Arrow, Bow, Sword, Polearm, Hafted
            dif_inc += dif_mod_calc(att_bonus, 3)
        else:
            val = dif_mod_calc(att_bonus, 6)
            if att_bonus > 0:
                val -= 1
            dif_inc += val

    # Evasion bonus (bonus above base): armor uses dif_mod(x, 6)-1, non-armor uses dif_mod(x, 9)-2.
    # Negative bonuses reduce difficulty using the same signed progression.
    evn_bonus = smithed_evn_bonus
    if evn_bonus != 0:
        armor_tvals = {30, 31, 32, 33, 34, 35, 36, 37}
        if tval in armor_tvals:
            val = dif_mod_calc(evn_bonus, 6)
            if evn_bonus > 0:
                val -= 1
        else:
            val = dif_mod_calc(evn_bonus, 9)
            if evn_bonus > 0:
                val -= 2
        dif_inc += val
    
    # Damage sides bonus: dif_mod(x, 3*|x|+2, &dif_inc)
    ds_bonus = smithed_ds_bonus
    if ds_bonus != 0:
        dif_inc += dif_mod_calc(ds_bonus, 3 * abs(ds_bonus) + 2)
    
    # Protection bonus
    if prot_bonus != 0:
        if tval == 37 and sval == 6:  # Hauberk
            dif_inc += dif_mod_calc(prot_bonus, 1)
            if prot_bonus > 0:
                dif_inc += 2
        elif tval == 40:  # Amulet
            dif_inc += dif_mod_calc(prot_bonus, 1)
            if prot_bonus > 0:
                dif_inc += 4
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

    if 'SLAY_SERPENT' in flags:
        dif_inc += 4
    if 'SLAY_VAMPIRE' in flags:
        dif_inc += 4
    if 'SLAY_HORROR' in flags:
        dif_inc += 4
    if 'SLAY_CAT' in flags:
        dif_inc += 3
    if 'SLAY_GIANT' in flags:
        dif_inc += 3
    
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
    if 'BRAND_ELEC' in flags:
        dif_inc += 16  # No monsters have HURT_ELEC, same as poison
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
    if 'WILL_DRAIN' in flags:
        dif_inc += 8  # Like VAMPIRIC+2
    if 'ACCURATE' in flags:
        dif_inc += 15
    if 'ARMOR_SHATTER' in flags:
        dif_inc += 15  # Like ACCURATE
    if 'DEPTH_SCALE_PS' in flags:
        dif_inc += 5  # Situational
    if 'PAIRED' in flags:
        dif_inc += 3  # Paired weapon bonus
    if 'SUBTLETY_THROW' in flags:
        dif_inc += 15
    if 'LIGHT_ARMOR' in flags:
        dif_inc += 2  # Light armour tag (e.g. the (Light) ego)

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
    
    # Per-stat/per-skill bonuses (no longer necessarily tied to a single pval).
    overrides = art.get('bonus_overrides', None)
    stat_bonus, skill_bonus = compute_stat_skill_bonuses(flags, total_pval, overrides)
    art['stat_bonus'] = stat_bonus
    art['skill_bonus'] = skill_bonus

    if total_pval > 0 and 'DAMAGE_SIDES' in flags:
        dif_inc += dif_mod_calc(total_pval, 18)

    if stat_bonus['STR'] > 0:
        dif_inc += dif_mod_calc(stat_bonus['STR'], 14)
    elif stat_bonus['STR'] < 0:
        dif_dec += dif_mod_calc(-stat_bonus['STR'], 10)
    if stat_bonus['DEX'] > 0:
        dif_inc += dif_mod_calc(stat_bonus['DEX'], 14)
    elif stat_bonus['DEX'] < 0:
        dif_dec += dif_mod_calc(-stat_bonus['DEX'], 10)
    if stat_bonus['CON'] > 0:
        dif_inc += dif_mod_calc(stat_bonus['CON'], 14)
    elif stat_bonus['CON'] < 0:
        dif_dec += dif_mod_calc(-stat_bonus['CON'], 10)
    if stat_bonus['GRA'] > 0:
        dif_inc += dif_mod_calc(stat_bonus['GRA'], 14)
    elif stat_bonus['GRA'] < 0:
        dif_dec += dif_mod_calc(-stat_bonus['GRA'], 10)

    if skill_bonus['ARCHERY'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['ARCHERY'], 4)
    elif skill_bonus['ARCHERY'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['ARCHERY'], 3)
    if skill_bonus['STEALTH'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['STEALTH'], 4)
    elif skill_bonus['STEALTH'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['STEALTH'], 3)
    if skill_bonus['PERCEPTION'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['PERCEPTION'], 3)
    elif skill_bonus['PERCEPTION'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['PERCEPTION'], 2)
    if skill_bonus['WILL'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['WILL'], 3)
    elif skill_bonus['WILL'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['WILL'], 2)
    if skill_bonus['SMITHING'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['SMITHING'], 4)
    elif skill_bonus['SMITHING'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['SMITHING'], 3)
    if skill_bonus['SONG'] > 0:
        dif_inc += dif_mod_calc(skill_bonus['SONG'], 4)
    elif skill_bonus['SONG'] < 0:
        dif_dec += dif_mod_calc(-skill_bonus['SONG'], 3)

    # Extra difficulty for multiple distinct stat/skill bonuses (first is "free").
    stat_count = sum(1 for k in STAT_TOKENS if stat_bonus.get(k, 0) > 0)
    skill_count = sum(1 for k in SKILL_TOKENS if skill_bonus.get(k, 0) > 0)
    if stat_count > 1:
        dif_inc += (stat_count - 1) * 7
    if skill_count > 1:
        dif_inc += (skill_count - 1) * 3
    
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
    if 'OATH_BOOST' in flags:
        dif_inc += 5

    # === ELEMENTAL RESISTANCES ===
    if 'RES_COLD' in flags:
        dif_inc += 5
    if 'RES_FIRE' in flags:
        dif_inc += 5
    if 'RES_POIS' in flags:
        dif_inc += 5
    if 'RES_ELEC' in flags:
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
    
    # === PENALTY FLAGS ===
    # All penalty flags now apply to all items including artefacts
    if 'DANGER' in flags:
        dif_dec += 5
    if 'DARKNESS' in flags:
        dif_dec += 2  # Changed from 3
    if 'AGGRAVATE' in flags:
        dif_dec += 3
    if 'HAUNTED' in flags:
        dif_dec += 5
    if 'VUL_COLD' in flags:
        dif_dec += 4
    if 'VUL_FIRE' in flags:
        dif_dec += 4
    if 'VUL_POIS' in flags:
        dif_dec += 4
    if 'TRAITOR' in flags:
        dif_dec += 2
    if 'CUMBERSOME' in flags:
        dif_dec += 3
    if 'OATH_NEGATE' in flags:
        dif_dec += 5
    if 'UNLIGHT' in flags:
        dif_dec += 5  # Worse than DARKNESS - pure negative, no light bonus
    if 'SLOWNESS' in flags:
        dif_dec += 15
    if 'HUNGER' in flags:
        dif_dec += 3
    if 'FEAR' in flags:  # Not RES_FEAR!
        dif_dec += 5
    
    # Curse penalties
    if 'LIGHT_CURSE' in flags:
        dif_dec += 3
    if 'HEAVY_CURSE' in flags:
        dif_dec += 4
    if 'PERMA_CURSE' in flags:
        dif_dec += 8

    # === ABILITIES (granted abilities) ===
    # dif_inc += 5 + (level / 3) per ability
    # Uses actual ability levels from ability.txt
    ability_list = art.get('ability_list', [])
    for skill_num, ability_val in ability_list:
        level = get_ability_level(skill_num, ability_val)
        dif_inc += 5 + (level // 3)
    
    # Calculate base difficulty
    dif = dif_inc - dif_dec
    
    # === SLOT MULTIPLIER ===
    # Minor slots get +20% difficulty
    slot = get_slot(art['tval'])
    if slot in ['ring', 'light', 'cloak', 'gloves', 'boots', 'arrow']:
        dif_mult += 20

    # === ENCHANTABILITY BONUS ===
    # Items with ENCHANTABLE flag on base item OR artefact get -30% difficulty
    # Check both base item flags and artefact's own flags
    base_item_flags = get_base_flags(tval, sval)
    if 'ENCHANTABLE' in base_item_flags or 'ENCHANTABLE' in flags:
        dif_mult -= 30

    # Apply multiplier
    dif = c_trunc_div(dif * dif_mult, 100)
    
    # Artefact arrows are easier (halved) - only for actual artefacts, not ego arrows
    if art['tval'] == 17 and art['type'] == 'artefact':  # Artefact arrows only
        dif = c_trunc_div(dif, 2)

    return dif


def dif_mod_calc(value, positive_base):
    """
    Calculates difficulty modification using the triangular number formula.
    From drop_system.c drop_dif_mod():
        int mod = 1 + ((positive_base - 1) / 5);
        if (value > 0)
            *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
        else if (value < 0) {
            int abs_value = -value;
            int negative_base = (positive_base + 1) / 2;
            int negative_mod = 1 + ((negative_base - 1) / 5);
            *dif_inc -= negative_base * abs_value + negative_mod * (abs_value * (abs_value - 1) / 2);
        }
    """
    if value > 0:
        mod = 1 + ((positive_base - 1) // 5)
        return positive_base * value + mod * (value * (value - 1) // 2)
    elif value < 0:
        # Negative values decrease difficulty, but by half as much as positives increase it
        abs_value = -value
        negative_base = (positive_base + 1) // 2  # Half the positive base, rounded up
        negative_mod = 1 + ((negative_base - 1) // 5)
        return -(negative_base * abs_value + negative_mod * (abs_value * (abs_value - 1) // 2))
    else:
        return 0


def parse_alloc_pairs(alloc_str):
    """Parse an allocation string like '1/3:10/5:15/10' into [(depth, rarity), ...].

    Each colon-delimited segment is either 'depth/rarity' or just 'depth' (rarity defaults to 1).
    Matches the C parser logic in init-parse-ego.c and object.txt A: line docs.
    """
    allocs = []
    for part in alloc_str.split(':'):
        part = part.strip()
        if not part:
            continue
        if '/' in part:
            d_str, r_str = part.split('/', 1)
            try:
                depth = int(d_str.strip())
                rarity = int(r_str.strip())
                if rarity < 0:
                    rarity = 0
                allocs.append((depth, rarity))
            except ValueError:
                pass
        else:
            try:
                allocs.append((int(part), 1))
            except ValueError:
                pass
    return allocs


def schedule_min_depth_py(alloc_pairs, fallback):
    """Return the minimum depth in an allocation schedule (matches C schedule_min_depth)."""
    if not alloc_pairs:
        return fallback
    min_d = min(d for d, _r in alloc_pairs)
    return min_d if min_d > 0 else fallback


def schedule_max_depth_cap_py(alloc_pairs):
    """Return the max-depth cap from a schedule (matches C schedule_max_depth_cap).

    A trailing zero-rarity entry signals a depth cap: the item should not appear
    at depths *strictly greater than* the cap depth.
    Returns:
        0   -> no cap (item spawns at all depths from min onward)
        -1  -> never spawns (all rarities are zero)
        >0  -> the cap depth (item excluded when depth > cap)
    """
    if not alloc_pairs:
        return 0
    last_positive = -1
    for i, (_d, r) in enumerate(alloc_pairs):
        if r > 0:
            last_positive = i
    if last_positive < 0:
        return -1  # all zero rarities
    for i in range(last_positive + 1, len(alloc_pairs)):
        if alloc_pairs[i][1] == 0:
            return alloc_pairs[i][0]
    return 0  # no cap


def rarity_from_schedule_py(alloc_pairs, depth, default_rarity=1):
    """Return the rarity at *depth* from a schedule (matches C rarity_from_schedule).

    Trailing zero-rarity entries are treated as max-depth cap markers and are
    stripped before evaluating, exactly as the C implementation does.
    """
    if not alloc_pairs:
        return default_rarity
    pairs = list(alloc_pairs)
    # Strip trailing cap markers (rarity == 0) but keep at least one entry
    while len(pairs) > 1 and pairs[-1][1] == 0:
        pairs = pairs[:-1]
    rarity = pairs[0][1]
    for i in range(1, len(pairs)):
        if depth >= pairs[i][0]:
            rarity = pairs[i][1]
        else:
            break
    return rarity


def clean_obj_name(name):
    """Strip article/plural markers (& and ~) from object.txt names, then apply
    abbreviations for long base-item names so they fit display columns."""
    name = name.replace('&', '').replace('~', '').strip()
    _abbrevs = [
        ('Star-Iron Greatsword', 'Star-Iron GS'),
        ('Elven Mithril Sword',  'Elven Mithril Sw'),
        ('Mithril Greatsword',   'Mithril GS'),
        ('Mithril Longsword',    'Mithril LS'),
        ('Dragon-horn Bow',      'Dragon Bow'),
        ('Pair of Mithril Greaves', 'Mithril Greaves'),
        ('Pair of Greaves',      'Greaves'),
        ('Pair of Boots',        'Boots'),
        ('Set of Mithril Gauntlets', 'Mithril Gauntlets'),
        ('Set of Gauntlets',     'Gauntlets'),
        ('Set of Gloves',        'Gloves'),
    ]
    for long, short in _abbrevs:
        name = name.replace(long, short)
    return name


def format_rarity_schedule(schedule):
    """Format a rarity schedule as a compact string: '6:16000 16:3000'."""
    if not schedule:
        return "-"
    return " ".join(f"{d}:{r}" for d, r in schedule)


def compute_rarity_schedule(component_schedules, sched_min=1):
    """Compute rarity schedule for a NORMAL (non-ego) item from its own allocation schedule.

    component_schedules: list of (alloc_pairs, fallback_rarity) tuples (normally just one).
    sched_min: minimum depth to start from.

    Returns [(depth, rarity), ...] with consecutive identical rarities merged.
    """
    all_depths = set([sched_min])
    for alloc_pairs, _fb_r in component_schedules:
        for d, _r in alloc_pairs:
            if d >= sched_min:
                all_depths.add(d)

    result = []
    prev_rarity = None
    for d in sorted(all_depths):
        combined = 1
        all_positive = True
        for alloc_pairs, fb_r in component_schedules:
            r = rarity_from_schedule_py(alloc_pairs, d, fb_r)
            if r <= 0:
                all_positive = False
                break
            combined *= r
        if not all_positive:
            continue
        if combined != prev_rarity:
            result.append((d, combined))
            prev_rarity = combined

    return result


def compute_combined_rarity_schedule(base_pairs, ego_pairs_list, sched_min):
    """Compute rarity schedule for an ego item using the C formula: (base * ego) / 100.

    Mirrors combine_allocations() from drop-system-catalog.c which applies:
        combined = (base_r * ego_r) / 100
    for each ego schedule in sequence (applied left-to-right for dual egos).

    base_pairs: [(depth, rarity), ...] from the base item's A: schedule.
    ego_pairs_list: list of ([(depth, rarity), ...], fallback_rarity) tuples, one per ego.
    sched_min: minimum depth to include.

    Returns [(depth, combined_rarity), ...] with consecutive identical rarities merged.
    """
    all_depths = set([sched_min])
    for d, _ in base_pairs:
        if d >= sched_min:
            all_depths.add(d)
    for ego_pairs, _ in ego_pairs_list:
        for d, _ in ego_pairs:
            if d >= sched_min:
                all_depths.add(d)

    result = []
    prev_rarity = None
    for d in sorted(all_depths):
        combined = rarity_from_schedule_py(base_pairs, d, 1)
        if combined <= 0:
            continue
        for ego_pairs, ego_fallback in ego_pairs_list:
            ego_r = rarity_from_schedule_py(ego_pairs, d, ego_fallback)
            if ego_r <= 0:
                combined = 0
                break
            prev = combined
            combined = (combined * ego_r + 99) // 100
        if combined <= 0:
            continue
        if combined != prev_rarity:
            result.append((d, combined))
            prev_rarity = combined

    return result


def _more_special_bonus(rarity_percent):
    """Apply MORE_SPECIAL rarity bonus: +20 flat tier, capped at 255 (mirrors C)."""
    if rarity_percent <= 0:
        return 0
    return min(rarity_percent + 20, 255)


def _apply_more_special_to_allocs(alloc_pairs):
    """Return a new alloc list with each rarity boosted by MORE_SPECIAL factor."""
    return [(d, _more_special_bonus(r)) for d, r in alloc_pairs]


def _less_special_penalty(rarity_percent):
    """Apply LESS_SPECIAL rarity penalty: -20 flat tier, floored at 0 (mirrors C)."""
    if rarity_percent <= 0:
        return 0
    return max(rarity_percent - 20, 0)


def _apply_less_special_to_allocs(alloc_pairs):
    """Return a new alloc list with each rarity reduced by LESS_SPECIAL factor."""
    return [(d, _less_special_penalty(r)) for d, r in alloc_pairs]


def effective_special_schedule(base_obj, special):
    """Return (first_positive_depth, full_schedule, max_depth) for a base+ego combination.

    Uses the C formula: combined = (base_rarity * ego_rarity) / 100.
    Ego fallback rarity defaults to 100 (matching C: ego default = no modification).
    max_depth: 0 = no cap, >0 = depth beyond which item does not appear.
    """
    base_allocs = base_obj.get('alloc_pairs', [])
    ego_allocs = special.get('alloc_pairs', [])

    base_fallback = max(base_obj.get('level', 1), 1)
    ego_fb_depth = special.get('depth', 0)
    # C: ego default rarity is e_ptr->rarity if > 0, else 100 (100 = no modification to base)
    ego_fb_rarity = special.get('rarity', 0) or 100

    base_eff = base_allocs if base_allocs else [(base_fallback, 1)]
    ego_eff = ego_allocs if ego_allocs else [(max(ego_fb_depth, 1), ego_fb_rarity)]

    # MORE_SPECIAL: boost BASE item rarity by +20 (one tier) when generating ego combinations.
    # e.g. Dagger (85) -> 100, putting it in the same weight group as Spear/Shortsword.
    if 'MORE_SPECIAL' in base_obj.get('flags', []):
        base_eff = _apply_more_special_to_allocs(base_eff)

    # LESS_SPECIAL: reduce BASE item rarity by -20 (one tier) when generating ego combinations.
    if 'LESS_SPECIAL' in base_obj.get('flags', []):
        base_eff = _apply_less_special_to_allocs(base_eff)

    # max_depth: take the stricter (smaller) of the two caps; 0 means no cap
    base_cap = max(schedule_max_depth_cap_py(base_eff), 0)
    ego_cap  = max(schedule_max_depth_cap_py(ego_eff),  0)
    # Also honour the ego's hard max_level field (e_ptr->max_level in C)
    ego_max_level = special.get('max_level', 0) or 0
    caps = [c for c in [base_cap, ego_cap, ego_max_level] if c > 0]
    max_depth = min(caps) if caps else 0

    sched_min = max(
        schedule_min_depth_py(base_eff, base_fallback),
        schedule_min_depth_py(ego_eff, max(ego_fb_depth, 1)),
        1,
    )
    schedule = compute_combined_rarity_schedule(base_eff, [(ego_eff, ego_fb_rarity)], sched_min)
    if not schedule:
        schedule = [(sched_min, 0)]
    return schedule[0][0], schedule, max_depth


def effective_dual_ego_schedule(base_obj, prefix, suffix):
    """Return (first_positive_depth, full_schedule, max_depth) for a base+prefix+suffix.

    Applies C combine_allocations() twice:
        tmp   = combine(base, prefix)   -> (base * prefix) / 100
        final = combine(tmp, suffix)    -> (tmp * suffix) / 100
    Ego fallback rarities default to 100 (matching C code).
    max_depth: stricter of any depth caps across the three components.
    """
    base_allocs   = base_obj.get('alloc_pairs', [])
    prefix_allocs = prefix.get('alloc_pairs', [])
    suffix_allocs = suffix.get('alloc_pairs', [])

    b_fb   = max(base_obj.get('level', 1), 1)
    p_fb_d = max(prefix.get('depth', 0), 1)
    # C: ego default rarity 100 when unset (no modification)
    p_fb_r = prefix.get('rarity', 0) or 100
    s_fb_d = max(suffix.get('depth', 0), 1)
    s_fb_r = suffix.get('rarity', 0) or 100

    base_eff   = base_allocs   if base_allocs   else [(b_fb,   1)]
    prefix_eff = prefix_allocs if prefix_allocs else [(p_fb_d, p_fb_r)]
    suffix_eff = suffix_allocs if suffix_allocs else [(s_fb_d, s_fb_r)]

    # MORE_SPECIAL: boost BASE item rarity by +20 (one tier) when generating ego combinations.
    if 'MORE_SPECIAL' in base_obj.get('flags', []):
        base_eff = _apply_more_special_to_allocs(base_eff)

    # LESS_SPECIAL: reduce BASE item rarity by -20 (one tier) when generating ego combinations.
    if 'LESS_SPECIAL' in base_obj.get('flags', []):
        base_eff = _apply_less_special_to_allocs(base_eff)

    # max_depth: strictest cap across base and both egos
    base_cap   = max(schedule_max_depth_cap_py(base_eff),   0)
    prefix_cap = max(schedule_max_depth_cap_py(prefix_eff), 0)
    suffix_cap = max(schedule_max_depth_cap_py(suffix_eff), 0)
    prefix_max_level = prefix.get('max_level', 0) or 0
    suffix_max_level = suffix.get('max_level', 0) or 0
    caps = [c for c in [base_cap, prefix_cap, suffix_cap,
                         prefix_max_level, suffix_max_level] if c > 0]
    max_depth = min(caps) if caps else 0

    sched_min = max(
        schedule_min_depth_py(base_eff,   b_fb),
        schedule_min_depth_py(prefix_eff, p_fb_d),
        schedule_min_depth_py(suffix_eff, s_fb_d),
        1,
    )
    # Apply prefix, then suffix (matching C build_ego_combo_variants double combine)
    schedule = compute_combined_rarity_schedule(
        base_eff, [(prefix_eff, p_fb_r), (suffix_eff, s_fb_r)], sched_min
    )
    if not schedule:
        schedule = [(sched_min, 0)]
    return schedule[0][0], schedule, max_depth


def get_base_level(tval, sval):
    """Get base item level for difficulty calculation from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('level', 3)
    return 3


def get_base_ds(tval, sval):
    """Get base damage sides from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('ds', 0)
    return 0


def get_base_protection(tval, sval):
    """Get base protection value (ps+1)*pd from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        obj = OBJECTS_BY_TYPE[key]
        ps = obj.get('ps', 0)
        pd = obj.get('pd', 0)
        return (ps + 1) * pd if ps > 0 else 0
    return 0


def get_base_pd_max(obj):
    """Get the smithing/drop maximum protection dice for a base object."""
    if obj.get('tval') == 40 and obj.get('sval') == 15:
        return 2
    return obj.get('pd', 0)


def get_base_att(tval, sval):
    """Get base attack bonus from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('att', 0)
    return 0


def get_base_evn(tval, sval):
    """Get base evasion bonus from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('evn', 0)
    return 0


def get_base_pval(tval, sval):
    """Get base pval from parsed object data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('pval', 0)
    return 0


def smithing_step_from_ego_bonus_py(bonus):
    if bonus == 0:
        return 0
    return 1 if bonus > 0 else -1


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
                    'max_att': 0,
                    'max_ds': 0,
                    'max_evn': 0,
                    'max_ps': 0,
                    'max_pval': 0,
                    'level': 0,
                    'weight': 0,
                    'alloc_pairs': [],
                    'flags': [],
                }
            
            # Item info (contains tval:sval:pval)
            elif line.startswith('I:') and current:
                parts = line[2:].split(':')
                current['tval'] = int(parts[0])
                current['sval'] = int(parts[1])
                if len(parts) > 2 and parts[2]:
                    current['pval'] = int(parts[2])
                current['max_pval'] = current['pval']
            
            # Weight/level info (W:level:unused:weight:cost)
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['level'] = int(parts[0])
                if len(parts) > 2:
                    current['weight'] = int(parts[2])

            # Allocation schedule (A:depth/rarity:depth/rarity:...)
            elif line.startswith('A:') and current:
                current['alloc_pairs'] = parse_alloc_pairs(line[2:])

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

                # Default max values are base values unless overridden by R: lines.
                current['max_att'] = current['att']
                current['max_ds'] = current['ds']
                current['max_evn'] = current['evn']
                current['max_ps'] = current['ps']

            # Range maximums (R:ATT:4, R:DS:7, R:EVN:2, R:PS:3, R:PVAL:4)
            elif line.startswith('R:') and current:
                parts = line[2:].split(':')
                if len(parts) >= 2:
                    stat = parts[0].strip().upper()
                    value = int(parts[1].strip())
                    if stat == 'ATT':
                        current['max_att'] = value
                    elif stat == 'DS':
                        current['max_ds'] = value
                    elif stat == 'EVN':
                        current['max_evn'] = value
                    elif stat == 'PS':
                        current['max_ps'] = value
                    elif stat == 'PVAL':
                        current['max_pval'] = value
            
            # Flags
            elif line.startswith('F:') and current:
                flags = [f.strip() for f in line[2:].split('|')]
                current['flags'].extend(flags)
    
    if current and 'INSTA_ART' not in current['flags']:
        objects.append(current)

    return objects


def populate_objects_dict(objects):
    """Populate global OBJECTS_BY_TYPE dictionary from parsed objects."""
    global OBJECTS_BY_TYPE
    OBJECTS_BY_TYPE = {}
    for obj in objects:
        key = (obj['tval'], obj['sval'])
        OBJECTS_BY_TYPE[key] = obj


def resolve_artefact_name(art):
    """Return the full display name for an artefact, matching game logic.

    Names starting with ' are standalone (e.g. 'Ringil').
    All other names are suffixed onto the base item kind name
    (e.g. "Great Spear" + " of Melkor").
    """
    name = art['name']
    if name.startswith("'"):
        return name
    key = (art['tval'], art['sval'])
    base_obj = OBJECTS_BY_TYPE.get(key)
    if base_obj:
        base_name = clean_obj_name(base_obj['name'])
        return f"{base_name} {name}"
    return name


def get_base_weight(tval, sval):
    """Get base item weight from parsed object.txt data."""
    key = (tval, sval)
    if key in OBJECTS_BY_TYPE:
        return OBJECTS_BY_TYPE[key].get('weight', 0)
    return 0


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
            if has_alignment_conflict(obj.get('flags', []), special.get('flags', [])):
                continue
            
            # Calculate stat ranges for this special + base combination
            # (mirrors build_ego_variants() in drop_system.c)
            att_min = obj['att'] + smithing_step_from_ego_bonus_py(special['max_att'])
            att_max = obj['max_att'] + special['max_att']
            ds_min = obj['ds'] + smithing_step_from_ego_bonus_py(special['to_ds'])
            ds_max = obj['max_ds'] + special['to_ds']
            evn_min = obj['evn'] + smithing_step_from_ego_bonus_py(special['max_evn'])
            evn_max = obj['max_evn'] + special['max_evn']
            ps_min = obj['ps'] + smithing_step_from_ego_bonus_py(special['to_ps'])
            ps_max = obj['max_ps'] + special['to_ps']
            pval_min_inc = special.get('min_pval', 0) if special.get('min_pval', 0) > 0 else (1 if special['max_pval'] > 0 else 0)
            pval_min = obj['pval'] + pval_min_inc
            pval_max = obj['max_pval'] + special['max_pval']
            dd_min = obj['dd'] + smithing_step_from_ego_bonus_py(special['to_dd'])
            dd_max = obj['dd'] + special['to_dd']
            pd_min = obj['pd'] + smithing_step_from_ego_bonus_py(special['to_pd'])
            pd_max = get_base_pd_max(obj) + special['to_pd']
            
            # Check if pval is allowed (base has pval flags or pval != 0 or ego grants pval)
            base_flags = set(obj['flags'])
            pval_allowed = has_pval_mask(base_flags) or obj['pval'] != 0 or special['max_pval'] > 0

            if ds_min < 0:
                ds_min = 0
            if ds_max < 0:
                ds_max = 0
            if dd_min < 0:
                dd_min = 0
            if dd_max < 0:
                dd_max = 0
            if pd_min < 0:
                pd_min = 0
            if pd_max < 0:
                pd_max = 0
            if ps_min < 0:
                ps_min = 0
            if ps_max < 0:
                ps_max = 0
            
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

            # Compute full rarity schedule, effective depth, and max_depth cap
            eff_depth, eff_schedule, eff_max_depth = effective_special_schedule(obj, special)
            eff_rarity = eff_schedule[0][1] if eff_schedule else 0
            _is_prefix = special.get('is_prefix', False)
            _obj_name  = clean_obj_name(obj['name'])
            _ego_name  = special['name']
            _variant_name = f"{_ego_name} {_obj_name}" if _is_prefix else f"{_obj_name} {_ego_name}"

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
                                            'name': _variant_name,
                                            'base_name': _obj_name,
                                            'special_name': _ego_name,
                                            'is_prefix': _is_prefix,
                                            'tval': obj['tval'],
                                            'sval': obj['sval'],
                                            'att': att,
                                            'ds': ds,
                                            'dd': dd,
                                            'evn': evn,
                                            'ps': ps,
                                            'pd': pd,
                                            'pval': pval,
                                            'ability_list': special['ability_list'][:],  # Copy ability list
                                            'flags': obj['flags'][:] + [f for f in special['flags'] if f not in obj['flags']],  # Combine base + ego flags
                                            'depth': eff_depth,
                                            'rarity': eff_rarity,
                                            'rarity_schedule': eff_schedule,
                                            'max_depth': eff_max_depth,
                                            # Store base stats for difficulty calculation
                                            'base_att': obj['att'],
                                            'base_evn': obj['evn'],
                                            'base_ds': obj['ds'],
                                            'base_ps': obj['ps'],
                                            'base_pd': obj['pd'],
                                            'base_dd': obj['dd'],
                                            'base_pval': obj['pval'],
                                            'base_level': obj['level'],
                                            'weight': obj.get('weight', 0),  # Inherit base item weight
                                        }
                                        variants.append(variant)
    
    return variants


def generate_dual_ego_variants(specials_raw, objects):
    """
    Generate variants where items have BOTH a prefix AND a suffix ego.
    This matches the modern game mechanic where items can have dual egos.

    For example: (Protective) Leather Armour of Brethil
    """
    variants = []

    # Separate prefix and suffix specials
    prefix_specials = [s for s in specials_raw if s.get('is_prefix', False)]
    suffix_specials = [s for s in specials_raw if not s.get('is_prefix', False)]

    # For each prefix+suffix combination
    for prefix in prefix_specials:
        for suffix in suffix_specials:
            if suffix_forbids_prefix_combo(suffix):
                continue
            if has_alignment_conflict(prefix.get('flags', []), suffix.get('flags', [])):
                continue

            # Find base items that can have BOTH this prefix AND this suffix
            for obj in objects:
                # Skip supplies and lights (TV_POTION=75, TV_STAFF=70, TV_GEM=56, TV_FOOD=80, TV_FLASK=77)
                if obj['tval'] in [75, 70, 56, 80, 77, 39]:
                    continue

                # Check if prefix applies to this base item
                prefix_applies = False
                for t_idx in range(len(prefix['tvals'])):
                    if obj['tval'] == prefix['tvals'][t_idx]:
                        if prefix['min_svals'][t_idx] <= obj['sval'] <= prefix['max_svals'][t_idx]:
                            prefix_applies = True
                            break

                if not prefix_applies:
                    continue

                # Check if suffix applies to this base item
                suffix_applies = False
                for t_idx in range(len(suffix['tvals'])):
                    if obj['tval'] == suffix['tvals'][t_idx]:
                        if suffix['min_svals'][t_idx] <= obj['sval'] <= suffix['max_svals'][t_idx]:
                            suffix_applies = True
                            break

                if not suffix_applies:
                    continue
                if has_alignment_conflict(
                    obj.get('flags', []), prefix.get('flags', []), suffix.get('flags', [])
                ):
                    continue

                # Compute full rarity schedule for this three-way combination
                combined_depth, combined_schedule, combined_max_depth = effective_dual_ego_schedule(obj, prefix, suffix)
                combined_rarity = combined_schedule[0][1] if combined_schedule else 0
                _obj_name_d = clean_obj_name(obj['name'])

                # Range math mirrors src/drop_system.c: build_ego_combo_variants().
                att_min = obj['att'] \
                    + smithing_step_from_ego_bonus_py(prefix['max_att']) \
                    + smithing_step_from_ego_bonus_py(suffix['max_att'])
                att_max = obj['max_att'] + prefix['max_att'] + suffix['max_att']
                ds_min = obj['ds'] \
                    + smithing_step_from_ego_bonus_py(prefix['to_ds']) \
                    + smithing_step_from_ego_bonus_py(suffix['to_ds'])
                ds_max = obj['max_ds'] + prefix['to_ds'] + suffix['to_ds']
                evn_min = obj['evn'] \
                    + smithing_step_from_ego_bonus_py(prefix['max_evn']) \
                    + smithing_step_from_ego_bonus_py(suffix['max_evn'])
                evn_max = obj['max_evn'] + prefix['max_evn'] + suffix['max_evn']
                ps_min = obj['ps'] \
                    + smithing_step_from_ego_bonus_py(prefix['to_ps']) \
                    + smithing_step_from_ego_bonus_py(suffix['to_ps'])
                ps_max = obj['max_ps'] + prefix['to_ps'] + suffix['to_ps']
                dd_min = obj['dd'] \
                    + smithing_step_from_ego_bonus_py(prefix['to_dd']) \
                    + smithing_step_from_ego_bonus_py(suffix['to_dd'])
                dd_max = obj['dd'] + prefix['to_dd'] + suffix['to_dd']
                pd_min = obj['pd'] \
                    + smithing_step_from_ego_bonus_py(prefix['to_pd']) \
                    + smithing_step_from_ego_bonus_py(suffix['to_pd'])
                pd_max = get_base_pd_max(obj) + prefix['to_pd'] + suffix['to_pd']

                prefix_pval_min = (
                    prefix.get('min_pval', 0)
                    if prefix.get('min_pval', 0) > 0
                    else (1 if prefix['max_pval'] > 0 else 0)
                )
                suffix_pval_min = (
                    suffix.get('min_pval', 0)
                    if suffix.get('min_pval', 0) > 0
                    else (1 if suffix['max_pval'] > 0 else 0)
                )
                pval_min = obj['pval'] + prefix_pval_min + suffix_pval_min
                pval_max = obj['max_pval'] + prefix['max_pval'] + suffix['max_pval']
                pval_allowed = (
                    has_pval_mask(set(obj.get('flags', [])))
                    or obj['pval'] != 0
                    or prefix['max_pval'] > 0
                    or suffix['max_pval'] > 0
                )

                if ds_min < 0:
                    ds_min = 0
                if ds_max < 0:
                    ds_max = 0
                if dd_min < 0:
                    dd_min = 0
                if dd_max < 0:
                    dd_max = 0
                if pd_min < 0:
                    pd_min = 0
                if pd_max < 0:
                    pd_max = 0
                if ps_min < 0:
                    ps_min = 0
                if ps_max < 0:
                    ps_max = 0

                if att_min > att_max:
                    att_min = att_max
                if ds_min > ds_max:
                    ds_min = ds_max
                if evn_min > evn_max:
                    evn_min = evn_max
                if ps_min > ps_max:
                    ps_min = ps_max
                if dd_min > dd_max:
                    dd_min = dd_max
                if pd_min > pd_max:
                    pd_min = pd_max
                if pval_min > pval_max:
                    pval_min = pval_max

                # Combined flags and abilities.
                combined_flags = prefix['flags'][:] + [f for f in suffix['flags'] if f not in prefix['flags']]
                combined_ability_list = prefix['ability_list'][:] + suffix['ability_list'][:]

                for att in range(att_min, att_max + 1):
                    for ds in range(ds_min, ds_max + 1):
                        for evn in range(evn_min, evn_max + 1):
                            for ps in range(ps_min, ps_max + 1):
                                for dd in range(dd_min, dd_max + 1):
                                    for pd in range(pd_min, pd_max + 1):
                                        pval_hi = pval_max if pval_allowed else pval_min
                                        for pval in range(pval_min, pval_hi + 1):
                                            variant = {
                                                'type': 'dual_ego',
                                                'prefix_idx': prefix['idx'],
                                                'suffix_idx': suffix['idx'],
                                                'name': f"{prefix['name']} {_obj_name_d} {suffix['name']}",
                                                'base_name': _obj_name_d,
                                                'prefix_name': prefix['name'],
                                                'suffix_name': suffix['name'],
                                                'is_prefix': False,  # dual ego is neither pure prefix nor suffix
                                                'tval': obj['tval'],
                                                'sval': obj['sval'],
                                                'att': att,
                                                'ds': ds,
                                                'dd': dd,
                                                'evn': evn,
                                                'ps': ps,
                                                'pd': pd,
                                                'pval': pval,
                                                'ability_list': combined_ability_list,
                                                'flags': combined_flags,
                                                'depth': combined_depth,
                                                'rarity': combined_rarity,
                                                'rarity_schedule': combined_schedule,
                                                'max_depth': combined_max_depth,
                                                # Store base stats for difficulty calculation
                                                'base_att': obj['att'],
                                                'base_evn': obj['evn'],
                                                'base_ds': obj['ds'],
                                                'base_ps': obj['ps'],
                                                'base_pd': obj['pd'],
                                                'base_dd': obj['dd'],
                                                'base_pval': obj['pval'],
                                                'base_level': obj['level'],
                                                'weight': obj.get('weight', 0),  # Inherit base item weight
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
        # Items with ENCHANTABLE flag (for -30% difficulty multiplier)
        (36, 2): {'ENCHANTABLE'},  # Robe
        (21, 3): {'ENCHANTABLE'},  # Quarterstaff
        (30, 3): {'ENCHANTABLE', 'STAND_FAST'},  # Mithril Greaves
        (31, 3): {'ENCHANTABLE', 'REGEN'},  # Mithril Gauntlets
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
        # TV_POTION=75, TV_STAFF=70, TV_GEM=56, TV_FOOD=80, TV_FLASK=77
        if obj['tval'] in [75, 70, 56, 80, 77]:
            continue
        
        # Skip lights (handled as normal variants via light category logic)
        if obj['tval'] == 39:  # Light
            continue
            
        # Determine if pval is allowed
        base_flags = set(obj['flags'])
        pval_allowed = has_pval_mask(base_flags) or obj['pval'] != 0
        
        # Set up smithing caps based on item type
        att_min = obj['att']
        att_max = obj['max_att']
        ds_min = obj['ds']
        ds_max = obj['max_ds']
        evn_min = obj['evn']
        evn_max = obj['max_evn']
        ps_min = obj['ps']
        ps_max = obj['max_ps']
        pval_min = obj['pval']
        pval_max = obj['max_pval']
        pd_min = obj['pd']
        pd_max = get_base_pd_max(obj)
        dd_min = obj['dd']
        dd_max = obj['dd']
        
        # Compute effective depth, full rarity schedule, and max-depth cap
        _base_allocs = obj.get('alloc_pairs', []) or [(max(obj['level'], 1), 1)]
        _base_sched_min = max(schedule_min_depth_py(_base_allocs, max(obj['level'], 1)), 1)
        _norm_schedule = compute_rarity_schedule([(_base_allocs, 1)], _base_sched_min)
        if not _norm_schedule:
            _norm_schedule = [(max(obj['level'], 1), 1)]
        _eff_depth  = _norm_schedule[0][0]
        _eff_rarity = _norm_schedule[0][1]
        _obj_name   = clean_obj_name(obj['name'])
        # max_depth: 0 = no cap, >0 = hard cap from A:.../0 trailing marker
        _max_depth_cap = schedule_max_depth_cap_py(_base_allocs)
        _max_depth = max(_max_depth_cap, 0)  # -1 (never) treated as 0; filter by weight instead

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
                                        'name': _obj_name,
                                        'tval': obj['tval'],
                                        'sval': obj['sval'],
                                        'att': att,
                                        'ds': ds,
                                        'dd': dd,
                                        'evn': evn,
                                        'ps': ps,
                                        'pd': pd,
                                        'pval': pval,
                                        'ability_list': [],
                                        'flags': obj['flags'][:],
                                        'depth': _eff_depth,
                                        'rarity': _eff_rarity,
                                        'rarity_schedule': _norm_schedule,
                                        'max_depth': _max_depth,
                                        # Store base stats for difficulty calculation
                                        'base_att': obj['att'],
                                        'base_evn': obj['evn'],
                                        'base_ds': obj['ds'],
                                        'base_ps': obj['ps'],
                                        'base_pd': obj['pd'],
                                        'base_dd': obj['dd'],
                                        'base_pval': obj['pval'],
                                        'base_level': obj['level'],
                                        'weight': obj.get('weight', 0),  # Base item weight
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


def main(argv=None):
    import argparse
    
    parser = argparse.ArgumentParser(description='Calculate artefact/special smithing difficulty')
    parser.add_argument('--csv', '-c', metavar='FILE',
                        help='Export results to CSV file')
    parser.add_argument('--artefacts-only', '-a', action='store_true',
                        help='Show only artefacts in output')
    args = parser.parse_args(argv)
    
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

    # Find ability.txt for ability levels
    ability_paths = [
        os.path.join(script_dir, '..', 'lib', 'edit', 'ability.txt'),
        os.path.join(script_dir, 'lib', 'edit', 'ability.txt'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit\ability.txt',
    ]

    ability_file = None
    for path in ability_paths:
        if os.path.exists(path):
            ability_file = path
            break

    # Parse ability.txt first to populate ABILITY_LEVELS
    if ability_file:
        parse_ability_file(ability_file)
        print(f"Loaded {len(ABILITY_LEVELS)} ability levels from ability.txt")

    artefacts, sval_order = parse_artefact_file(artefact_file)
    specials_raw = parse_special_file(special_file) if special_file else []
    objects = parse_object_file(object_file) if object_file else []

    # Populate global object lookup for weight calculations
    if objects:
        populate_objects_dict(objects)
        print(f"Loaded {len(OBJECTS_BY_TYPE)} base item types for weight lookup")

    # Filter out "Ultimate" template artefacts (idx 182-198) and Morgoth Crown (idx 175-178)
    artefacts = [a for a in artefacts if not (
        (a['name'].startswith("'Ultimate") and a['idx'] >= 182) or
        a['idx'] in [175, 176, 177, 178]  # Morgoth Crown variants (with 0, 1, 2, 3 Silmarils)
    )]
    
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

    # Generate dual-ego variants (prefix + suffix combinations)
    print(f"\nGenerating dual-ego variants (prefix + suffix combinations)...")
    prefix_count = len([s for s in specials_raw if s.get('is_prefix', False)])
    suffix_count = len(specials_raw) - prefix_count
    print(f"  Found {prefix_count} prefix egos and {suffix_count} suffix egos")
    dual_egos = generate_dual_ego_variants(specials_raw, objects)
    print(f"Total dual-ego variants: {len(dual_egos)}")

    print(f"\nTotal artefacts: {len(artefacts)}")

    # Calculate difficulties
    all_items = []
    for art in artefacts:
        art['type'] = 'artefact'
        art['rarity_schedule'] = [(art['depth'], art['rarity'])]
        art['difficulty'] = calculate_difficulty(art)
        all_items.append(art)

    for norm in normals:
        norm['difficulty'] = calculate_difficulty(norm)
        all_items.append(norm)

    for spec in specials:
        spec['difficulty'] = calculate_difficulty(spec)
        all_items.append(spec)

    for dual in dual_egos:
        dual['difficulty'] = calculate_difficulty(dual)
        all_items.append(dual)

    # Filter to artefacts only if requested
    if args.artefacts_only:
        all_items = [item for item in all_items if item['type'] == 'artefact']
        normals = []
        specials = []
        dual_egos = []
        print(f"\n[--artefacts-only mode: showing only artefacts]")

    # CSV export mode
    if args.csv:
        count = export_csv(all_items, args.csv)
        print(f"\nExported {count} items to {args.csv}")
        print(f"  Artefacts: {len(artefacts)}")
        print(f"  Normal variants: {len(normals)}")
        print(f"  Special variants: {len(specials)}")
        print(f"  Dual-ego variants: {len(dual_egos)}")
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
        dual_count = len([d for d in dual_egos if low <= d['difficulty'] <= high])
        print(f"  {label}: {art_count} artefacts, {norm_count} normals, {spec_count} specials, {dual_count} dual-ego")
    print()
    
    # Full listing sorted by difficulty
    print("FULL ITEM LIST (by Smithing Difficulty):")
    print("-" * 130)
    print(f"{'Diff':>4} {'Lvl':>3} {'Rarity Schedule':<26} {'Type':<14} {'Name':<42} Stats")
    print("-" * 130)

    # Separator lines derived from the real drop formula (drop-system-selection.c):
    #   sides      = 25 + (3 * depth) // 4
    #   max_target = int(1.25 * depth) - 19 + sides + quality_bonus
    #   upper_band = max_target + 2          <- items above this never drop here
    # DROP_BONUS_SUPERB = 15  (drop_system.h)
    # Threshold = max_upper + 1 (separator fires when difficulty drops to <= max_upper).
    # Depths 20/23/25, normal (bonus=0) and superb (bonus=15), descending:
    def _drop_max_upper(depth, quality_bonus=0):
        sides = 25 + (3 * depth) // 4
        return int(1.25 * depth) - 19 + sides + quality_bonus + 2
    _smi_thresholds = [
        (_drop_max_upper(25, 15) + 1, f"--- Max drop depth 25 superb  (max diff {_drop_max_upper(25, 15)}) " + "-" * 80),
        (_drop_max_upper(23, 15) + 1, f"--- Max drop depth 23 superb  (max diff {_drop_max_upper(23, 15)}) " + "-" * 80),
        (_drop_max_upper(20, 15) + 1, f"--- Max drop depth 20 superb  (max diff {_drop_max_upper(20, 15)}) " + "-" * 80),
        (_drop_max_upper(25)     + 1, f"--- Max drop depth 25 normal  (max diff {_drop_max_upper(25)}) "     + "-" * 80),
        (_drop_max_upper(23)     + 1, f"--- Max drop depth 23 normal  (max diff {_drop_max_upper(23)}) "     + "-" * 80),
        (_drop_max_upper(20)     + 1, f"--- Max drop depth 20 normal  (max diff {_drop_max_upper(20)}) "     + "-" * 80),
    ]
    _smi_thresholds_iter = iter(_smi_thresholds)
    _next_threshold, _next_sep = next(_smi_thresholds_iter, (None, None))

    for item in items_by_diff:
        tval_name = get_tval_name(item['tval'])
        if item['type'] == 'artefact':
            item_type = 'ART'
        elif item['type'] == 'special':
            item_type = 'SPC'
        elif item['type'] == 'dual_ego':
            item_type = 'DUO'
        else:
            item_type = 'NRM'
        tval_name = f"{item_type}:{tval_name}"
        stats = []
        if item['att'] != 0 or item['dd'] > 0:
            stats.append(f"+{item['att']},{item['dd']}d{item['ds']}")
        if item['evn'] != 0 or item['pd'] > 0:
            stats.append(f"+{item['evn']},{item['pd']}d{item['ps']}")
        sb = item.get('stat_bonus', {})
        kb = item.get('skill_bonus', {})
        for key in STAT_TOKENS:
            v = sb.get(key, 0)
            if v != 0:
                stats.append(f"{key}{v:+d}")
        skill_abbr = {
            'ARCHERY': 'ARC',
            'STEALTH': 'STL',
            'PERCEPTION': 'PER',
            'WILL': 'WIL',
            'SMITHING': 'SMT',
            'SONG': 'SNG',
        }
        for key in SKILL_TOKENS:
            v = kb.get(key, 0)
            if v != 0:
                stats.append(f"{skill_abbr.get(key, key)}{v:+d}")
        if item['pval'] != 0:
            stats.append(f"pval:{item['pval']}")
        ability_count = len(item.get('ability_list', []))
        if ability_count > 0:
            stats.append(f"abil:{ability_count}")
        stats_str = ' '.join(stats)

        rar_str = format_rarity_schedule(item.get('rarity_schedule', [(item['depth'], item.get('rarity', 0))]))
        if item['difficulty'] < 10:
            continue
        # Print any threshold separators we've crossed (list sorted descending)
        while _next_threshold is not None and item['difficulty'] < _next_threshold:
            print(f"\033[90m{_next_sep}\033[0m")
            _next_threshold, _next_sep = next(_smi_thresholds_iter, (None, None))
        line = f"{item['difficulty']:>4} {item['depth']:>3} {rar_str:<26} {tval_name:<14} {item['name']:<42} {stats_str}"
        if item['type'] == 'artefact':
            line = f"\033[93m{line}\033[0m"  # bright yellow
        print(line)
    
    print()
    print("=" * 120)
    print()
    
    # Group by item type (tval) and subtype (sval)
    print("ARTEFACTS BY TYPE:")
    print("-" * 120)

    # Filter out sval 50 (special/placeholder items)
    filtered_artefacts = [art for art in artefacts if art['sval'] != 50]

    # Group by (tval, sval)
    by_tval_sval = {}
    for art in filtered_artefacts:
        key = (art['tval'], art['sval'])
        if key not in by_tval_sval:
            by_tval_sval[key] = []
        by_tval_sval[key].append(art)

    # Also group by tval for section headers
    by_tval = {}
    for (tval, sval), arts in by_tval_sval.items():
        if tval not in by_tval:
            by_tval[tval] = []
        by_tval[tval].append((sval, arts))

    # Sort by the order (tval, sval) first appeared in artefact.txt
    def get_tval_order(tval_key):
        sval_arts_list = by_tval[tval_key]
        if sval_arts_list:
            min_order = min(sval_order.get((tval_key, sval), float('inf')) for sval, _ in sval_arts_list)
            return min_order
        return float('inf')

    def get_sval_order(tval, sval):
        return sval_order.get((tval, sval), float('inf'))

    for tval in sorted(by_tval.keys(), key=get_tval_order):
        tval_name = get_tval_name(tval)
        total_arts = sum(len(arts) for _, arts in by_tval[tval])
        all_tval_arts = [art for _, arts in by_tval[tval] for art in arts]
        avg_diff = sum(a['difficulty'] for a in all_tval_arts) / len(all_tval_arts)
        print(f"\n{tval_name} ({total_arts} artefacts, avg diff: {avg_diff:.1f}):")

        # For Rings and Amulets, don't group by sval - just list all artefacts
        if tval in [45, 40]:  # Ring or Amulet
            arts_sorted = sorted(all_tval_arts, key=lambda x: x['difficulty'], reverse=True)
            for art in arts_sorted:
                rarity = art.get('rarity', 0)
                print(f"  {art['difficulty']:>3}: {art['name']:<28} (Lvl:{art['depth']:>2} Rar:{rarity:>3})")
        else:
            # Sort svals by their order in artefact.txt
            for sval, arts in sorted(by_tval[tval], key=lambda x: get_sval_order(tval, x[0])):
                # Get base item name from OBJECTS_BY_TYPE, strip formatting chars (& and ~)
                base_name = OBJECTS_BY_TYPE.get((tval, sval), {}).get('name', f'sval {sval}')
                base_name = base_name.replace('&', '').replace('~', '').strip()
                arts_sorted = sorted(arts, key=lambda x: x['difficulty'], reverse=True)
                sval_avg = sum(a['difficulty'] for a in arts) / len(arts)
                print(f"  {base_name} ({len(arts)}, avg: {sval_avg:.1f}):")
                for art in arts_sorted:
                    rarity = art.get('rarity', 0)
                    print(f"    {art['difficulty']:>3}: {art['name']:<28} (Lvl:{art['depth']:>2} Rar:{rarity:>3})")
    
    print()
    print("=" * 120)
    print("DIFFICULTY FORMULA (from src/cmd4.c object_difficulty()):")
    print("  - Base item level / 2")
    print("  - Attack bonus: weapons +3/point, others +6/point; negatives reduce at half rate")
    print("  - Evasion bonus: +6/point armor, +9/point others; negatives reduce at half rate")
    print("  - Damage sides: +3*x+2 per extra side (triangular); negatives reduce at half rate")
    print("  - Protection: +3/point; amulet/hauberk flat offsets only apply on positive bonuses")
    print("  - Slays: +3-5 each (Spider/Rauko/Dragon +4, Man/Elf +5, others +3)")
    print("  - Brands: Cold +18, Fire +14, Poison +16 (+20 per extra brand)")
    print("  - Sharpness: +24 (arrows +14), Sharpness2: +40")
    print("  - Stats (STR/DEX/CON/GRA): +14 per stat bonus")
    print("  - Skills: Perception/Will +3 per skill bonus, Stealth/Song/Archery/Smithing +4 per skill bonus")
    print("  - Extra multi-bonus penalty: +7 per extra stat, +3 per extra skill")
    print("  - Sustains: +2 each")
    print("  - Resistances: Fire/Cold/Poison +5, Fear/Blind/Confu/Stun +2, Bleed/Hallu +1")
    print("  - Free Action: +7, Light: +8, Radiance: +6, Regen: +4, See Invis: +4")
    print("  - Speed: +40, Cheat Death: +13, Accurate: +15, Vampiric: +6")
    print("  - Abilities: +6 each (5 + level/3)")
    print("  - Minor slots (ring/cloak/gloves/boots/light/arrow): +20% multiplier")
    print("  - Artefact arrows: difficulty halved")
    print("=" * 120)
    print()


if __name__ == '__main__':
    # Enable ANSI colour codes on Windows
    if sys.platform == 'win32':
        import ctypes
        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    # Capture arguments on start to preserve them when rerunning
    # Defaults to sys.argv[1:] if we pass None, but capturing explicit list is safer for reruns
    current_argv = sys.argv[1:]
    while True:
        main(current_argv)
        try:
            # Flush to ensure prompt appears before waiting for input
            sys.stdout.flush()
            response = input("\nPress 'r' to rerun or Enter to exit: ").strip().lower()
            if response != 'r':
                break
            # Clear screen before rerunning (works in most terminals; IDLE will just print separator)
            if sys.platform == 'win32':
                os.system('cls')
            else:
                os.system('clear')
            print("\n" + "=" * 120)
            print("RELOADING FILES...")
            print("=" * 120 + "\n")
        except (EOFError, KeyboardInterrupt, OSError):
            break
