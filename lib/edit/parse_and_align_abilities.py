
from collections import defaultdict

def parse_house_file(house_path):
    hero_abilities = defaultdict(list)
    current_hero = None
    with open(house_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("N:"):
                current_hero = line.split(":", 2)[2]
            elif line.startswith("C:") and current_hero:
                nums = [int(n) for n in line[2:].replace(":", " ").split() if n.isdigit()]
                coords = [(nums[i], nums[i + 1]) for i in range(0, len(nums) - 1, 2)]
                for coord in coords:
                    hero_abilities[coord].append(current_hero)
    return hero_abilities

def parse_abilities_file(abilities_path):
    ability_dict = {}
    with open(abilities_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("C:") and "#" in line:
                code, name = line.split("#", 1)
                r, c = map(int, code.strip()[2:].split(":"))
                ability_dict[(r, c)] = {'name': name.strip(), 'code': f"C:{r}:{c}"}
    return ability_dict

def write_annotated_output(abilities_dict, hero_abilities, output_path):
    with open(output_path, "w", encoding="utf-8") as f:
        for (r, c), info in sorted(abilities_dict.items()):
            code_str = info['code']
            name = info['name']
            heroes = hero_abilities.get((r, c), [])
            if heroes:
                line = f"{code_str:<6}  # {name:<25}: {', '.join(heroes)}\n"
            else:
                line = f"{code_str:<6}  # {name:<25}:\n"
            f.write(line)

if __name__ == "__main__":
    abilities_path = "actual_abilities_commented.txt"
    house_path = "house.txt"
    output_path = "actual_abilities_with_heroes_final.txt"

    hero_abilities = parse_house_file(house_path)
    abilities_dict = parse_abilities_file(abilities_path)
    write_annotated_output(abilities_dict, hero_abilities, output_path)
