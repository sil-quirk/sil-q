#!/usr/bin/env python3
import re
import pandas as pd

# Unique trait scoring - each unique trait gets its own score (default 2)
# To customize scoring for individual uniques, change the values below
UNIQUE_SCORES = {
    'SMT_FEANOR': 3,      # Uses only 1 forge cast for custom artifacts
    'WIL_FIN': 3,         # Majesty ability gets twice more base will
    'MEL_MAEDHROS': 1,    # One handed
    'SNG_FIN': 2,         # Song of Staying is twice as effective
    'SNG_THINGOL': 3,     # Song of Mastery is twice as effective
    'SNG_LUT': 3,         # Song of Lorien is twice more effective
    'SMT_TELCHAR': 3,     # Available to craft SHARPNESS2 weapons
    'SMT_GAMIL': 2,       # Able to craft without mithril
    'MIM': 1,             # Has all stealth abilities
    'EARENDIL': 2,        # Will affinity is always at MASTER+
    'WIL_TURIN': 2,       # Debuf->rage->hallucination
    'WIL_TUOR': 1,        # Horns are 2 times more effective
    'SNG_HURIN': 3,       # Song of Slaying is twice more effective
    'SNG_MEL': 2,         # Song of Thresholds difficulty decreased
    'SMT_EOL': 2,         # Galvane armor
}

# Ability scoring configuration
ABILITY_MULTIPLIER = 0.2  # Multiplier for ability level requirements

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
                ability_score = level_req * ABILITY_MULTIPLIER
                total_score += ability_score
                
                ability_details.append({
                    'name': ability_info['name'],
                    'skill': skill_num,
                    'ability': ability_num,
                    'level_req': level_req,
                    'score': ability_score
                })
            else:
                # Unknown ability, use default score
                default_score = 1.0
                total_score += default_score
                ability_details.append({
                    'name': f'Unknown({skill_num},{ability_num})',
                    'skill': skill_num,
                    'ability': ability_num,
                    'level_req': '?',
                    'score': default_score
                })
        except ValueError:
            # Invalid skill/ability format, use default score
            default_score = 1.0
            total_score += default_score
            ability_details.append({
                'name': f'Invalid({skill_str},{ability_str})',
                'skill': skill_str,
                'ability': ability_str,
                'level_req': '?',
                'score': default_score
            })
    
    return total_score, ability_details

def calculate_p_value(total_with_dots):
    """Calculate P: value based on Total with dots score"""
    if total_with_dots >= 18:
        return 3
    elif total_with_dots >= 14:
        return 2
    elif total_with_dots >= 10:
        return 1
    else:
        return 0

def analyze_p_updates(scores):
    """Analyze what P: values would be updated and show preview"""
    print("\n" + "="*60)
    print("P: VALUE ANALYSIS (Based on Total_Dots)")
    print("="*60)
    print("Rules:")
    print("  Total_Dots >= 18: P:3")
    print("  Total_Dots >= 14: P:2") 
    print("  Total_Dots >= 10: P:1")
    print("  Total_Dots <  10: P:0")
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
        total_dots = score['Total_Dots']
        new_p = calculate_p_value(total_dots)
        current_p = current_p_values.get(hero_name, 0)
        new_p_values[hero_name] = new_p
        
        if current_p != new_p:
            changes_needed.append({
                'hero': hero_name,
                'current_p': current_p,
                'new_p': new_p,
                'total_dots': total_dots
            })
    
    print(f"{'Hero':<20} {'Total_Dots':<12} {'Current P:':<10} {'New P:':<8} {'Change'}")
    print("-"*60)
    
    for change in changes_needed:
        change_indicator = "→" if change['current_p'] != change['new_p'] else "="
        print(f"{change['hero']:<20} {change['total_dots']:<12.2f} {change['current_p']:<10} {change['new_p']:<8} {change_indicator}")
    
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
                    'C_pairs': []    # abilities
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
        stats_total = Str + Dex + Con + Gra

        # 2) Net affinities / penalties
        def net_aff(flags):
            return sum(1 for f in flags if f.endswith('_AFFINITY')) \
                 - sum(1 for f in flags if f.endswith('_PENALTY'))
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
        uniq_total += free_count * 2

        # 4) Abilities - scored individually based on level requirements
        abil_total, ability_details = calculate_ability_score(abilities_data, h['C_pairs'])
        # Debug: uncomment the line below to see ability scoring details
        # if ability_details:
        #     print(f"  {h['name']} abilities:")
        #     for detail in ability_details:
        #         print(f"    {detail['name']} (level {detail['level_req']}): {detail['score']:.2f}")
        #     print(f"    Total: {abil_total:.2f}")

        # 5) Grand total
        total = stats_total + aff_total + uniq_total + abil_total

        # 6) Dot counts
        special = set(h['F'] + (r['F'] if r else []))
        red_count = (
            (2 if 'KINSLAYER' in special else 0) +
            (1 if 'TREACHERY' in special else 0) +
            (1 if 'CURSE' in special else 0) +
            (1 if 'MOR_CURSE' in special else 0)
        )
        green_count = (1 if 'GIFTERU' in special else 0)
        dots = '🔴'*red_count + '🟢'*green_count

        # net_dots for tie-breaker
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
            'Total':       total,
            'Total_Dots':  total_with_dots,
            'Dots':        dots,
        })

    # sort by Total descending, then by Total_Dots descending
    return sorted(results,
                  key=lambda x: (x['Total_Dots'], x['Total']),
                  reverse=True)

def main():
    houses = parse_houses('character.txt')
    races  = parse_races('race.txt')
    abilities_data = parse_abilities('ability.txt')
    scores = compute_scores(houses, races, abilities_data)

    df = pd.DataFrame(scores)
    # drop the internal '_net' column before printing
    # df = df.drop(columns=['_net'])
    print(df.to_string(index=False))
    
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
