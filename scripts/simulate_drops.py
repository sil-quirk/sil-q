#!/usr/bin/env python3
"""
Simulate the new item drop system for Sil-More.

New Drop Logic:
1. Build a table of ALL possible items with their difficulties
   - Normal items with all stat variations (+1 damage, +2 damage, +d1, etc.)
   - Special items (egoes) with all applicable base items and stat variations
   - Artefacts
   
2. When dropping an item:
   - Roll: 1.8 * Current_Depth + min(1d30, 1d30)
   - Add bonuses: +5 for good, +10 for great
   - Create difficulty band: target +/- 2
   
3. Filter items:
   - Apply depth/monster/place restrictions
   - Apply depth penalty: difficulty += 2 * (min_depth - current_depth) if current_depth < min_depth
   
4. Group items:
   - Artefacts: each in its own group
   - Specials: grouped by ego (e.g., all "of Falas" items together)
   - Normal items: grouped by base item type (all shortswords together, etc.)
   
5. Rarity roll using cumulative formula:
   - Normal items: rarity 1
   - Specials: use rarity from special.txt
   - Artefacts: use rarity from artefact.txt
   
6. Random selection within chosen group
"""

import os
import sys
import random
from collections import defaultdict
from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional

# Import from existing scripts
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)

from calc_artefact_difficulty import (
    parse_artefact_file, dif_mod_calc,
    get_base_level, get_base_ds, get_base_protection, get_base_att, get_base_evn,
    get_base_pval, get_slot, get_tval_name, calculate_difficulty
)

from calc_difficulty_distribution import (
    parse_full_special_file, special_applies_to_item,
    get_base_ps, get_base_pd, SMITHABLE_BASE_ITEMS
)


@dataclass
class ItemVariant:
    """Represents a specific item configuration."""
    name: str
    group_name: str  # For grouping (base item name, ego name, or artefact name)
    group_type: str  # 'normal', 'special', 'artefact'
    difficulty: int
    min_depth: int
    max_depth: int  # 0 means no max
    rarity: int
    tval: int
    sval: int
    # Item stats
    att_bonus: int = 0
    ds_bonus: int = 0
    evn_bonus: int = 0
    ps_bonus: int = 0
    pval: int = 0
    flags: List[str] = field(default_factory=list)
    special_name: str = ""  # Name of the ego if applicable
    
    def full_description(self) -> str:
        """Generate a full item description."""
        parts = []
        
        # Base name
        base = self.name
        
        # Add stats
        stats = []
        if self.att_bonus > 0:
            stats.append(f"+{self.att_bonus} att")
        elif self.att_bonus < 0:
            stats.append(f"{self.att_bonus} att")
        if self.ds_bonus > 0:
            stats.append(f"+{self.ds_bonus}d")
        if self.evn_bonus > 0:
            stats.append(f"+{self.evn_bonus} evn")
        elif self.evn_bonus < 0:
            stats.append(f"{self.evn_bonus} evn")
        if self.ps_bonus > 0:
            stats.append(f"+{self.ps_bonus}p")
        if self.pval > 0:
            stats.append(f"+{self.pval} pval")
        
        if stats:
            return f"{base} ({', '.join(stats)})"
        return base


def build_item_table(artefact_file: str, special_file: str) -> List[ItemVariant]:
    """Build the complete table of all possible items with difficulties."""
    items = []
    
    # Parse data files
    artefacts = parse_artefact_file(artefact_file)
    specials = parse_full_special_file(special_file)
    
    # Filter out template artefacts
    artefacts = [a for a in artefacts if not (a['name'].startswith("'Ultimate") and a['idx'] >= 182)]
    
    # 1. Add artefacts (each is its own group)
    for art in artefacts:
        art['type'] = 'artefact'
        diff = calculate_difficulty(art)
        items.append(ItemVariant(
            name=art['name'],
            group_name=art['name'],
            group_type='artefact',
            difficulty=diff,
            min_depth=art['depth'],
            max_depth=0,  # Artefacts have no max depth
            rarity=art['rarity'],
            tval=art['tval'],
            sval=art['sval'],
            att_bonus=art.get('att', 0),
            ds_bonus=art.get('ds', 0) - get_base_ds(art['tval'], art['sval']),
            evn_bonus=art.get('evn', 0),
            ps_bonus=art.get('ps', 0),
            pval=art.get('pval', 0),
            flags=art.get('flags', [])
        ))
    
    # 2. Add normal items with stat variations
    for tval, sval, name in SMITHABLE_BASE_ITEMS:
        base_att = get_base_att(tval, sval)
        base_evn = get_base_evn(tval, sval)
        base_ds = get_base_ds(tval, sval)
        base_ps = get_base_ps(tval, sval)
        base_level = get_base_level(tval, sval)
        
        # Determine possible variations based on item type
        att_range = [0]
        ds_range = [0]
        evn_range = [0]
        ps_range = [0]
        
        # Weapons can get attack bonus and damage bonus
        if tval in [19, 23, 22, 21, 20]:  # Bows, Swords, Polearms, Hafted, Digging
            att_range = [0, 1]
            ds_range = [0, 1]
        
        # Arrows can get attack bonus
        if tval == 17:
            att_range = [0, 1, 2]
        
        # Armor can get evasion bonus and protection bonus
        if tval in [30, 31, 32, 34, 36, 37]:  # Boots, Gloves, Helm, Shield, Soft Armor, Mail
            evn_range = [0, 1]
            if base_ps > 0:
                ps_range = [0, 1]
        
        # Cloaks get evasion only
        if tval == 35:
            evn_range = [0, 1]
            ps_range = [0]
        
        # Generate all combinations
        for att_bonus in att_range:
            for ds_bonus in ds_range:
                for evn_bonus in evn_range:
                    for ps_bonus in ps_range:
                        diff = calculate_normal_item_difficulty(
                            tval, sval, att_bonus, ds_bonus, evn_bonus, ps_bonus
                        )
                        items.append(ItemVariant(
                            name=name,
                            group_name=name,
                            group_type='normal',
                            difficulty=diff,
                            min_depth=base_level,
                            max_depth=0,
                            rarity=1,
                            tval=tval,
                            sval=sval,
                            att_bonus=att_bonus,
                            ds_bonus=ds_bonus,
                            evn_bonus=evn_bonus,
                            ps_bonus=ps_bonus
                        ))
    
    # 3. Add special items (ego items)
    for special in specials:
        # Skip cursed/negative specials for now (those with VUL_, AGGRAVATE only, etc.)
        if is_negative_ego(special):
            continue
            
        special_rarity = special.get('depth', 0)  # W: line first field is actually depth
        # Get rarity from W: line - need to re-parse
        ego_depth = special.get('depth', 0)
        ego_rarity = get_ego_rarity(special)
        ego_max_depth = get_ego_max_depth(special)
        
        # For each base item this ego can apply to
        for tval, sval, base_name in SMITHABLE_BASE_ITEMS:
            if not special_applies_to_item(special, tval, sval):
                continue
            
            base_att = get_base_att(tval, sval)
            base_evn = get_base_evn(tval, sval)
            base_ds = get_base_ds(tval, sval)
            base_ps = get_base_ps(tval, sval)
            base_level = get_base_level(tval, sval)
            base_pval = get_base_pval(tval, sval)
            
            # Ego bonuses
            ego_att = 1 if special.get('max_att', 0) > 0 else 0
            ego_ds = 1 if special.get('to_ds', 0) > 0 else 0
            ego_evn = 1 if special.get('max_evn', 0) > 0 else 0
            ego_ps = 1 if special.get('to_ps', 0) > 0 else 0
            ego_pval = 1 if special.get('max_pval', 0) > 0 else 0
            
            # Create item name
            ego_name = special['name']
            item_name = f"{base_name} {ego_name}"
            
            # Generate variations (base stats + ego bonuses + possible extra bonuses)
            # For simplicity, we'll generate: base, base+1att, base+1ds, etc.
            att_variations = [ego_att]
            ds_variations = [ego_ds]
            evn_variations = [ego_evn]
            ps_variations = [ego_ps]
            
            # Allow +1 more on top of ego bonus for weapons/armor
            if tval in [19, 23, 22, 21, 20]:  # Weapons
                att_variations = [ego_att, ego_att + 1] if ego_att < 2 else [ego_att]
                ds_variations = [ego_ds, ego_ds + 1] if ego_ds < 2 else [ego_ds]
            if tval in [30, 31, 32, 34, 36, 37]:  # Armor
                evn_variations = [ego_evn, ego_evn + 1] if ego_evn < 2 else [ego_evn]
                ps_variations = [ego_ps, ego_ps + 1] if ego_ps < 2 and base_ps > 0 else [ego_ps]
            
            for att_bonus in att_variations:
                for ds_bonus in ds_variations:
                    for evn_bonus in evn_variations:
                        for ps_bonus in ps_variations:
                            diff = calculate_special_item_difficulty(
                                tval, sval, att_bonus, ds_bonus, evn_bonus, ps_bonus,
                                ego_pval, special
                            )
                            min_depth = max(base_level, ego_depth)
                            items.append(ItemVariant(
                                name=item_name,
                                group_name=ego_name,
                                group_type='special',
                                difficulty=diff,
                                min_depth=min_depth,
                                max_depth=ego_max_depth,
                                rarity=ego_rarity,
                                tval=tval,
                                sval=sval,
                                att_bonus=att_bonus,
                                ds_bonus=ds_bonus,
                                evn_bonus=evn_bonus,
                                ps_bonus=ps_bonus,
                                pval=base_pval + ego_pval,
                                flags=special.get('flags', []),
                                special_name=ego_name
                            ))
    
    return items


def is_negative_ego(special: dict) -> bool:
    """Check if an ego is purely negative/cursed."""
    flags = set(special.get('flags', []))
    negative_flags = {'VUL_POIS', 'VUL_COLD', 'VUL_FIRE', 'DARKNESS', 'FEAR', 
                      'HUNGER', 'DANGER', 'LIGHT_CURSE', 'HEAVY_CURSE'}
    positive_flags = {'RES_POIS', 'RES_COLD', 'RES_FIRE', 'RES_FEAR', 'STR', 'DEX', 
                      'CON', 'GRA', 'SLAY_', 'BRAND_', 'FREE_ACT', 'SEE_INVIS',
                      'REGEN', 'LIGHT', 'RADIANCE', 'STEALTH', 'PERCEPTION', 'WILL',
                      'SONG', 'ARCHERY', 'SUST_', 'CHEAT_DEATH', 'ACCURATE', 'SHARPNESS'}
    
    # Check if any positive flags exist
    has_positive = False
    for flag in flags:
        for pf in positive_flags:
            if flag.startswith(pf) or flag == pf:
                has_positive = True
                break
        if has_positive:
            break
    
    # If only negative flags, it's a negative ego
    has_negative = any(f in flags for f in negative_flags)
    
    # Check special abilities
    has_abilities = special.get('abilities', 0) > 0
    
    return has_negative and not has_positive and not has_abilities


def get_ego_rarity(special: dict) -> int:
    """Get rarity from special data (second field of W: line)."""
    # This should be parsed from the file - for now use default
    # The parse_full_special_file doesn't capture rarity, so we'll need to add it
    return special.get('rarity', 1)


def get_ego_max_depth(special: dict) -> int:
    """Get max depth from special data (third field of W: line)."""
    return special.get('max_depth', 0)


def calculate_normal_item_difficulty(tval: int, sval: int, att_bonus: int = 0,
                                      ds_bonus: int = 0, evn_bonus: int = 0,
                                      ps_bonus: int = 0) -> int:
    """Calculate difficulty for a normal item with stat bonuses."""
    dif_inc = 0
    dif_mult = 100
    
    base_level = get_base_level(tval, sval)
    base_ps = get_base_ps(tval, sval)
    base_pd = get_base_pd(tval, sval)
    
    # Base item level contribution
    if tval not in [45, 40]:  # Not ring or amulet
        dif_inc += base_level // 2
    
    # Attack bonus
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
    if evn_bonus > 0:
        val = dif_mod_calc(evn_bonus, 6)
        if evn_bonus > 0:
            val -= 1
        dif_inc += val
    
    # Damage sides bonus
    if ds_bonus > 0:
        dif_inc += dif_mod_calc(ds_bonus, 3 * ds_bonus + 2)
    
    # Protection bonus
    if ps_bonus > 0 and base_pd > 0:
        new_prot = (base_ps + ps_bonus + 1) * base_pd
        base_prot = (base_ps + 1) * base_pd if base_ps > 0 else 0
        prot_bonus = new_prot - base_prot
        if prot_bonus > 0:
            if tval == 37 and sval == 6:  # Hauberk
                dif_inc += dif_mod_calc(prot_bonus, 1) + 2
            else:
                dif_inc += dif_mod_calc(prot_bonus, 3)
    
    # Slot multiplier
    slot = get_slot(tval)
    if slot in ['ring', 'light', 'cloak', 'gloves', 'boots', 'arrow']:
        dif_mult += 20
    
    dif = dif_inc * dif_mult // 100
    
    if tval == 17:  # Arrow
        dif = dif // 2

    return dif


def calculate_special_item_difficulty(tval: int, sval: int, att_bonus: int,
                                       ds_bonus: int, evn_bonus: int, ps_bonus: int,
                                       pval_bonus: int, special: dict) -> int:
    """Calculate difficulty for a special (ego) item."""
    dif_inc = 0
    dif_mult = 100
    brands = 0
    
    base_level = get_base_level(tval, sval)
    base_ps = get_base_ps(tval, sval)
    base_pd = get_base_pd(tval, sval)
    base_pval = get_base_pval(tval, sval)
    
    # Base item level contribution
    if tval not in [45, 40]:
        dif_inc += base_level // 2
    
    # Attack bonus
    if att_bonus > 0:
        if tval == 17:
            dif_inc += dif_mod_calc(att_bonus, 5) // 2
        elif tval in [19, 23, 22, 21, 20]:
            dif_inc += dif_mod_calc(att_bonus, 3)
        else:
            val = dif_mod_calc(att_bonus, 6)
            if val > 0:
                val -= 1
            dif_inc += val
    
    # Evasion bonus
    if evn_bonus > 0:
        val = dif_mod_calc(evn_bonus, 6)
        if evn_bonus > 0:
            val -= 1
        dif_inc += val
    
    # Damage sides bonus
    if ds_bonus > 0:
        dif_inc += dif_mod_calc(ds_bonus, 3 * ds_bonus + 2)
    
    # Protection bonus
    if ps_bonus > 0 and base_pd > 0:
        new_prot = (base_ps + ps_bonus + 1) * base_pd
        base_prot = (base_ps + 1) * base_pd if base_ps > 0 else 0
        prot_bonus = new_prot - base_prot
        if prot_bonus > 0:
            if tval == 37 and sval == 6:
                dif_inc += dif_mod_calc(prot_bonus, 1) + 2
            else:
                dif_inc += dif_mod_calc(prot_bonus, 3)
    
    # Process special flags
    flags = set(special.get('flags', []))
    total_pval = base_pval + pval_bonus
    
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
        dif_inc += 12 if tval == 17 else 16
        brands += 1
    
    if brands > 1:
        dif_inc += (brands - 1) * 20
    
    # Sharpness
    if 'SHARPNESS' in flags:
        dif_inc += 14 if tval == 17 else 24
    
    # Other weapon flags
    if 'VAMPIRIC' in flags:
        dif_inc += 6
    if 'ACCURATE' in flags:
        dif_inc += 15
    
    # Pval-dependent flags
    if total_pval > 0:
        if 'TUNNEL' in flags:
            dif_inc += dif_mod_calc(pval_bonus, 8) if pval_bonus > 0 else 0
        if 'STR' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'DEX' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'CON' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'GRA' in flags:
            dif_inc += dif_mod_calc(total_pval, 14)
        if 'ARCHERY' in flags:
            dif_inc += dif_mod_calc(total_pval, 4)
        if 'STEALTH' in flags:
            dif_inc += dif_mod_calc(total_pval, 4)
        if 'PERCEPTION' in flags:
            dif_inc += dif_mod_calc(total_pval, 3)
        if 'WILL' in flags:
            dif_inc += dif_mod_calc(total_pval, 3)
        if 'SONG' in flags:
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
    
    # Misc abilities
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
    
    # Abilities from B: lines
    for _ in range(special.get('abilities', 0)):
        dif_inc += 6  # Approximate: 5 + level/3
    
    # Slot multiplier
    slot = get_slot(tval)
    if slot in ['ring', 'light', 'cloak', 'gloves', 'boots', 'arrow']:
        dif_mult += 20
    
    dif = dif_inc * dif_mult // 100
    
    if tval == 17:
        dif = dif // 2

    return dif


def roll_target_difficulty(depth: int, quality: str = 'normal', verbose: bool = False) -> tuple:
    """
    Roll target difficulty for item drop.
    Formula: max(0, 1.8 * depth + min(1d30, 1d30) - 25 + bonus)
    Plus bonuses: +7 for good, +15 for great
    Result is clamped to minimum of 0.
    
    Returns: (target, roll1, roll2, min_roll, base_calc, bonus)
    """
    base = 1.8 * depth
    roll1 = random.randint(1, 30)
    roll2 = random.randint(1, 30)
    min_roll = min(roll1, roll2)
    
    bonus = 0
    if quality == 'good':
        bonus = 7
    elif quality == 'great':
        bonus = 15
    
    base_calc = int(base) + min_roll - 25
    target = max(0, base_calc + bonus)
    
    if verbose:
        print(f"DROP_TARGET: depth={depth} good={'yes' if quality == 'good' else 'no'} "
              f"great={'yes' if quality == 'great' else 'no'} bonus={bonus} "
              f"roll1={roll1} roll2={roll2} min={min_roll} base_calc={base_calc} "
              f"target={target} band={target-2}..{target+2}")
    
    return target, roll1, roll2, min_roll, base_calc, bonus


def apply_depth_penalty(items: List[ItemVariant], current_depth: int) -> List[Tuple[ItemVariant, int]]:
    """
    Apply depth penalty to items and return list of (item, adjusted_difficulty).
    Penalty: difficulty += 2 * (min_depth - current_depth) if current_depth < min_depth
    Also filter out items exceeding max_depth.
    """
    result = []
    for item in items:
        # Filter by max depth
        if item.max_depth > 0 and current_depth > item.max_depth:
            continue
        
        # Calculate adjusted difficulty
        adjusted_diff = item.difficulty
        if current_depth < item.min_depth:
            adjusted_diff += 2 * (item.min_depth - current_depth)
        
        result.append((item, adjusted_diff))
    
    return result


def filter_by_difficulty_band(items: List[Tuple[ItemVariant, int]], 
                               target: int, band: int = 2, verbose: bool = False) -> List[ItemVariant]:
    """Filter items to those within target +/- band difficulty."""
    result = [item for item, adj_diff in items 
            if target - band <= adj_diff <= target + band]
    
    if verbose and len(result) > 0:
        # Log first few candidates
        samples = min(5, len(result))
        for i in range(samples):
            item = result[i]
            adj_diff = next(d for it, d in items if it == item)
            print(f"DROP_CANDIDATE: relaxed={'yes' if band > 2 else 'no'} "
                  f"group_name={item.group_name[:30]} group_type={item.group_type} "
                  f"base_dif={item.difficulty} eff_dif={adj_diff} "
                  f"min_depth={item.min_depth} max_depth={item.max_depth} rarity={item.rarity}")
    
    return result


def group_items(items: List[ItemVariant]) -> Dict[str, List[ItemVariant]]:
    """
    Group items by their group_name.
    - Artefacts: each in its own group
    - Specials: grouped by ego name
    - Normal: grouped by base item name
    """
    groups = defaultdict(list)
    for item in items:
        groups[item.group_name].append(item)
    return dict(groups)


def get_group_rarity(items: List[ItemVariant]) -> int:
    """Get rarity for a group (use first item's rarity)."""
    if not items:
        return 1
    return items[0].rarity


def rarity_roll(groups: Dict[str, List[ItemVariant]], verbose: bool = False) -> Optional[str]:
    """
    Perform rarity-weighted random selection of a group.
    Uses inverse rarity weighting (lower rarity = more common).
    Formula matches C code: weight = max(1, 100 // max(1, rarity))
    """
    if not groups:
        return None
    
    # Calculate weights (inverse of rarity)
    # Higher rarity = lower weight = less common
    weights = []
    group_names = list(groups.keys())
    
    for name in group_names:
        rarity = get_group_rarity(groups[name])
        # Weight is inverse of rarity, minimum weight of 1
        weight = max(1, 100 // max(1, rarity))
        weights.append(weight)
    
    # Weighted random selection
    total = sum(weights)
    r = random.randint(0, total - 1)  # Match C's rand_int(total) which returns 0..total-1
    
    cumulative = 0
    chosen_idx = len(group_names) - 1
    for i, weight in enumerate(weights):
        cumulative += weight
        if r < cumulative:
            chosen_idx = i
            break
    
    if verbose:
        # Log group selection details (first 10 groups)
        samples = min(10, len(group_names))
        for i in range(samples):
            group = groups[group_names[i]]
            kind = 0 if group[0].group_type == 'normal' else (1 if group[0].group_type == 'special' else 2)
            rarity = get_group_rarity(group)
            print(f"DROP_GROUP: idx={i} kind={kind} rarity={rarity} "
                  f"weight={weights[i]} total={total} entries={len(group)} "
                  f"chosen={'YES' if i == chosen_idx else 'no'}")
        print(f"DROP_GROUP_PICK: pick={r} total={total} chosen_idx={chosen_idx}")
    
    return group_names[chosen_idx]


def select_item_from_group(group: List[ItemVariant]) -> ItemVariant:
    """Randomly select an item from a group."""
    return random.choice(group)


def simulate_drop(items: List[ItemVariant], depth: int, quality: str = 'normal', verbose: bool = False) -> Optional[ItemVariant]:
    """Simulate a single item drop with optional detailed logging."""
    # 1. Roll target difficulty
    target, roll1, roll2, min_roll, base_calc, bonus = roll_target_difficulty(depth, quality, verbose)
    
    # 2. Apply depth penalties and filter
    adjusted = apply_depth_penalty(items, depth)
    
    # 3. Filter by difficulty band (strict mode: +/- 2)
    band = 2
    eligible = filter_by_difficulty_band(adjusted, target, band, verbose)
    strict_count = len(eligible)
    used_relaxed = False
    
    # 4. If no items found in strict mode, use relaxed mode (all items regardless of band)
    if not eligible:
        eligible = [item for item, adj_diff in adjusted]  # All items, ignoring difficulty band
        used_relaxed = True
        if verbose:
            print(f"DROP_RELAXED: strict_count=0 relaxed_count={len(eligible)} used_relaxed=yes")
    
    if not eligible:
        return None
    
    # 5. Group items
    groups = group_items(eligible)
    
    # 6. Rarity roll to select group
    selected_group_name = rarity_roll(groups, verbose)
    if not selected_group_name:
        return None
    
    # 7. Select random item from group
    selected_item = select_item_from_group(groups[selected_group_name])
    
    if verbose:
        print(f"DROP_ITEM_SELECT: group_name={selected_group_name[:40]} "
              f"group_type={selected_item.group_type} entry_count={len(groups[selected_group_name])} "
              f"difficulty={selected_item.difficulty} rarity={selected_item.rarity}")
    
    return selected_item


def parse_special_with_rarity(filepath: str) -> List[dict]:
    """Parse special.txt and include rarity and max_depth from W: line."""
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
                    'tval_ranges': [],
                    'max_att': 0,
                    'to_dd': 0,
                    'to_ds': 0,
                    'max_evn': 0,
                    'to_pd': 0,
                    'to_ps': 0,
                    'max_pval': 0,
                    'abilities': 0,
                    'flags': [],
                    'depth': 0,
                    'rarity': 1,
                    'max_depth': 0,
                }
            
            elif line.startswith('W:') and current:
                parts = line[2:].split(':')
                current['depth'] = int(parts[0]) if parts[0] else 0
                current['rarity'] = int(parts[1]) if len(parts) > 1 and parts[1] else 1
                current['max_depth'] = int(parts[2]) if len(parts) > 2 and parts[2] else 0
            
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


def build_item_table_v2(artefact_file: str, special_file: str) -> List[ItemVariant]:
    """Build item table with proper rarity parsing."""
    items = []
    
    # Parse data files
    artefacts = parse_artefact_file(artefact_file)
    specials = parse_special_with_rarity(special_file)
    
    # Filter out template artefacts
    artefacts = [a for a in artefacts if not (a['name'].startswith("'Ultimate") and a['idx'] >= 182)]
    
    # 1. Add artefacts (each is its own group)
    for art in artefacts:
        art['type'] = 'artefact'
        diff = calculate_difficulty(art)
        items.append(ItemVariant(
            name=art['name'],
            group_name=art['name'],
            group_type='artefact',
            difficulty=diff,
            min_depth=art['depth'],
            max_depth=0,
            rarity=art['rarity'],
            tval=art['tval'],
            sval=art['sval'],
            att_bonus=art.get('att', 0),
            ds_bonus=art.get('ds', 0) - get_base_ds(art['tval'], art['sval']),
            evn_bonus=art.get('evn', 0),
            ps_bonus=art.get('ps', 0),
            pval=art.get('pval', 0),
            flags=art.get('flags', [])
        ))
    
    # 2. Add normal items with stat variations
    for tval, sval, name in SMITHABLE_BASE_ITEMS:
        base_att = get_base_att(tval, sval)
        base_evn = get_base_evn(tval, sval)
        base_ds = get_base_ds(tval, sval)
        base_ps = get_base_ps(tval, sval)
        base_level = get_base_level(tval, sval)
        
        att_range = [0]
        ds_range = [0]
        evn_range = [0]
        ps_range = [0]
        
        if tval in [19, 23, 22, 21, 20]:
            att_range = [0, 1]
            ds_range = [0, 1]
        
        if tval == 17:
            att_range = [0, 1, 2]
        
        if tval in [30, 31, 32, 34, 36, 37]:
            evn_range = [0, 1]
            if base_ps > 0:
                ps_range = [0, 1]
        
        if tval == 35:
            evn_range = [0, 1]
            ps_range = [0]
        
        for att_bonus in att_range:
            for ds_bonus in ds_range:
                for evn_bonus in evn_range:
                    for ps_bonus in ps_range:
                        diff = calculate_normal_item_difficulty(
                            tval, sval, att_bonus, ds_bonus, evn_bonus, ps_bonus
                        )
                        items.append(ItemVariant(
                            name=name,
                            group_name=name,
                            group_type='normal',
                            difficulty=diff,
                            min_depth=base_level,
                            max_depth=0,
                            rarity=1,
                            tval=tval,
                            sval=sval,
                            att_bonus=att_bonus,
                            ds_bonus=ds_bonus,
                            evn_bonus=evn_bonus,
                            ps_bonus=ps_bonus
                        ))
    
    # 3. Add special items
    for special in specials:
        if is_negative_ego(special):
            continue
        
        ego_depth = special.get('depth', 0)
        ego_rarity = special.get('rarity', 1)
        ego_max_depth = special.get('max_depth', 0)
        
        for tval, sval, base_name in SMITHABLE_BASE_ITEMS:
            if not special_applies_to_item(special, tval, sval):
                continue
            
            base_att = get_base_att(tval, sval)
            base_evn = get_base_evn(tval, sval)
            base_ds = get_base_ds(tval, sval)
            base_ps = get_base_ps(tval, sval)
            base_level = get_base_level(tval, sval)
            base_pval = get_base_pval(tval, sval)
            
            ego_att = 1 if special.get('max_att', 0) > 0 else 0
            ego_ds = 1 if special.get('to_ds', 0) > 0 else 0
            ego_evn = 1 if special.get('max_evn', 0) > 0 else 0
            ego_ps = 1 if special.get('to_ps', 0) > 0 else 0
            ego_pval = 1 if special.get('max_pval', 0) > 0 else 0
            
            ego_name = special['name']
            item_name = f"{base_name} {ego_name}"
            
            att_variations = [ego_att]
            ds_variations = [ego_ds]
            evn_variations = [ego_evn]
            ps_variations = [ego_ps]
            
            if tval in [19, 23, 22, 21, 20]:
                att_variations = [ego_att, ego_att + 1] if ego_att < 2 else [ego_att]
                ds_variations = [ego_ds, ego_ds + 1] if ego_ds < 2 else [ego_ds]
            if tval in [30, 31, 32, 34, 36, 37]:
                evn_variations = [ego_evn, ego_evn + 1] if ego_evn < 2 else [ego_evn]
                ps_variations = [ego_ps, ego_ps + 1] if ego_ps < 2 and base_ps > 0 else [ego_ps]
            
            for att_bonus in att_variations:
                for ds_bonus in ds_variations:
                    for evn_bonus in evn_variations:
                        for ps_bonus in ps_variations:
                            diff = calculate_special_item_difficulty(
                                tval, sval, att_bonus, ds_bonus, evn_bonus, ps_bonus,
                                ego_pval, special
                            )
                            min_depth = max(base_level, ego_depth)
                            items.append(ItemVariant(
                                name=item_name,
                                group_name=ego_name,
                                group_type='special',
                                difficulty=diff,
                                min_depth=min_depth,
                                max_depth=ego_max_depth,
                                rarity=ego_rarity,
                                tval=tval,
                                sval=sval,
                                att_bonus=att_bonus,
                                ds_bonus=ds_bonus,
                                evn_bonus=evn_bonus,
                                ps_bonus=ps_bonus,
                                pval=base_pval + ego_pval,
                                flags=special.get('flags', []),
                                special_name=ego_name
                            ))
    
    return items


def run_simulation(items: List[ItemVariant], depths: List[int], 
                   normal_count: int, good_count: int, great_count: int,
                   verbose: bool = False, verbose_samples: int = 3):
    """Run simulations and print results. 
    
    Args:
        verbose: If True, print detailed logs for first few drops at each depth
        verbose_samples: Number of drops to log in detail per depth
    """
    
    print("=" * 100)
    print("ITEM DROP SIMULATION RESULTS")
    print("=" * 100)
    print()
    print(f"Total items in table: {len(items)}")
    print(f"  - Artefacts: {len([i for i in items if i.group_type == 'artefact'])}")
    print(f"  - Specials: {len([i for i in items if i.group_type == 'special'])}")
    print(f"  - Normal: {len([i for i in items if i.group_type == 'normal'])}")
    print()
    
    for depth in depths:
        print(f"\n{'=' * 80}")
        print(f"DEPTH {depth} (50 * {depth} = {50 * depth} feet)")
        print(f"{'=' * 80}")
        
        # Calculate expected difficulty range
        base_diff = 1.8 * depth - 25
        min_roll = 1
        max_roll = 30
        
        print(f"Base difficulty formula: max(0, 1.8*{depth} + min(1d30,1d30) - 25)")
        print(f"Expected range: {max(0, base_diff + min_roll):.1f} to {max(0, base_diff + max_roll):.1f}")
        print(f"Good bonus: +7, Great bonus: +15")
        print()
        
        # Collect ALL drops for this depth
        all_drops = []
        no_drop_count = 0
        
        # Track which drops get verbose logging
        drop_counter = 0
        
        # Run all simulations
        for quality, count in [('normal', normal_count - good_count - great_count),
                                ('good', good_count),
                                ('great', great_count)]:
            for _ in range(count):
                # Enable verbose logging for first few drops
                do_verbose = verbose and drop_counter < verbose_samples
                if do_verbose:
                    print(f"\n--- Verbose Drop #{drop_counter + 1} (quality={quality}) ---")
                
                item = simulate_drop(items, depth, quality, verbose=do_verbose)
                drop_counter += 1
                
                if item:
                    all_drops.append((item, quality))
                else:
                    no_drop_count += 1
        
        if verbose and verbose_samples > 0:
            print(f"\n--- End of verbose logging (showed {min(verbose_samples, drop_counter)} drops) ---\n")
        
        # Count by group
        group_counts = defaultdict(lambda: {'count': 0, 'normal': 0, 'good': 0, 'great': 0, 'item': None})
        type_counts = {'artefact': 0, 'special': 0, 'normal': 0}
        
        for item, quality in all_drops:
            key = item.group_name
            group_counts[key]['count'] += 1
            group_counts[key][quality] += 1
            group_counts[key]['item'] = item
            type_counts[item.group_type] += 1
        
        # Print summary
        total_drops = len(all_drops)
        print(f"Total drops: {total_drops}, No valid drops: {no_drop_count}")
        print(f"By type: Artefacts={type_counts['artefact']}, Specials={type_counts['special']}, Normal={type_counts['normal']}")
        print()
        
        # Sort by difficulty (highest first)
        sorted_groups = sorted(group_counts.items(), key=lambda x: -x[1]['item'].difficulty)
        
        print(f"{'Group/Item':<45} {'Type':<8} {'Rarity':<6} {'Diff':<5} {'Total':<6} {'Norm':<5} {'Good':<5} {'Great':<5}")
        print("-" * 95)
        
        for group_name, data in sorted_groups[:40]:
            item = data['item']
            display_name = group_name[:43] if len(group_name) > 43 else group_name
            type_str = item.group_type[:3].upper()
            print(f"{display_name:<45} {type_str:<8} {item.rarity:<6} {item.difficulty:<5} "
                  f"{data['count']:<6} {data['normal']:<5} {data['good']:<5} {data['great']:<5}")
        
        if len(sorted_groups) > 40:
            print(f"... and {len(sorted_groups) - 40} more groups")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Simulate the new item drop system for Sil-More')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable detailed logging for first few drops')
    parser.add_argument('-s', '--samples', type=int, default=3,
                        help='Number of drops to log in detail per depth (default: 3)')
    args = parser.parse_args()
    
    # Find data files
    possible_base_paths = [
        os.path.join(script_dir, '..'),
        os.path.join(script_dir, '..', '..'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh',
    ]
    
    artefact_file = None
    special_file = None
    
    for base in possible_base_paths:
        art_path = os.path.join(base, 'lib', 'edit', 'artefact.txt')
        spec_path = os.path.join(base, 'lib', 'edit', 'special.txt')
        if os.path.exists(art_path) and os.path.exists(spec_path):
            artefact_file = art_path
            special_file = spec_path
            break
    
    if not artefact_file or not special_file:
        print("ERROR: Could not find artefact.txt and special.txt")
        return
    
    print("Building item table...")
    items = build_item_table_v2(artefact_file, special_file)
    print(f"Built table with {len(items)} item variants")
    
    # Run simulations for different depths
    # 100 drops per depth: 80 normal, 10 good, 10 great
    depths = [1, 5, 10, 15, 20, 25]
    run_simulation(items, depths, normal_count=100, good_count=10, great_count=10,
                   verbose=args.verbose, verbose_samples=args.samples)


if __name__ == '__main__':
    main()
