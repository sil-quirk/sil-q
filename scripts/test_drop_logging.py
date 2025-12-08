#!/usr/bin/env python3
"""
Test script to demonstrate the enhanced logging in the drop simulation.
This runs a small number of drops with verbose logging enabled.
"""
import sys
import os

# Add parent directory to path
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)

from simulate_drops import build_item_table_v2, simulate_drop

def main():
    # Find data files
    base_path = os.path.join(script_dir, '..')
    artefact_file = os.path.join(base_path, 'lib', 'edit', 'artefact.txt')
    special_file = os.path.join(base_path, 'lib', 'edit', 'special.txt')
    
    if not os.path.exists(artefact_file) or not os.path.exists(special_file):
        print("ERROR: Could not find data files")
        return
    
    print("Building item table...")
    items = build_item_table_v2(artefact_file, special_file)
    print(f"Built table with {len(items)} item variants\n")
    
    # Test a few drops at depth 10 with verbose logging
    test_depth = 10
    print("=" * 80)
    print(f"Testing 5 drops at depth {test_depth} with verbose logging")
    print("=" * 80)
    print()
    
    for i in range(5):
        print(f"\n{'='*60}")
        print(f"DROP #{i+1}")
        print('='*60)
        
        quality = 'normal' if i < 3 else ('good' if i < 4 else 'great')
        item = simulate_drop(items, test_depth, quality, verbose=True)
        
        if item:
            print(f"\n>>> RESULT: {item.group_name}")
            print(f"    Type: {item.group_type}, Difficulty: {item.difficulty}, Rarity: {item.rarity}")
        else:
            print("\n>>> RESULT: No item generated")
    
    print("\n" + "=" * 80)
    print("Now compare this output with generation.txt from the game!")
    print("Look for DROP_TARGET, DROP_CANDIDATE, DROP_GROUP, DROP_ITEM_SELECT entries")
    print("=" * 80)

if __name__ == '__main__':
    main()
