# External deps (optional)

Sil-More normally uses `find_package(SDL3 ...)`.

For Android (and other environments where you want a self-contained build), the top-level CMake supports:

- `-DSIL_BUILD_WITH_SDL_SOURCES=ON`

When enabled, CMake expects these repos cloned into this folder:

- `external/SDL` (SDL3)
- `external/SDL_image` (SDL_image built against SDL3)
- `external/SDL_ttf` (SDL_ttf built against SDL3)

Example:

- `git clone https://github.com/libsdl-org/SDL.git external/SDL`
- `git clone https://github.com/libsdl-org/SDL_image.git external/SDL_image`
- `git clone https://github.com/libsdl-org/SDL_ttf.git external/SDL_ttf`

Then configure Sil-More with `-DSIL_BUILD_WITH_SDL_SOURCES=ON`.
