import re
from PIL import Image
import os
import math

def parse_styles(style_file):
    styles = {}
    current_style = None
    
    with open(style_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            if line.startswith('N:'):
                parts = line.split(':')
                idx = parts[1]
                name = parts[2].split('#')[0].strip() # Remove comments
                current_style = {'id': idx, 'name': name, 'W': None, 'F': [], 'D': []}
                styles[idx] = current_style
            
            elif current_style:
                if line.startswith('W:'):
                    # W:row:col
                    parts = line.split('#')[0].strip().split(':')
                    if len(parts) >= 3:
                        current_style['W'] = (int(parts[1]), int(parts[2]))
                
                elif line.startswith('F:'):
                    # F:row:col row:col ...
                    content = line[2:].split('#')[0].strip()
                    tokens = content.replace(',', ' ').split()
                    for token in tokens:
                        # Clean token of trailing colons or weird chars
                        token = token.strip(':')
                        if ':' in token:
                            parts = token.split(':')
                            if len(parts) >= 2:
                                r, c = parts[0], parts[1]
                                current_style['F'].append((int(r), int(c)))
                            
                elif line.startswith('D:'):
                    # D:row:col ...
                    content = line[2:].split('#')[0].strip()
                    tokens = content.replace(',', ' ').split()
                    for token in tokens:
                        token = token.strip(':')
                        if ':' in token:
                            parts = token.split(':')
                            if len(parts) >= 2:
                                r, c = parts[0], parts[1]
                                current_style['D'].append((int(r), int(c)))

    return styles

def get_tile_stats(img, row, col, tile_size=16):
    x = col * tile_size
    y = row * tile_size
    
    if x + tile_size > img.width or y + tile_size > img.height:
        return "OutOfBounds"
        
    tile = img.crop((x, y, x + tile_size, y + tile_size))
    pixels = list(tile.getdata())
    
    # Filter out transparent pixels if RGBA, or assume black is background if RGB?
    # The game uses a specific color key usually, but let's just analyze visible pixels.
    # Assuming standard RGB or RGBA.
    
    r_total, g_total, b_total = 0, 0, 0
    count = 0
    brightness_values = []
    
    for p in pixels:
        if len(p) == 4 and p[3] == 0: # Transparent
            continue
        
        r, g, b = p[0], p[1], p[2]
        # Simple brightness
        lum = 0.299*r + 0.587*g + 0.114*b
        
        r_total += r
        g_total += g
        b_total += b
        brightness_values.append(lum)
        count += 1
        
    if count == 0:
        return "Transparent"
        
    avg_r = int(r_total / count)
    avg_g = int(g_total / count)
    avg_b = int(b_total / count)
    
    avg_lum = sum(brightness_values) / count
    
    # Variance/Texture
    variance = sum((l - avg_lum) ** 2 for l in brightness_values) / count
    std_dev = math.sqrt(variance)
    
    # Determine color name approximation
    color_desc = get_color_name(avg_r, avg_g, avg_b)
    
    return {
        'color_name': color_desc,
        'rgb': (avg_r, avg_g, avg_b),
        'brightness': int(avg_lum),
        'texture': int(std_dev) # Higher means more contrasty/noisy (like bricks), Lower means smooth
    }

def get_color_name(r, g, b):
    # Very basic color bucketing
    if r < 40 and g < 40 and b < 40: return "Black/Dark"
    if r > 200 and g > 200 and b > 200: return "White/Bright"
    if abs(r-g) < 20 and abs(r-b) < 20: return "Grey"
    
    if r > g and r > b:
        if g > b and r < (g + 50): return "Orange/Brown"
        return "Red"
    if g > r and g > b:
        return "Green"
    if b > r and b > g:
        if r > g and b < (r + 50): return "Purple"
        return "Blue"
    
    if r > b and g > b: return "Yellow/Brown"
    if r > g and b > g: return "Purple/Magenta"
    if g > r and b > r: return "Cyan"
    
    return "Complex"

def analyze():
    base_path = r"c:\Users\efrem\Documents\GitHub\sil-qh"
    style_path = os.path.join(base_path, "lib", "edit", "style.txt")
    img_path = os.path.join(base_path, "lib", "xtra", "graf", "16x16.png")
    
    print(f"Parsing {style_path}...")
    styles = parse_styles(style_path)
    
    print(f"Loading {img_path}...")
    try:
        img = Image.open(img_path).convert('RGBA')
    except Exception as e:
        print(f"Error loading image: {e}")
        return

    print(f"Image size: {img.size}")

    print("\nAnalysis of Visual Styles (from 16x16.png):")
    print(f"{'ID':<4} {'Name':<20} | {'Wall Visual':<35} | {'Floor Visual':<35} | {'Door Visual':<35}")
    print("-" * 140)

    sorted_ids = sorted(styles.keys(), key=lambda x: int(x))
    
    for sid in sorted_ids:
        s = styles[sid]
        
        # Wall
        w_desc = "None"
        if s['W']:
            stats = get_tile_stats(img, s['W'][0], s['W'][1])
            if isinstance(stats, dict):
                tex = "Smooth" if stats['texture'] < 20 else "Rough" if stats['texture'] < 50 else "Noisy/Brick"
                w_desc = f"{stats['color_name']} ({tex}, Br:{stats['brightness']})"
            else:
                w_desc = str(stats)
        
        # Floor (take first)
        f_desc = "None"
        if s['F']:
            stats = get_tile_stats(img, s['F'][0][0], s['F'][0][1])
            if isinstance(stats, dict):
                tex = "Smooth" if stats['texture'] < 20 else "Rough" if stats['texture'] < 50 else "Noisy"
                f_desc = f"{stats['color_name']} ({tex}, Br:{stats['brightness']})"
            else:
                f_desc = str(stats)

        # Door (take first)
        d_desc = "None"
        if s['D']:
            stats = get_tile_stats(img, s['D'][0][0], s['D'][0][1])
            if isinstance(stats, dict):
                d_desc = f"{stats['color_name']} (Br:{stats['brightness']})"
            else:
                d_desc = str(stats)
                
        print(f"{sid:<4} {s['name']:<20} | {w_desc:<35} | {f_desc:<35} | {d_desc:<35}")

if __name__ == "__main__":
    analyze()
