# Metarun Blessing System Overview

## Campaign Loss Condition

- The metarun no longer ends after a fixed number of deaths.
- Defeat now occurs when the number of living characters (tracked via the score file) is **strictly less** than the number of Silmarils still required to win, divided by three (rounded up).
- Example: with two Silmarils left to recover, at least one living character must remain. With three Silmarils left, at least one character is also required; with four or five, at least two, etc.
- Death-boosting curses still increase the required survivors, while future minor blessings can now ease the requirement.

## Blessing Point Economy

- Whenever a campaign death occurs, the fallen character’s score is added to the metarun’s “fallen score pool”. Gift of Eru interventions still spare the death counter but now contribute their score to the blessing pool.
- Every **300** points accumulated unlocks **one blessing point**, with any excess tracked toward the next threshold.
- Accumulated totals and available points are shown in the metarun statistics screen.

### Spending Points

- From the statistics screen press **b** to enter the blessing exchange.
- **Remove a curse** (cost 1): choose any active curse and wipe all stacks.
- **Gain a minor blessing** (cost 1): choose one of three random, eligible blessings. Options never include a curse that is currently active or has no positive effect.
- **Unlock a major blessing**: choose from the remaining entries defined in `lib/edit/blessing.txt`. Each record sets its own cost (currently 3), menu summary, and unlock message. The default data ships with:
  1. **Supply Covenant** - raises the supplies cache limit to 50 lbs (tenths).
  2. **Artefact Patronage** - every new hero begins with a randomly selected artefact from depths = 10.
- After unlocking a major blessing the effects are applied immediately (supply limit) or at the start of the next character (artefact patronage).
## Curse and Blessing Architecture

- Meta-run curse stacks now use signed values. Positive stacks represent curses; negative stacks represent blessings.
- All curse displays now show both curses and blessings, with blessings highlighted in green.
- `curses.txt` has been extended with `B:`, `E:`, `H:`, and `G:` directives to describe blessing names, flavour text, power text, and RHF flags respectively. Stat penalties automatically invert via existing `S:` values.
- The parser and data structures (`curse_type`) include dedicated blessing fields, and new helper functions expose blessing names/descriptions for UI.
- Major blessings are now data-driven via `lib/edit/blessing.txt`, enabling new campaign-wide upgrades without recompilation.

## Game Effects

- Blessing RHF/CUR flags are now respected. The existing flag-count helpers sum both curse and blessing contributions.
- Stat penalties/bonuses already invert automatically through signed stack counts. Skill affinities rely on the new `G:` fields.
- Supplying blessing points and spending them triggers saving of the metarun state and reapplication of runtime effects (supply cache limits, alive-count cache, etc.).
- The blessing exchange menu recalculates available points on each visit, spends through the cumulative ledger, and reapplies campaign-wide effects immediately after every purchase.

## Additional Changes

- Awarded blessing points are reported when returning from death/escape sequences.
- The metarun statistics screen shows blessing totals, pool progress, and the current count of living heroes.
- Gift of Eru now reduces the scoring power value by one (bottoming out at zero), increasing final scores for those characters.
- The statistics screen now renders Silmaril and death progress as symbol bars and only lists major blessings that have been unlocked.
- Fresh runs count the currently active hero toward the "Living Heroes" total even before the first save, avoiding the zero-alive display bug.

