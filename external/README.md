# External deps

The pinned SDL source trees under `external/` are the source of truth for Windows and Android builds.
Desktop package-based `find_package(SDL3 ...)` builds still exist as an optional local-development path.

For Android and other self-contained builds, the repo tracks these directories as pinned Git submodules:

- `external/SDL` (SDL3)
- `external/SDL_image` (SDL_image built against SDL3)
- `external/SDL_ttf` (SDL_ttf built against SDL3)
- `external/SDL_mixer` (SDL_mixer built against SDL3)

Initialize them from the repo root with:

- `git submodule update --init --recursive`

That restores the exact SDL source trees pinned by this repo, including the nested submodules used by `SDL_image`, `SDL_ttf`, and `SDL_mixer`.

Keep `external/` read-only during normal repo work. To update SDL sources, move the submodule commits intentionally rather than editing or re-cloning those directories by hand.

For the canonical Windows/Android path, configure Sil-More with `-DSIL_BUILD_WITH_SDL_SOURCES=ON`.
