#pragma once

#include "SdlPtr.h"

#include <SDL.h>

namespace pd {

// The painted meadow scene behind the title and every menu: gradient sky,
// glowing sun, hazy hills, a flowery meadow and drifting clouds. Painted
// once at startup (Canvas) and shared, so menus switch instantly.
class Backdrop {
public:
    bool build(SDL_Renderer* r);
    // `time` drives the cloud drift.
    void render(SDL_Renderer* r, float time) const;

private:
    TexturePtr background_;
    TexturePtr cloud_;
};

} // namespace pd
