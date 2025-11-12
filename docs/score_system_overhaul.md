# Score / Statistics Database Overhaul

This document tracks the binary redesign for Sil-QH's end-of-run data. It
supersedes the ad-hoc `scores.raw` text-within-binary file by introducing a
modular statistics database that cleanly separates metaruns, individual runs,
characters, and monster analytics.

## Goals
- Replace the legacy `scores.raw` layout with a versioned binary schema that
  captures the full state of a run (not just the rendered score string).
- Keep metarun state (`meta.raw`) authoritative while providing explicit links
  from each score entry back to its metarun and character.
- Add dedicated stores for character histories and monster interaction stats so
  future UX/reporting features can query holistic data without parsing saves.
- Maintain backwards compatibility: legacy files are imported automatically and
  archived; new files are atomic, checksummed, and easy to back up.

## Data Architecture

| File | Purpose | Notes |
| ---- | ------- | ----- |
| `meta.raw` | Metarun campaign state | Remains fixed-size per metarun; new fields reference the statistics DB via IDs. |
| `runs.db` (replacement for `scores.raw`) | Run statistics database (scoreboard) | Contains a header plus `score_record_v1` entries (see `src/score/score_format.h`). Acts as the canonical run history for scoring, autoload, and analytics. |
| `characters.db` | Character identity rollup | Each hero (name + GUID) has cumulative stats: best score, total silmarils, deaths, escapes, last race/house, etc. |
| `monsters.db` | Monster analytics | Tracks “seen”, “killed”, and “killed by” counts using stable GUIDs exported from `monster.txt`. |

### Identifier Strategy & GUID Generation

- **Runs:** Every `score_record_v1` carries a `record_id` (monotonic integer) plus the owning `metarun_id`. Records stay stable even if files are compacted.
- **Characters:** Characters are keyed by their *normalized name* (trimmed, lowercase, collapsed whitespace). We derive `character_id` via a 64-bit FNV-1a hash of the normalized name and persist that alongside the raw string. If the player reuses the same name, new runs automatically roll up into the same `characters.db` entry. If we ever need to split two heroes with the same spelling, we can seed the hash with the generation timestamp or expose a “rename” tool that rewrites the ID.
- **Monsters:** Today, saves reference monsters by `r_idx` (see `save.c:570-640` for `wr_monster()` and the matching loader). The new analytics layer needs stability even when the `r_info` array changes, so every monster in `lib/edit/monster.txt` will receive an explicit `# guid:` line. GUIDs should be 16 hex characters (64 bits). Tooling can generate them automatically, but data authors can also copy one from the helper script output. When adding a monster: 1) run `tools/make_guid.py` (to be added) or any UUID generator, 2) paste it into the monster definition, 3) rebuild – the GUID becomes the authoritative ID for saves/analytics while `r_idx` stays a transient array index.
- **Races/Houses/Quests:** Follow the same pattern as monsters. Each entry gets a GUID tag in its `lib/edit/*.txt` definition; `score_record_v1` stores the numeric index for compatibility plus the GUID via lookup tables, so remapping data later won’t orphan historical stats.

This scheme keeps developer ergonomics straightforward: if you add new data, assign a GUID once and never change it. The loaders will fall back to the stored name if a GUID is missing, but CI will start warning as soon as we wire the validator.

### Run Statistics (Runs DB)
Stored as `score_record_v1` entries featuring:
- IDs for metarun (`metarun_id`), character (`character_id`), and record order.
- Timeline data (start/end timestamps, turns spent).
- Gameplay snapshot: silmarils, depth reached/left, unique kills, quests,
  skills/abilities purchased, curses, XP, kill/seen totals.
- Outcome markers: alive/dead/escaped, run flags (Morgoth slain, noscore, cheat).
- Killer metadata: kind (monster, trap, fall, self, other), stable GUID, race
  fallback, textual display strings, and cause codes.
- Savefile hint plus reserved bytes for future per-run blobs (e.g., map seeds).

### Character Database
- Primary key is the normalized character name hash described above; the record
  also stores the canonical spelling so UI code can present it exactly as the
  player typed it. Optional future fields can track per-house variants if we
  decide to let the same name exist in multiple houses intentionally.
- Stores ancestry (latest race/house descriptor), run counters, total/best
  score, turn totals, depth milestones, silmarils, blessing points, etc., so we
  can show “lifetime with this hero” stats every time a run ends.

### Monster Statistics
- One entry per monster GUID. Tracks cumulative player interactions: seen/killed
  counts, kills inflicted on players, banishments, and reserved fields for
  quest-specific tallies.
- Enables features like “Most lethal monsters this metarun” or balancing passes
  based on real telemetry.

## Phased Implementation

### Phase 1 – Format & Architecture (in progress)
1. **Schema definitions:** Land shared headers describing `score_db_header`,
   `score_record_v1`, `score_character_record_v1`, and `score_monster_stats_v1`
   (see `src/score/score_format.h`). These represent the typed binary layout.
2. **Legacy audit:** Document the invariants of `scores.raw`/`meta.raw` (done in
   `session_notes.md`) so we know which code paths require parity.
3. **GUID plumbing:** Assign persistent GUIDs to monsters, races, houses, and
   characters; the record structs already provide slots for these IDs.
4. **Migration plan:** Design a converter that reads `scores.raw`, produces
   typed run records, and writes `runs.db` atomically while archiving the source.

### Phase 2 – Module Split (in progress)
1. **Score I/O module:** Move the score-file context and low-level helpers out
   of `src/files.c`. `score/score_io.c` now owns the singleton plumbing; future
   steps will relocate `open_scores_file_versioned`, header parsing, and backup
   logic into this module.
2. **Logic module:** Extract scoring math (`score_breakdown`, `score_points`,
   sort helpers) into `score/score_logic.c` (planned). Callers will link against
   a dedicated `score.h` instead of reaching into `files.c`.
3. **UI/API split:** The UI (`show_scores`, `show_scores_interactive`) will sit
   in `score/score_ui.c`, consuming the public API rather than touching file
   descriptors directly.

### Phase 3 – Dual Format Support
- Add serializers/deserializers for `score_record_v1`.
- Populate both `scores.raw` and `runs.db` during a grace period; diff the
  derived statistics to guarantee parity.
- Ship a CLI / in-game migration command to convert historical archives.

### Phase 4 – Feature Expansion
- Enable the character + monster databases, update metarun summaries to consume
  them, and expose richer dashboards (quests completed, kill ratios, etc.).
- Revisit metarun score formulas using the richer dataset instead of parsing
  text fields.

### Phase 5 – Cleanup
- Drop legacy ASCII fields after one release of dual-write.
- Consolidate backups (shared timestamped archives for `meta.raw`, `runs.db`,
  `characters.db`, `monsters.db`).
- Add integrity tooling: per-record checksums, header CRC, and repair utilities.

## Testing Strategy
- **Unit coverage:** Score parsing/conversion must have golden tests that assert
  total counts match the legacy pipeline.
- **Integration:** Run end-to-end metarun flows (start run, die, escape) and
  verify `runs.db`, `characters.db`, and `monsters.db` entries update as
  expected. Autoload logic should continue to honor alive entries.
- **Migration:** Use curated `scores.raw` snapshots to test the converter,
  ensuring killer metadata, depth, silmarils, and scores match.
- **Backups:** Exercise file rotation (bak1/bak2/bak3) and verify new databases
  integrate seamlessly with existing `init2.c` seeding logic.

## Open Questions
- Where should per-run narrative artifacts (combat logs, screenshots) live?
- Do we embed monster GUIDs directly in `monster.txt`, or generate them from
  names? (Preferred: explicit `# guid:` entries to avoid renaming collisions.)
- Should the monster stats table record per-metarun counters or only lifetime
  totals? (Plan: track lifetime totals globally, with optional per-metarun
  overlays stored alongside the metarun entry.)
