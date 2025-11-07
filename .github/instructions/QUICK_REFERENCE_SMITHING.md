# Quick Reference: Smithing Costs

## ENCHANTABLE Items (Full List)
Only 2 items have the `ENCHANTABLE` flag:
1. **Robe** (soft armor) → -30% difficulty
2. **Quarterstaff** (hafted weapon) → -30% difficulty

---

## Minor Equipment Slots (+20% Difficulty Surcharge)
These 8 slots cost 20% MORE to smith for:

1. Left Hand (Rings)
2. Right Hand (Rings)  
3. Light Source (Lanterns)
4. Cloak (Capes)
5. Gloves (Gauntlets)
6. Boots
7. Quiver 1 (Arrows)
8. Quiver 2 (Arrows)

**NOT Minor** (no surcharge):
- Main hand weapon
- Chest armor
- Shield
- Helmet/Crown
- Amulet (neck)

---

## Ring vs Amulet: Why Rings are More Expensive

| Factor | Ring | Amulet |
|--------|------|--------|
| Equip Slot | LEFT/RIGHT (minor +20%) | NECK (no surcharge) |
| Protection Cost | +4 base + 1× per point | normal 3× per point |
| **Result** | **~20% more expensive** | **baseline** |

**Example:**
- Ring +1 Str → ~19-20 difficulty
- Amulet +1 Con → ~16-17 difficulty

The +20% equip slot surcharge is the primary difference!

---

## Difficulty Multipliers (Complete List)

### DISCOUNTS (multiply final difficulty):
- Robes/Quarterstaff (ENCHANTABLE): **-30%** (×0.70)
- Telchar + Sharpness/Accurate: **-25%** (×0.75)
- Feanor + Fire/Light brand: **-25%** (×0.75)
- **Feanor + Lamp: -40% (×0.60)** ← BIGGEST DISCOUNT!

### SURCHARGES (multiply final difficulty):
- Minor equipment slots (left ring, gloves, boots, etc.): **+20%** (×1.20)

---

## dif_mod Formula

```c
void dif_mod(int value, int positive_base, int* dif_inc)
{
    int mod = 1 + ((positive_base - 1) / 5);
    
    if (value > 0) {
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
}
```

Examples:
- `dif_mod(1, 14)` → adds 14
- `dif_mod(2, 14)` → adds 31 (14×2 + 3×1 = 28+3)
- `dif_mod(3, 14)` → adds 51 (14×3 + 3×3 = 42+9)

Stat costs scale **quadratically** (triangular number pattern).
