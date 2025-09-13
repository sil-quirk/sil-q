# Quest Vault Regeneration Fix Test Plan

## Problem Summary
Quest vaults were successfully placed during initial level generation but disappeared during level regeneration due to connectivity failures. The root cause was that quest states (like `MANDOS_QUEST_GIVER_PRESENT`) were being set immediately during vault placement, but persisted through regeneration, causing subsequent regeneration attempts to skip quest vault placement.

## Fix Implementation
1. **Added pending quest state structure** to defer quest state changes
2. **Modified `process_quest_vault_area()`** to record but not immediately apply quest state changes
3. **Added `reset_pending_quest_states()`** called at start of each generation attempt
4. **Added `apply_pending_quest_states()`** called only when level generation is completely successful
5. **Updated quest vault tracking** to detect when pending quest changes indicate vault placement

## Key Changes Made

### 1. Pending Quest State Structure
```c
typedef struct {
    bool has_aule_change;
    bool has_mandos_change;
    int aule_level;
    int mandos_level;
    int aule_forge_y, aule_forge_x;
    int mandos_vault_y, mandos_vault_x;
} pending_quest_states_t;

static pending_quest_states_t pending_quest_states = {0};
```

### 2. Quest State Deferral in `process_quest_vault_area()`
**Before:**
```c
p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
p_ptr->quest_reserved[0] = 1;
```

**After:**
```c
pending_quest_states.has_aule_change = true;
pending_quest_states.aule_level = p_ptr->depth;
// Quest states applied only on successful level generation
```

### 3. Generation Loop Integration
- `reset_pending_quest_states()` called at start of each generation attempt
- `apply_pending_quest_states()` called only when `okay = true` (successful generation)

## Expected Behavior After Fix

### Before Fix (Problematic)
1. Quest vault placed → Quest state set immediately (`MANDOS_QUEST_GIVER_PRESENT`)
2. Level generation fails connectivity check
3. Regeneration attempt → Quest state still active → Quest vault NOT placed
4. Result: Quest vault disappears

### After Fix (Corrected)
1. Quest vault placed → Quest state change DEFERRED (not applied)
2. Level generation fails connectivity check  
3. Regeneration attempt → Quest state reset → Quest vault CAN be placed again
4. Level generation succeeds → Quest state changes APPLIED
5. Result: Quest vault persists through regeneration

## Test Validation Points

### 1. Log Messages to Verify Fix
Look for these log messages during testing:

**Generation Attempt Start:**
```
QUEST VAULT FIX: Starting level generation attempt (quest_vault_used=false)
```

**Quest State Deferral:**
```
Mandos quest: GIVER_PRESENT change DEFERRED (quest vault) at X,Y depth=Z
```

**Regeneration Reset:**
```
QUEST VAULT FIX: Level generation failed (connectivity), regenerating (quest_vault_used=false)
```

**Successful Application:**
```
Mandos quest: GIVER_PRESENT APPLIED (deferred from quest vault) at X,Y depth=Z
QUEST VAULT FIX: Level completely successful - setting quest_vault_used = 1
```

### 2. Quest Vault Persistence Test
- **Objective:** Verify quest vaults are re-placed during regeneration
- **Method:** Look for quest vault placement in multiple generation attempts
- **Success Criteria:** Quest vault appears in final level despite regeneration

### 3. Quest State Timing Test  
- **Objective:** Verify quest states are not set until level generation is completely successful
- **Method:** Check that quest giver interactions only work after successful level generation
- **Success Criteria:** No premature quest state changes during regeneration

## Test Scenarios

### Scenario 1: Force Quest Vault Regeneration
1. Enable debug logging
2. Create character at appropriate depth for quest vault
3. Use existing level generation that forces multiple attempts
4. Verify quest vault appears in final level

### Scenario 2: Manual Quest State Verification
1. Check save file before entering quest vault level
2. Verify quest state is `NOT_STARTED`
3. Enter level with quest vault
4. Verify quest state changes to `GIVER_PRESENT` only after successful generation

### Scenario 3: Connectivity Failure Simulation
1. Monitor log for connectivity check failures
2. Verify quest vaults are re-placed on regeneration attempts
3. Confirm quest states are only applied on successful generation

## Fix Validation Checklist

- [ ] Code compiles without errors
- [ ] Quest vault placement logic preserves existing functionality
- [ ] Quest state changes are properly deferred
- [ ] Regeneration resets quest state properly
- [ ] Successful generation applies quest states correctly
- [ ] Quest vault tracking works with pending states
- [ ] Log messages provide clear debugging information
- [ ] No regression in non-quest vault level generation

## Notes
- This fix maintains backward compatibility with existing save files
- The fix only affects quest vault regeneration behavior
- All existing quest mechanics remain unchanged once quest states are applied
- The pending quest state system could be extended for future quest types
