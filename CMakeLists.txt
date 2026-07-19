cmake_minimum_required(VERSION 3.18)
project(sil-more C)

# Project language standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SIL_BUILD_WITH_SDL_SOURCES_DEFAULT OFF)
# On macOS, Homebrew ships sdl3 / sdl3_image / sdl3_ttf but no sdl3_mixer,
# and its sdl3 lags the version our vendored SDL_mixer pins — so there is no
# working mixed-mode configuration. Default APPLE (macOS + iOS) to the
# source build alongside Windows/Android.
if(WIN32 OR ANDROID OR APPLE)
  set(SIL_BUILD_WITH_SDL_SOURCES_DEFAULT ON)
endif()

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)
option(SIL_USE_LOCAL_DATA "Store generated data inside the game directory (debug builds)" OFF)
option(SIL_BUILD_WITH_SDL_SOURCES "Build SDL dependencies from pinned sources under external/ (recommended for Windows and Android)" ${SIL_BUILD_WITH_SDL_SOURCES_DEFAULT})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(ANDROID)
  # Ensure Android logging symbols are resolvable for SDL and dependent shared libs.
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")
endif()

# Detect iOS when the CMake system name is set (via -DCMAKE_SYSTEM_NAME=iOS)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(SIL_IOS ON)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
  endif()
endif()

if(SIL_IOS AND SIL_USE_LOCAL_DATA)
  message(WARNING "SIL_USE_LOCAL_DATA is not writable in an iOS app bundle; forcing it OFF so saves/configs use SDL_GetPrefPath().")
  set(SIL_USE_LOCAL_DATA OFF CACHE BOOL "Store generated data inside the game directory (debug builds)" FORCE)
endif()

set(SIL_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/defines.h"
)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/defines.h" SIL_VERSION_STRING_LINE
  REGEX "^#define[ \t]+VERSION_STRING[ \t]+\"[^\"]+\""
)
if(NOT SIL_VERSION_STRING_LINE)
  message(FATAL_ERROR "Could not read VERSION_STRING from src/defines.h")
endif()
string(REGEX REPLACE
  "^#define[ \t]+VERSION_STRING[ \t]+\"([^\"]+)\".*$"
  "\\1"
  SIL_VERSION_STRING
  "${SIL_VERSION_STRING_LINE}"
)

# Configure SDL_mixer when built from the vendored submodule: enable only the
# codecs we actually use (WAV + Ogg Vorbis via stb_vorbis) and keep the build
# self-contained so it works on every platform without system codec packages.
macro(_sil_configure_vendored_sdl_mixer)
  set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_STRICT OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLMIXER_AIFF OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VOC OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_AU OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVE ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
endmacro()

if (SIL_BUILD_WITH_SDL_SOURCES)

  # iOS has no system FreeType/HarfBuzz; force SDL_ttf to build from its
  # vendored copies.  Android already defaults to vendored via MSVC logic
  # in SDL_ttf, but set it explicitly here for clarity.
  if(SIL_IOS OR ANDROID)
    set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored freetype/harfbuzz for mobile" FORCE)
  endif()

  # Build SDL as static libraries on iOS to avoid code-signing issues
  # with dynamic frameworks and to simplify the app bundle.
  if(SIL_IOS)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3IMAGE_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3IMAGE_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3TTF_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_STATIC ON CACHE BOOL "" FORCE)
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL" "${CMAKE_BINARY_DIR}/_deps/SDL")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  # When building SDL as static on iOS, SDL_image and SDL_ttf call
  # find_package(SDL3) internally, but the export files don't exist yet
  # (they're generated at install time).  Write a minimal shim that
  # declares the package found - the actual targets are already visible
  # from add_subdirectory above.
  if(SIL_IOS)
    set(_sdl3_shim_dir "${CMAKE_BINARY_DIR}/_sdl3_shim")
    file(MAKE_DIRECTORY "${_sdl3_shim_dir}")
    file(WRITE "${_sdl3_shim_dir}/SDL3Config.cmake"
      "set(SDL3_FOUND TRUE)\n"
      "set(SDL3_VERSION \"${SDL_VERSION}\")\n"
      "set(SDL3_SDL3-static_FOUND TRUE)\n"
      "set(SDL3_Headers_FOUND TRUE)\n"
      "# Targets already defined by add_subdirectory\n"
    )
    file(WRITE "${_sdl3_shim_dir}/SDL3ConfigVersion.cmake"
      "set(PACKAGE_VERSION \"${SDL_VERSION}\")\n"
      "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
      "set(PACKAGE_VERSION_EXACT FALSE)\n"
    )
    set(SDL3_DIR "${_sdl3_shim_dir}" CACHE PATH "" FORCE)
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_image/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_image" "${CMAKE_BINARY_DIR}/_deps/SDL_image")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_image is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_ttf/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_ttf" "${CMAKE_BINARY_DIR}/_deps/SDL_ttf")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_ttf is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_mixer/CMakeLists.txt")
    _sil_configure_vendored_sdl_mixer()
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_mixer" "${CMAKE_BINARY_DIR}/_deps/SDL_mixer")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_mixer is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (ANDROID AND TARGET SDL3-shared)
    # Needed for __android_log_print / __android_log_write in SDL Android backend.
    target_link_libraries(SDL3-shared PRIVATE log)
  endif()
else()
  find_package(SDL3 REQUIRED)
  find_package(SDL3_ttf REQUIRED)
  find_package(SDL3_image REQUIRED)
  # Homebrew (and several other distros) do not ship an SDL3_mixer package
  # yet, so fall back to building the vendored submodule when no system
  # package is present.
  find_package(SDL3_mixer QUIET)
  if(NOT SDL3_mixer_FOUND)
    if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_mixer/CMakeLists.txt")
      _sil_configure_vendored_sdl_mixer()
      add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_mixer" "${CMAKE_BINARY_DIR}/_deps/SDL_mixer")
    else()
      message(FATAL_ERROR "SDL3_mixer was not found as an installed package and external/SDL_mixer is missing. Run 'git submodule update --init --recursive' to build it from source, or install SDL3_mixer system-wide.")
    endif()
  endif()
endif()

add_subdirectory(src/log)

set(SIL_MORE_SOURCES
    src/blitz.c
    src/birth/birth-allocation.c
    src/birth/birth-blitz.c
    src/birth/birth-choice-ui.c
    src/birth/birth-flow.c
    src/birth/birth-oath.c
    src/birth/birth-selection.c
    src/birth/birth-setup.c
    src/birth/birth-skills.c
    src/birth/birth-traits.c
    src/birth/birth-ui-text.c
    src/cave/cave-awareness.c
    src/cave/cave-flow.c
    src/cave/cave-geometry.c
    src/cave/cave-map-ui.c
    src/cave/cave-projection.c
    src/cave/cave-styles.c
    src/cave/cave-view.c
    src/cave/cave-visuals.c
    src/cJSON.c
    src/cmd/combat/cmd-combat.c
    src/cmd/combat/cmd-ranged.c
    src/cmd/item/cmd-fletchery.c
    src/cmd/item/cmd-identify.c
    src/cmd/item/cmd-item-activate.c
    src/cmd/item/cmd-item-core.c
    src/cmd/item/cmd-item-utility.c
    src/cmd/item/cmd-pickup.c
    src/cmd/monster/cmd-monster-alert.c
    src/cmd/movement/cmd-depth-bonus.c
    src/cmd/movement/cmd-movement.c
    src/cmd/movement/cmd-run.c
    src/cmd/ui/cmd-ui-abilities.c
    src/cmd/ui/cmd-ui-character.c
    src/cmd/ui/cmd-ui-common.c
    src/cmd/ui/cmd-ui-knowledge.c
    src/cmd/ui/cmd-ui-look.c
    src/cmd/ui/cmd-ui-main-menu.c
    src/cmd/ui/cmd-ui-nearby.c
    src/cmd/ui/cmd-ui-object-display.c
    src/cmd/ui/cmd-ui-settings.c
    src/cmd/ui/cmd-ui-smithing.c
    src/cmd/world/cmd-interact-chest.c
    src/cmd/world/cmd-interact.c
    src/dungeon/dungeon-command.c
    src/dungeon/dungeon-identify.c
    src/dungeon/dungeon-loop.c
    src/dungeon/dungeon-morgoth.c
    src/dungeon/dungeon-narrative.c
    src/dungeon/dungeon-player.c
    src/dungeon/dungeon-spectator.c
    src/dungeon/dungeon-startup.c
    src/dungeon/dungeon-state.c
    src/dungeon/dungeon-story.c
    src/dungeon/dungeon-world.c
    src/fs/file.c
    src/fs/savefile-name.c
    src/game/game-lifecycle.c
    src/metarun/metarun-files.c
    src/platform-signals.c
    src/score/score_entry.c
    src/score/score_paths.c
    src/score/score_postmortem.c
    src/ui/character-dump.c
    src/ui/character-screen.c
    src/ui/file-viewer.c
    src/ui/help.c
    src/ui/story.c
    src/format.c
    src/level-generation/level-generation.c
    src/level-generation/level-generation-access.c
    src/level-generation/level-generation-finalize.c
    src/level-generation/level-generation-layout-anchors.c
    src/level-generation/level-generation-layout-big-cave.c
    src/level-generation/level-generation-layout-caves.c
    src/level-generation/level-generation-layout-connectivity.c
    src/level-generation/level-generation-layout-doors.c
    src/level-generation/level-generation-layout-modes.c
    src/level-generation/level-generation-layout-room-placement.c
    src/level-generation/level-generation-partition-population.c
    src/level-generation/level-generation-population.c
    src/level-generation/level-generation-quests.c
    src/level-generation/level-generation-room-build.c
    src/level-generation/level-generation-rooms-basic.c
    src/level-generation/level-generation-rooms-morgoth.c
    src/level-generation/level-generation-rooms-special.c
    src/level-generation/level-generation-rooms-vaults.c
    src/level-generation/level-generation-screen.c
    src/level-generation/level-generation-state.c
    src/level-generation/level-generation-terrain-connectivity.c
    src/level-generation/level-generation-terrain-features.c
    src/level-generation/level-generation-terrain-placement.c
    src/level-generation/level-generation-terrain-tunnels.c
    src/level-generation/level-generation-vault-special.c
    src/gen-log.c
    src/init/init-arrays.c
    src/init/init-cleanup.c
    src/init/init-flags.c
    src/init/init-info.c
    src/init/init-lifecycle.c
    src/init/init-object-bonuses.c
    src/init/init-parse-ability.c
    src/init/init-parse-artefact.c
    src/init/init-parse-curses.c
    src/init/init-parse-effects.c
    src/init/init-parse-ego.c
    src/init/init-parse-flavor.c
    src/init/init-parse-history.c
    src/init/init-parse-limits.c
    src/init/init-parse-major-blessings.c
    src/init/init-parse-monster.c
    src/init/init-parse-names.c
    src/init/init-parse-oaths.c
    src/init/init-parse-object-kind.c
    src/init/init-parse-player.c
    src/init/init-parse-quests.c
    src/init/init-parse-runtypes.c
    src/init/init-parse-skeleton-notes.c
    src/init/init-parse-story.c
    src/init/init-parse-terrain.c
    src/init/init-parse-vault.c
    src/init/init-parser-core.c
    src/init/init-paths.c
    src/init/init-style.c
    src/item_set.c
    src/fs/load.c
    src/fs/load-player.c
    src/fs/load-notes-inventory.c
    src/fs/load-dungeon.c
    src/main.c
    src/main-sdl.c
    src/sdl/config/sdl-settings.c
    src/sdl/core/sdl-events.c
    src/sdl/core/sdl-init.c
    src/sdl/core/sdl-layout.c
    src/sdl/core/sdl-state.c
    src/sdl/input/sdl-gamepad.c
    src/sdl/input/sdl-map-geometry.c
    src/sdl/input/sdl-mouse-path.c
    src/sdl/input/sdl-player-actions.c
    src/sdl/input/sdl-pointer-targeting.c
    src/sdl/input/sdl-screen-pointer.c
    src/sdl/input/sdl-tooltips.c
    src/sdl/input/sdl-touch-controls.c
    src/sdl/input/sdl-touch-tutorial.c
    src/sdl/render/sdl-fonts.c
    src/sdl/render/sdl-present.c
    src/sdl/render/sdl-term-callbacks.c
    src/sdl/render/sdl-text.c
      src/sdl/ui/sdl-main-menu.c
      src/sdl/ui/sdl-halls-screen.c
      src/sdl/ui/sdl-hint-quest-menu.c
      src/sdl/ui/sdl-menus.c
    src/sdl/ui/sdl-pane-menus.c
    src/sdl/ui/sdl-panes.c
    src/sdl/ui/sdl-screens.c
    src/sdl/ui/sdl-song-menu.c
    src/sdl/ui/sdl-question-menu.c
    src/melee/melee-attack.c
    src/melee/melee-attack-ranged.c
    src/melee/melee-combat-display.c
    src/melee/melee-movement.c
    src/melee/melee-movement-path.c
    src/melee/melee-movement-resolution.c
    src/melee/melee-process.c
    src/melee/melee-util.c
    src/metarun/metarun-artefact-memory.c
    src/metarun/metarun-blessing-ui.c
    src/metarun/metarun-curses.c
    src/metarun/metarun-difficulty-ui.c
    src/metarun/metarun-effects-ui.c
    src/metarun/metarun-history-ui.c
    src/metarun/metarun-lifecycle.c
    src/metarun/metarun-oaths.c
    src/metarun/metarun-persistence.c
    src/metarun/metarun-score.c
    src/metarun/metarun-settings.c
    src/metarun/metarun-state.c
    src/metarun/metarun-stats-ui.c
    src/metarun/metarun-ui-common.c
    src/metarun_legacy.c
    src/quest.c
    src/monster/monster-attr.c
    src/monster/monster-desc.c
    src/monster/monster-list.c
    src/monster/monster-lore.c
    src/monster/monster-lore-combat.c
    src/monster/monster-move.c
    src/monster/monster-recall.c
    src/monster/monster-select.c
    src/monster/monster-spawn.c
    src/monster/monster-summon.c
    src/monster/monster-update.c
    src/object/object-allocation.c
    src/object/object-desc.c
    src/object/object-flags.c
    src/object/object-flavor.c
    src/object/object-info.c
    src/object/object-inventory.c
    src/object/object-inventory-limits.c
    src/object/object-knowledge.c
    src/object/object-list.c
    src/object/object-make.c
    src/object/object-autoinscribe.c
    src/object/object-place.c
    src/object/object-pval.c
    src/object/object-ui-core.c
    src/object/object-ui-enhanced.c
    src/object/object-ui-identify.c
    src/object/object-ui-select.c
    src/object/object-use.c
    src/object/object-util.c
    src/drop_system.c
    src/thrall_quest.c
    src/player/ability-log.c
    src/player/player-active-weapon.c
    src/player/combat-stats.c
    src/player/effects.c
    src/player/experience.c
    src/player/killer.c
    src/player/player-bonuses.c
    src/player/player-light.c
    src/player/player-lore.c
    src/player/player-skills.c
    src/player/player-upkeep.c
    src/player/player-vitals.c
    src/pane.c
    src/quest/quest-core.c
    src/quest/quest-dialogue.c
    src/quest/quest-status.c
    src/quest/valar/other-valar.c
    src/quest/valar/tulkas-varda.c
    src/randart.c
    src/rng.c
    src/fs/save.c
    src/fs/save-player.c
    src/fs/save-notes-inventory.c
    src/fs/save-dungeon.c
    src/score/score_io.c
    src/score/score_logic.c
    src/score/score_guid.c
    src/score/score_artefact.c
    src/score/score_runs.c
    src/score/score_ui.c
    src/sdl-config.c
    src/sdl-sound.c
    src/sound-config.c
    src/fs/io_sdl.c
    src/fs/path.c
    src/ui/colors.c
    src/ui/menu-click.c
    src/ui/question.c
    src/ui/story_font.c
    src/ui/status/status-frame.c
    src/ui/status/status-health.c
    src/ui/status/status-line.c
    src/ui/status/status-panel.c
    src/ui/story/pause-with-text.c
    src/ui/targeting/aim-select.c
    src/ui/targeting/direction.c
    src/ui/targeting/targeting.c
    src/log/bootstrap.c
    src/log/fatal.c
    src/player/player-song-disguise.c
    src/player/player-song-duels.c
    src/player/player-song-effects.c
    src/player/player-song-monster.c
    src/player/player-song-revealing.c
    src/player/player-songs.c
    src/spell/spell-damage.c
    src/spell/spell-detection.c
    src/spell/spell-identify.c
    src/spell/spell-monster.c
    src/spell/spell-projection.c
    src/spell/spell-projection-effects.c
    src/spell/spell-teleport.c
    src/spell/spell-terrain.c
    src/spell/spell-utility.c
    src/spell/spell-visuals.c
    src/supplies.c
    src/tables.c
    src/use-obj.c
    src/support/command.c
    src/support/editing-buffer.c
    src/support/feedback.c
    src/support/geometry.c
    src/support/input.c
    src/support/macro.c
    src/support/message.c
    src/support/movement-input.c
    src/support/misc.c
    src/support/prompt.c
    src/support/quark.c
    src/support/screen.c
    src/support/text-output.c
    src/support/utf8.c
    src/variable.c
    src/wizard1.c
    src/wizard2.c
    src/world/monster-death.c
    src/world/panel.c
    src/z-term.c
    src/support/strl.c
)

if(ANDROID)
  add_library(sil-more SHARED ${SIL_MORE_SOURCES})
elseif(SIL_IOS)
  enable_language(OBJC)
  list(APPEND SIL_MORE_SOURCES src/main-sdl-ios.m)
  add_executable(sil-more MACOSX_BUNDLE ${SIL_MORE_SOURCES})
else()
  add_executable(sil-more ${SIL_MORE_SOURCES})
endif()

if(ANDROID)
  # SDL's Android launcher expects libmain.so by convention.
  set_target_properties(sil-more PROPERTIES OUTPUT_NAME "main")
endif()

if(SIL_IOS)
  set_target_properties(sil-more PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.silqh.silmore"
    MACOSX_BUNDLE_BUNDLE_NAME "Sil-More"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SIL_VERSION_STRING}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SIL_VERSION_STRING}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
  )

  set(SIL_IOS_ASSET_CATALOG "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
  set_source_files_properties("${SIL_IOS_ASSET_CATALOG}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )
  target_sources(sil-more PRIVATE "${SIL_IOS_ASSET_CATALOG}")

  # Bundle the runtime lib/ resources into the app.
  # iOS apps are sandboxed; read-only data must live inside the .app bundle.
  file(GLOB SIL_RES_EDIT    CONFIGURE_DEPENDS "lib/edit/*.txt")
  file(GLOB SIL_RES_PREF    CONFIGURE_DEPENDS "lib/pref/*.json")
  file(GLOB SIL_RES_FONT    CONFIGURE_DEPENDS "lib/xtra/font/*.ttf")
  file(GLOB SIL_RES_GRAF    CONFIGURE_DEPENDS "lib/xtra/graf/*.png")
  file(GLOB_RECURSE SIL_RES_SOUND LIST_DIRECTORIES false CONFIGURE_DEPENDS "lib/xtra/sound/*")
  file(GLOB_RECURSE SIL_RES_MUSIC LIST_DIRECTORIES false CONFIGURE_DEPENDS "lib/xtra/music/*")
  file(GLOB SIL_RES_HELP    CONFIGURE_DEPENDS "lib/help/*.txt" "lib/help/*.hlp")

  # Preserve directory structure inside the bundle's Resources/lib/ tree.
  foreach(_file ${SIL_RES_EDIT})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/edit")
  endforeach()
  foreach(_file ${SIL_RES_PREF})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/pref")
  endforeach()
  foreach(_file ${SIL_RES_FONT})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/xtra/font")
  endforeach()
  foreach(_file ${SIL_RES_GRAF})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/xtra/graf")
  endforeach()
  foreach(_file ${SIL_RES_SOUND})
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(RELATIVE_PATH _rel_dir "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/sound" "${_dir}")
    string(REPLACE "\\" "/" _rel_dir "${_rel_dir}")
    if(_rel_dir STREQUAL ".")
      set(_package_dir "Resources/lib/xtra/sound")
    else()
      set(_package_dir "Resources/lib/xtra/sound/${_rel_dir}")
    endif()
    set_source_files_properties("${_file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_package_dir}")
  endforeach()
  foreach(_file ${SIL_RES_MUSIC})
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(RELATIVE_PATH _rel_dir "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/music" "${_dir}")
    string(REPLACE "\\" "/" _rel_dir "${_rel_dir}")
    if(_rel_dir STREQUAL ".")
      set(_package_dir "Resources/lib/xtra/music")
    else()
      set(_package_dir "Resources/lib/xtra/music/${_rel_dir}")
    endif()
    set_source_files_properties("${_file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_package_dir}")
  endforeach()
  foreach(_file ${SIL_RES_HELP})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/help")
  endforeach()

  set(SIL_ALL_RESOURCES
    ${SIL_RES_EDIT} ${SIL_RES_PREF} ${SIL_RES_FONT}
    ${SIL_RES_GRAF} ${SIL_RES_SOUND} ${SIL_RES_MUSIC} ${SIL_RES_HELP}
  )
  target_sources(sil-more PRIVATE ${SIL_ALL_RESOURCES})

  target_compile_definitions(sil-more PRIVATE SIL_IOS)
endif()

# Include Windows resource (.rc) so the executable has an icon on Windows
if(WIN32 AND NOT ANDROID)
  # Prefer the .rc in src/icons which references the Sil.ico file present there.
  target_sources(sil-more PRIVATE ${CMAKE_SOURCE_DIR}/src/icons/sil.rc)
endif()

# add_library(log STATIC src/log/log.c)
target_include_directories(log PUBLIC src/log)

# add_executable(sil-more ${SIL_MORE_SOURCES})

target_compile_options(sil-more PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)

target_include_directories(sil-more PRIVATE src)
if(MINGW)
  target_link_options(sil-more PRIVATE -mwindows)
endif()

# Link SDL3 libraries (required for this build)
target_compile_definitions(sil-more PRIVATE USE_SDL)

if(SIL_IOS)
  target_link_libraries(sil-more PRIVATE SDL3::SDL3-static SDL3_ttf::SDL3_ttf-static SDL3_image::SDL3_image-static SDL3_mixer::SDL3_mixer-static log m "-framework UIKit")
else()
  target_link_libraries(sil-more PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer log m)
endif()

if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-more PUBLIC LOG_MODE_TIMESTAMP)
endif()

if (STEAMDECK_SUPPORT)
    target_compile_definitions(sil-more PUBLIC STEAMDECK_SUPPORT)
endif()

if (SIL_USE_LOCAL_DATA)
    target_compile_definitions(sil-more PUBLIC SIL_USE_LOCAL_DATA)
endif()
