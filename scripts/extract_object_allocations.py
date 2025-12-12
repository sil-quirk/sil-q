#!/usr/bin/env python3
"""
Extract object names and A: field allocations from object.txt
and output as CSV.
"""

import csv
import re

def parse_object_txt(filepath):
    """Parse object.txt and extract objects with A: allocations."""
    objects = []
    current = None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip()
            
            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue
            
            # Parse N: line - object definition
            if line.startswith('N:'):
                if current:
                    objects.append(current)
                parts = line[2:].split(':', 1)
                idx = int(parts[0])
                # Extract name from "& name~" format
                name_raw = parts[1].strip() if len(parts) > 1 else ""
                # Clean up the name - remove & and ~
                name = name_raw.replace('&', '').replace('~', '').strip()
                current = {
                    'idx': idx,
                    'name': name,
                    'allocations': []
                }
            
            # Parse A: line - allocations
            elif line.startswith('A:') and current:
                # Format: A:depth/rarity:depth/rarity:...
                alloc_str = line[2:]
                # Split by : to get individual allocations
                parts = alloc_str.split(':')
                for part in parts:
                    if '/' in part:
                        depth, rarity = part.split('/')
                        try:
                            current['allocations'].append({
                                'depth': int(depth),
                                'rarity': int(rarity)
                            })
                        except ValueError:
                            pass
    
    # Don't forget the last entry
    if current:
        objects.append(current)
    
    return objects

def main():
    filepath = r'../lib/edit/object.txt'
    objects = parse_object_txt(filepath)
    
    # Find the maximum number of allocations for any object
    max_allocs = max((len(obj['allocations']) for obj in objects), default=0)
    
    # Output as CSV
    output_file = r'../scripts/output/object_allocations.csv'
    
    with open(output_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Create header with dynamic columns for allocations
        header = ['Index', 'Object Name']
        for i in range(max_allocs):
            header.append(f'Depth {i+1}')
            header.append(f'Rarity {i+1}')
        writer.writerow(header)
        
        for obj in objects:
            if obj['allocations']:
                row = [obj['idx'], obj['name']]
                for alloc in obj['allocations']:
                    row.append(alloc['depth'])
                    row.append(alloc['rarity'])
                writer.writerow(row)
    
    print(f"Extracted {len(objects)} objects")
    print(f"Written to {output_file}")
    
    # Print summary
    total_allocations = sum(len(obj['allocations']) for obj in objects)
    objects_with_alloc = sum(1 for obj in objects if obj['allocations'])
    print(f"Objects with allocations: {objects_with_alloc}")
    print(f"Total allocations: {total_allocations}")

if __name__ == '__main__':
    main()
