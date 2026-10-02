#pragma once

// RAII wrappers for SDL handles. Every SDL resource in the game is owned by
// one of these smart pointers so nothing has to be freed by hand.

#include <SDL.h>
#include <SDL_mixer.h>

#include <memory>

namespace pd {

struct SdlDeleter {
    void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
    void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
    void operator()(SDL_Texture* p) const { SDL_DestroyTexture(p); }
    void operator()(SDL_Surface* p) const { SDL_FreeSurface(p); }
    void operator()(SDL_Joystick* p) const { SDL_JoystickClose(p); }
    void operator()(Mix_Chunk* p) const { Mix_FreeChunk(p); }
    void operator()(Mix_Music* p) const { Mix_FreeMusic(p); }
};

using WindowPtr = std::unique_ptr<SDL_Window, SdlDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, SdlDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlDeleter>;
using JoystickPtr = std::unique_ptr<SDL_Joystick, SdlDeleter>;
using ChunkPtr = std::unique_ptr<Mix_Chunk, SdlDeleter>;
using MusicPtr = std::unique_ptr<Mix_Music, SdlDeleter>;

} // namespace pd
