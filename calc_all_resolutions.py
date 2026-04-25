#!/usr/bin/env python3
"""
Calculate optimal SDL configuration for all requested resolutions.
Priority: MAXIMUM SCALE (up to 3) first, then fit panes if possible.
"""

# All requested resolutions
RESOLUTIONS = [
    (800, 600),
    (1024, 768),
    (1152, 864),
    (1280, 720),
    (1280, 768),
    (1280, 800),
    (1280, 1024),
    (1360, 768),
    (1366, 768),
    (1400, 1050),
    (1440, 900),
    (1536, 864),
    (1600, 900),
    (1600, 1200),
    (1680, 1050),
    (1920, 1080),
    (1920, 1200),
    (2048, 1152),
    (2160, 1440),
    (2560, 1080),
    (2560, 1440),
    (2560, 1600),
    (2736, 1824),
    (2880, 1620),
    (2880, 1800),
    (3000, 2000),
    (3200, 1800),
    (3240, 2160),
    (3440, 1440),
    (3840, 1080),
    (3840, 1200),
    (3840, 1440),
    (3840, 1600),
    (3840, 2160),
    (4096, 2160),
    (4480, 1440),
    (5120, 1440),
    (5120, 2160),
    (5120, 2880),
    (6016, 3384),
    (7680, 4320),
]

# Constants
MIN_MAIN_COLS = 40  # minimum main terminal columns in tile mode
MIN_MAIN_ROWS = 24  # minimum main terminal rows
TILE_SIZE = 16  # base tile size at scale 1
MIN_RIGHT_COLS = 40  # minimum columns for right pane
MAX_BOTTOM_ROWS = 4  # maximum rows for bottom pane
CHAR_WIDTH_RATIO = 0.6  # character width as ratio of font height

def calculate_config(width, height):
    """Calculate optimal configuration for given resolution."""
    
    # 1. Determine MAXIMUM scale (up to 3) that fits main terminal
    scale = 0
    for s in [3, 2, 1]:
        main_width = MIN_MAIN_COLS * TILE_SIZE * s
        main_height = MIN_MAIN_ROWS * TILE_SIZE * s
        if main_width <= width and main_height <= height:
            scale = s
            break
    
    if scale == 0:
        print(f"SKIP: {width}x{height} - too small for minimum terminal!")
        return None
    
    # 2. Calculate main terminal minimum size in pixels
    main_min_width = MIN_MAIN_COLS * TILE_SIZE * scale
    main_min_height = MIN_MAIN_ROWS * TILE_SIZE * scale
    
    # 3. Aux view font size (approximately half of tile size)
    aux_font_size = {3: 18, 2: 16, 1: 9}[scale]
    
    # 4. Check if we can fit right pane (40+ columns)
    char_width = int(aux_font_size * CHAR_WIDTH_RATIO)
    if char_width == 0:
        char_width = 1
    remaining_width = width - main_min_width
    available_cols = remaining_width // char_width
    
    has_right_pane = available_cols >= MIN_RIGHT_COLS
    right_cols = 0
    if has_right_pane:
        # Use 50 cols if we have space, otherwise 40
        right_cols = 50 if available_cols >= 50 else 40
    
    # 5. Check if we can fit bottom pane (at least 1 row)
    remaining_height = height - main_min_height
    available_rows = remaining_height // aux_font_size if aux_font_size > 0 else 0
    
    has_bottom_pane = available_rows >= 1
    bottom_rows = 0
    if has_bottom_pane:
        bottom_rows = min(available_rows, MAX_BOTTOM_ROWS)
    
    config = {
        'width': width,
        'height': height,
        'scale': scale,
        'aux_font_size': aux_font_size,
        'has_right_pane': has_right_pane,
        'right_cols': right_cols,
        'has_bottom_pane': has_bottom_pane,
        'bottom_rows': bottom_rows,
    }
    
    return config

def get_resolution_name(width, height):
    """Get a friendly name for resolution."""
    names = {
        (800, 600): "SVGA",
        (1024, 768): "XGA",
        (1280, 720): "HD 720p",
        (1280, 800): "WXGA",
        (1280, 1024): "SXGA",
        (1366, 768): "HD",
        (1600, 900): "HD+",
        (1600, 1200): "UXGA",
        (1920, 1080): "Full HD",
        (1920, 1200): "WUXGA",
        (2560, 1440): "QHD",
        (2560, 1600): "MacBook 13\"",
        (2880, 1800): "MacBook 15\"",
        (3840, 2160): "4K UHD",
        (5120, 2880): "5K",
        (7680, 4320): "8K UHD",
    }
    return names.get((width, height), f"{width}x{height}")

def generate_c_code(configs):
    """Generate C code for resolution profiles."""
    lines = []
    
    for cfg in configs:
        if cfg is None:
            continue
        
        name = get_resolution_name(cfg['width'], cfg['height'])
        lines.append(f"    // {cfg['width']}x{cfg['height']} ({name})")
        lines.append("    {")
        lines.append(f"        .width = {cfg['width']},")
        lines.append(f"        .height = {cfg['height']},")
        lines.append(f"        .name = \"{cfg['width']}x{cfg['height']} ({name})\",")
        lines.append(f"        .main_view_scale = {cfg['scale']},")
        lines.append(f"        .aux_view_font_size = {cfg['aux_font_size']},")
        
        # Build panes array
        panes = []
        
        if cfg['has_right_pane']:
            panes.append(('PANE_INVENTORY', 'PLACE_RIGHT', 22, cfg['right_cols']))
            panes.append(('PANE_WORN', 'PLACE_RIGHT', 17, 0))
            panes.append(('PANE_INFO', 'PLACE_RIGHT', 0, 0))
        
        if cfg['has_bottom_pane']:
            panes.append(('PANE_ROLLS', 'PLACE_BOTTOM', cfg['bottom_rows'], 0))
            panes.append(('PANE_LOG', 'PLACE_BOTTOM', 0, 0))
        
        lines.append(f"        .pane_count = {len(panes)},")
        lines.append("        .panes = {")
        
        if panes:
            for pane in panes:
                pane_type, placement, rows, cols = pane
                lines.append(f"            {{ {pane_type}, {placement}, {rows}, {cols}  }},")
        
        lines.append("        }")
        lines.append("    },")
        lines.append("")
    
    return '\n'.join(lines)

if __name__ == '__main__':
    print("Calculating SDL configurations for all resolutions...")
    print("Priority: MAXIMUM SCALE (up to 3)")
    print("=" * 80)
    
    configs = []
    for width, height in RESOLUTIONS:
        cfg = calculate_config(width, height)
        if cfg:
            configs.append(cfg)
            name = get_resolution_name(width, height)
            panes_info = []
            if cfg['has_right_pane']:
                panes_info.append(f"R{cfg['right_cols']}")
            if cfg['has_bottom_pane']:
                panes_info.append(f"B{cfg['bottom_rows']}")
            panes_str = "+".join(panes_info) if panes_info else "No panes"
            print(f"{width:4}x{height:4} {name:20} -> Scale {cfg['scale']}, Font {cfg['aux_font_size']:2}, {panes_str}")
    
    print("\n" + "=" * 80)
    print(f"\nTotal configurations: {len(configs)}")
    print("\n" + "=" * 80)
    print("\nGenerated C code:\n")
    print(generate_c_code(configs))
