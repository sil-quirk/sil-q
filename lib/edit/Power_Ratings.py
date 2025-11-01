#!/usr/bin/env python3
import re
import pandas as pd

# Unique trait scoring - each unique trait gets its own score (default 2)
# To customize scoring for individual uniques, change the values below
UNIQUE_SCORES = {
    'SMT_FEANOR': 2,      # Uses only 1 forge cast for custom artifacts
    'WIL_FIN': 2,         # Majesty ability gets twice more base will
    'MEL_MAEDHROS': 1.5,    # One handed
    'SNG_FIN': 1.5,         # Song of Staying is twice as effective
    'SNG_THINGOL': 2,     # Song of Mastery is twice as effective
    'SNG_LUT': 2,         # Song of Lorien is twice more effective
    'SMT_TELCHAR': 2,     # Available to craft SHARPNESS2 weapons
    'SMT_GAMIL': 1,       # Able to craft without mithril
    'MIM': 1,             # Has all stealth abilities
    'EARENDIL': 1,        # Will affinity is always at MASTER+
    'WIL_TURIN': 1,       # Debuf->rage->hallucination
    'WIL_TUOR': 1,        # Horns are 2 times more effective
    'SNG_HURIN': 2,       # Song of Slaying is twice more effective
    'SNG_MEL': 1,         # Song of Thresholds difficulty decreased
    'SMT_EOL': 1,         # Galvane armor
}

# Ability scoring configuration
ABILITY_MULTIPLIER = 0.2  # Multiplier for ability level requirements

# P: thresholds (change here to adjust rating cutoffs)
# Characters with Total >= P4_THRESHOLD get P:4, Characters with Total >= P3_THRESHOLD get P:3, >= P2_THRESHOLD get P:2, >= P1_THRESHOLD get P:1
P4_THRESHOLD = 19
P3_THRESHOLD = 15
P2_THRESHOLD = 11.5
P1_THRESHOLD = 9

# Stat and skill weighting (change here to tune scoring)
# Per-stat multipliers (applied to each stat value before summing)
STAT_WEIGHTS = {
    'Str': 1.0,   # Strength (keep previous overall stat multiplier for Str)
    'Dex': 1.0,   # Dexterity (user requested 1)
    'Con': 1.0,   # Constitution
    'Gra': 1.0    # Grace / perception-adj stat (keep at 1.5 by default)
}

# Skill/affinity weights (used when flags like PER_AFFINITY appear)
SKILL_WEIGHTS = {
    'PER': 0.7,   # Perception
    'WIL': 0.8,   # Will
    'STL': 0.8,   # Stealth
    'SMT': 0.9,   # Smithing
    'SNG': 0.9,   # Song
    'EVN': 1.0,   # Evasion
    'ARC': 1.0,   # Archery
    'MEL': 1.0,   # Melee (default 1)
}

# Proficiency flags (BOW_PROFICIENCY / AXE_PROFICIENCY) get a small fixed bonus
PROFICIENCY_WEIGHTS = {
    'BOW_PROFICIENCY': 0.2,
    'AXE_PROFICIENCY': 0.2,
}

def parse_abilities(path):
    """Parse ability.txt to get ability level requirements"""
    abilities = {}  # key: (skill_num, ability_num), value: level_requirement
    
    with open(path, 'r', encoding='utf-8') as f:
        current_ability = None
        for line in f:
            line = line.strip()
            if not line or line.startswith('#') or line.startswith('V:'):
                continue
                
            if line.startswith('N:'):
                _, num, name = line.split(':', 2)
                current_ability = {
                    'num': int(num),
                    'name': name.strip(),
                    'skill_num': None,
                    'ability_num': None,
                    'level_req': None
                }
                
            elif line.startswith('I:') and current_ability:
                # I:skill_number:ability_number:level_requirement
                parts = line.split(':')
                if len(parts) >= 4:
                    skill_num = int(parts[1])
                    ability_num = int(parts[2])
                    level_req = int(parts[3])
                    
                    current_ability['skill_num'] = skill_num
                    current_ability['ability_num'] = ability_num
                    current_ability['level_req'] = level_req
                    
                    # Store using (skill_num, ability_num) as key
                    abilities[(skill_num, ability_num)] = {
                        'name': current_ability['name'],
                        'level_req': level_req,
                        'ability_id': current_ability['num']
                    }
    
    return abilities

def calculate_ability_score(abilities_data, character_abilities):
    """Calculate total ability score based on level requirements"""
    total_score = 0
    ability_details = []
    
    # character_abilities is a list of (skill, ability) pairs (as strings)
    for skill_str, ability_str in character_abilities:
        try:
            skill_num = int(skill_str)
            ability_num = int(ability_str)
            ability_key = (skill_num, ability_num)
            
            if ability_key in abilities_data:
                ability_info = abilities_data[ability_key]
                level_req = ability_info['level_req']
                # New rule: abilities contribute 0 if level_req < 6,
                # otherwise contribute 0.1 * level_req (exact formula).
                try:
                    lr = float(level_req)
                except Exception:
                    lr = 0.0
                if lr >= 6.0:
                    ability_score = 0.1 * lr
                else:
                    ability_score = 0.0
                total_score += ability_score

                ability_details.append({
                    'name': ability_info['name'],
                    'skill': skill_num,
                    'ability': ability_num,
                    'level_req': level_req,
                    'score': ability_score
                })
            else:
                # Unknown ability - treat as zero contribution under new rule
                default_score = 0.0
                total_score += default_score
                ability_details.append({
                    'name': f'Unknown({skill_num},{ability_num})',
                    'skill': skill_num,
                    'ability': ability_num,
                    'level_req': '?',
                    'score': default_score
                })
        except ValueError:
            # Invalid skill/ability format - treat as zero contribution
            default_score = 0.0
            total_score += default_score
            ability_details.append({
                'name': f'Invalid({skill_str},{ability_str})',
                'skill': skill_str,
                'ability': ability_str,
                'level_req': '?',
                'score': default_score
            })
    
    return total_score, ability_details

def calculate_p_value(total_value):
    """Calculate P: value based on numeric Total.

    Uses the thresholds defined at the top of the file (P4_THRESHOLD, P3_THRESHOLD, P2_THRESHOLD, P1_THRESHOLD).
    """
    if total_value >= P4_THRESHOLD:
        return 4
    elif total_value >= P3_THRESHOLD:
        return 3
    elif total_value >= P2_THRESHOLD:
        return 2
    elif total_value >= P1_THRESHOLD:
        return 1
    else:
        return 0

def analyze_p_updates(scores):
    """Analyze what P: values would be updated and show preview"""
    print("\n" + "="*60)
    print("P: VALUE ANALYSIS (Based on Total)")
    print("="*60)
    print("Rules:")
    print(f"  Total >= {P4_THRESHOLD}: P:4")
    print(f"  Total >= {P3_THRESHOLD}: P:3")
    print(f"  Total >= {P2_THRESHOLD}: P:2")
    print(f"  Total >= {P1_THRESHOLD}: P:1")
    print(f"  Total <  {P1_THRESHOLD}: P:0")
    print("-"*60)
    
    current_p_values = {}
    new_p_values = {}
    
    # Read current P: values from character.txt
    with open('character.txt', 'r', encoding='utf-8') as f:
        current_char = None
        for line in f:
            if line.startswith('N:'):
                _, num, name = line.split(':', 2)
                current_char = name.strip()
            elif line.startswith('P:') and current_char:
                current_p_values[current_char] = int(line.split(':')[1].strip())
    
    # Calculate new P: values
    changes_needed = []
    for score in scores:
        hero_name = score['Hero']
        total_val = score['Total']
        new_p = calculate_p_value(total_val)
        current_p = current_p_values.get(hero_name, 0)
        new_p_values[hero_name] = new_p
        
        if current_p != new_p:
            changes_needed.append({
                'hero': hero_name,
                'current_p': current_p,
                'new_p': new_p,
                'total': total_val
            })
    
    print(f"{'Hero':<20} {'Total':>12} {'Current P:':<10} {'New P:':<8} {'Change'}")
    print("-"*60)
    
    for change in changes_needed:
        change_indicator = "→" if change['current_p'] != change['new_p'] else "="
        print(f"{change['hero']:<20} {(change['total_dots'] if 'total_dots' in change else change.get('total', 0)):>12.2f} {change['current_p']:<10} {change['new_p']:<8} {change_indicator}")
    
    if not changes_needed:
        print("No changes needed - all P: values already match the rules!")
    else:
        print(f"\n{len(changes_needed)} characters would have P: values updated.")
    
    return changes_needed, new_p_values

def update_character_txt_p_values(new_p_values):
    """Update P: values in character.txt file"""
    with open('character.txt', 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    updated_lines = []
    current_char = None
    
    for line in lines:
        if line.startswith('N:'):
            _, num, name = line.split(':', 2)
            current_char = name.strip()
            updated_lines.append(line)
        elif line.startswith('P:') and current_char and current_char in new_p_values:
            # Update P: value
            new_p = new_p_values[current_char]
            updated_lines.append(f'P:{new_p}\n')
        else:
            updated_lines.append(line)
    
    # Write back to file
    with open('character.txt', 'w', encoding='utf-8') as f:
        f.writelines(updated_lines)
    
    print("✓ character.txt has been updated with new P: values!")

def print_unique_scores():
    """Display all unique traits and their current scores"""
    print("Current Unique Trait Scores:")
    print("-" * 40)
    for unique, score in sorted(UNIQUE_SCORES.items()):
        print(f"{unique:<15}: {score} points")
    print("-" * 40)

def parse_houses(path):
    houses = []
    with open(path, 'r', encoding='utf-8') as f:
        current = None
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#') or line.startswith('V:'):
                continue

            if line.startswith('N:'):
                if current and current['num'] != 0:
                    houses.append(current)
                _, num, name = raw.split(':', 2)
                current = {
                    'num': int(num),
                    'name': name.strip(),
                    'S': [],         # [Str, Dex, Con, Gra]
                    'F': [],         # affinities / penalties
                    'U': [],         # uniques
                    'C_pairs': [],   # abilities
                    'P': 0           # current P: value from file (default 0)
                }

            elif line.startswith('S:'):
                current['S'] = list(map(int, line.split(':')[1:5]))

            elif line.startswith('F:'):
                flags = raw.split(':',1)[1].split('|')
                current['F'] = [f.strip() for f in flags if f.strip()]

            elif line.startswith('U:'):
                uniques = raw.split(':',1)[1].split('|')
                current['U'] = [u.strip() for u in uniques if u.strip()]

            elif line.startswith('C:'):
                content = raw.split('#',1)[0][2:].strip()
                parts = content.split(':')
                for i in range(0, len(parts)-1, 2):
                    current['C_pairs'].append((parts[i].strip(), parts[i+1].strip()))
            elif line.startswith('P:') and current:
                # P:value
                try:
                    current['P'] = int(line.split(':',1)[1].strip())
                except Exception:
                    current['P'] = 0

        if current and current['num'] != 0:
            houses.append(current)
    return houses

def parse_races(path):
    races = []
    with open(path, 'r', encoding='utf-8') as f:
        current = None
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#') or line.startswith('V:'):
                continue

            if line.startswith('N:'):
                if current:
                    races.append(current)
                _, num, name = raw.split(':', 2)
                current = {
                    'num': int(num),
                    'name': name.strip(),
                    'S':   [],  # [Str, Dex, Con, Gra]
                    'F':   [],  # affinities / penalties
                    'C':   []   # member hero IDs
                }

            elif line.startswith('S:'):
                current['S'] = list(map(int, line.split(':')[1:5]))

            elif line.startswith('F:'):
                flags = raw.split(':',1)[1].split('|')
                current['F'] = [f.strip() for f in flags if f.strip()]

            elif line.startswith('C:'):
                ids = re.findall(r'\d+', raw)
                current['C'] = list(map(int, ids))

        if current:
            races.append(current)
    return races

def compute_scores(houses, races, abilities_data):
    # build hero→race lookup by membership
    hero2race = {}
    for r in races:
        for hid in r['C']:
            hero2race[hid] = r

    results = []
    for h in houses:
        r = hero2race.get(h['num'], None)

        # 1) Stats by type
        hs = h['S']
        rs = r['S'] if r else [0,0,0,0]
        Str = hs[0] + rs[0]
        Dex = hs[1] + rs[1]
        Con = hs[2] + rs[2]
        Gra = hs[3] + rs[3]
        # Apply per-stat weights and compute weighted stat total
        stats_total = (
            STAT_WEIGHTS.get('Str', 1.0) * Str +
            STAT_WEIGHTS.get('Dex', 1.0) * Dex +
            STAT_WEIGHTS.get('Con', 1.0) * Con +
            STAT_WEIGHTS.get('Gra', 1.0) * Gra
        )

        # 2) Net affinities / penalties (weighted)
        def net_aff(flags):
            total = 0.0
            for f in flags:
                if f.endswith('_AFFINITY'):
                    key = f.rsplit('_', 1)[0]  # e.g. 'PER' from 'PER_AFFINITY'
                    weight = SKILL_WEIGHTS.get(key, 1.0)
                    total += weight
                elif f.endswith('_PENALTY'):
                    key = f.rsplit('_', 1)[0]
                    weight = SKILL_WEIGHTS.get(key, 1.0)
                    total -= weight
                else:
                    # handle standalone proficiencies (BOW_PROFICIENCY, AXE_PROFICIENCY)
                    prof_weight = PROFICIENCY_WEIGHTS.get(f, 0.0)
                    total += prof_weight
            return total

        aff_total = net_aff(h['F']) + (net_aff(r['F']) if r else 0)

        # 3) Unique traits - each unique scored individually
        uniq_total = 0
        for unique in h['U']:
            # Get individual score for this unique, default to 2 if not in mapping
            unique_score = UNIQUE_SCORES.get(unique, 2)
            uniq_total += unique_score
            # Debug: uncomment the line below to see which uniques are being scored
            # print(f"  {h['name']}: {unique} = {unique_score} points")
            
        #    + FREE flags → +2 each
        free_count = h['F'].count('FREE') + (r['F'].count('FREE') if r else 0)
        uniq_total += free_count * 1.5

        # 4) Abilities - scored individually based on the exact rule:
        #    contribution = 0 if level_req < 6, else 0.1 * level_req
        abil_total, ability_details = calculate_ability_score(abilities_data, h['C_pairs'])
        # Debug: uncomment the line below to see ability scoring details
        # if ability_details:
        #     print(f"  {h['name']} abilities:")
        #     for detail in ability_details:
        #         print(f"    {detail['name']} (level {detail['level_req']}): {detail['score']:.2f}")
        #     print(f"    Total: {abil_total:.2f}")

        # 5) Grand total (abilities already use the exact formula in abil_total)
        total = stats_total + aff_total + uniq_total + abil_total

        # 6) Dot counts
        special = set(h['F'] + (r['F'] if r else []))

        # MOR_CURSE: move this to affect the numeric total (penalty)
        # but exclude it from the dot-based tie-breaker. We apply a
        # small numeric penalty when MOR_CURSE is present and then
        # do not count it in red_count/net_dots below.
        mor_curse_penalty = -1 if 'MOR_CURSE' in special else 0

        red_count = (
            (2 if 'KINSLAYER' in special else 0) +
            (1 if 'TREACHERY' in special else 0) +
            (1 if 'CURSE' in special else 0)
            # 'MOR_CURSE' intentionally excluded from dot calculation
        )
        green_count = (1 if 'GIFTERU' in special else 0)
        dots = '🔴'*red_count + '🟢'*green_count

        # Apply MOR_CURSE penalty to the numeric total so it affects
        # rankings by 'Total' but not the dot tie-breaker.
        total += mor_curse_penalty

        # net_dots for tie-breaker (MOR_CURSE excluded)
        net_dots = green_count - red_count

        total_with_dots = total + (1 * net_dots)

        results.append({
            'Hero':        h['name'],
            'Str':         Str,
            'Dex':         Dex,
            'Con':         Con,
            'Gra':         Gra,
            'Stats':       stats_total,
            'Affinities':  aff_total,
            'Unique':      uniq_total,
            'Abilities':   abil_total,
            'Current_P':   h.get('P', 0),
            'Total_Dots':  total_with_dots,
            # Place numeric Total before the emoji Dots so the numbers
            # are not visually shifted by variable-width emoji when
            # printing to a terminal.
            'Total':       total,
            'Dots':        dots,
        })

    # sort by Total (primary) descending, then Total_Dots (secondary)
    return sorted(results,
                  key=lambda x: (x['Total'], x['Total_Dots']),
                  reverse=True)

def main():
    houses = parse_houses('character.txt')
    races  = parse_races('race.txt')
    abilities_data = parse_abilities('ability.txt')
    scores = compute_scores(houses, races, abilities_data)

    df = pd.DataFrame(scores)
    # drop the internal '_net' column before printing
    # df = df.drop(columns=['_net'])
    # Use a fixed float format so numeric columns (Total, Total_Dots, Abilities)
    # line up nicely in monospaced console output. Width 6 with 1 decimal
    # gives more room for negative signs and two-digit numbers.
    print(df.to_string(index=False, float_format='{:6.1f}'.format))
    
    print("\n" + "="*50)
    print_unique_scores()
    
    print(f"\nAbility Scoring: Level requirement × {ABILITY_MULTIPLIER}")
    print(f"Found {len(abilities_data)} abilities in ability.txt")
    
    # Optional: Show detailed ability breakdown for a specific character
    show_details = input("\nShow detailed ability breakdown for a character? (y/n): ").strip().lower()
    if show_details in ['y', 'yes']:
        print("\nAvailable characters:")
        for i, score in enumerate(scores[:10]):  # Show first 10
            print(f"{i+1:2d}. {score['Hero']}")
        try:
            choice = int(input("Enter number (1-10): ")) - 1
            if 0 <= choice < min(10, len(scores)):
                hero_name = scores[choice]['Hero']
                # Find the character in houses
                for h in houses:
                    if h['name'] == hero_name:
                        _, ability_details = calculate_ability_score(abilities_data, h['C_pairs'])
                        print(f"\n{hero_name} Ability Breakdown:")
                        print("-" * 40)
                        total_abil = 0
                        for detail in ability_details:
                            print(f"{detail['name']:<25} Level {detail['level_req']:>2}: {detail['score']:>4.1f}")
                            total_abil += detail['score']
                        print("-" * 40)
                        print(f"{'Total':<25} {'':>8}: {total_abil:>4.1f}")
                        break
        except (ValueError, IndexError):
            print("Invalid choice.")
    
    # Analyze P: value updates
    changes_needed, new_p_values = analyze_p_updates(scores)
    
    if changes_needed:
        print(f"\nDo you want to update character.txt with these new P: values? (y/n): ", end="")
        response = input().strip().lower()
        if response in ['y', 'yes']:
            update_character_txt_p_values(new_p_values)
        else:
            print("P: values not updated.")
    
    input("\nDone — press Enter to exit...")

if __name__ == '__main__':
    main()
