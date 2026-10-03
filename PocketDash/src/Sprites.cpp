#include "Sprites.h"

#include <SDL_image.h>

#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

namespace pd {

namespace {

using Art = std::vector<const char*>;

// Palette: one character per colour. '.' is transparent.
bool paletteColor(char c, SDL_Color& out) {
    switch (c) {
    case 'K': out = {40, 28, 60, 255}; return true;    // outline
    case 'W': out = {255, 255, 255, 255}; return true; // white / shine
    case 'R': out = {232, 67, 79, 255}; return true;   // cap red
    case 'r': out = {168, 40, 56, 255}; return true;   // cap brim
    case 'S': out = {255, 215, 168, 255}; return true; // skin
    case 'E': out = {26, 26, 42, 255}; return true;    // eyes
    case 'P': out = {255, 143, 163, 255}; return true; // cheeks
    case 'H': out = {122, 74, 42, 255}; return true;   // hair
    case 'Y': out = {255, 210, 63, 255}; return true;  // scarf / gold
    case 'y': out = {214, 140, 30, 255}; return true;  // dark gold
    case 'B': out = {59, 125, 216, 255}; return true;  // tunic
    case 'G': out = {90, 190, 90, 255}; return true;   // backpack
    case 'N': out = {107, 62, 38, 255}; return true;   // boots
    case 'g': out = {120, 110, 140, 255}; return true; // empty heart
    case 'w': out = {210, 205, 225, 255}; return true; // empty heart shine
    case 'C': out = {90, 170, 255, 255}; return true;  // slime
    case 'c': out = {50, 110, 200, 255}; return true;  // slime shade
    case 'V': out = {150, 90, 200, 255}; return true;  // beetle shell
    case 'v': out = {100, 60, 150, 255}; return true;  // beetle shell shade
    case 'M': out = {250, 235, 210, 255}; return true; // mushroom stem
    case 'A': out = {80, 220, 200, 255}; return true;  // active checkpoint flag / gem
    case 'O': out = {255, 150, 40, 255}; return true;  // orange (giant icon)
    case '.': out = {0, 0, 0, 0}; return true;
    default: return false;
    }
}

// --- Player (16x16 per frame) ----------------------------------------------
// Body rows 0-13 per facing; the last two rows (feet) vary per frame.

const Art kPlayerFront = {
    ".....KKKKKK.....",
    "....KRRRRRRK....",
    "...KRRRRWWRRK...",
    "..KRRRRRRRWRRK..",
    "..KrrrrrrrrrrK..",
    "..KHSSSSSSSSHK..",
    "..KSSESSSSESSK..",
    "..KSSESSSSESSK..",
    "..KSPSSSSSSPSK..",
    "...KSSSKKSSSK...",
    "....KYYYYYYK....",
    "...KBBBYYBBBK...",
    "..KSKBBBBBBKSK..",
    "...KKBBBBBBKK...",
};

const Art kPlayerBack = {
    ".....KKKKKK.....",
    "....KRRRRRRK....",
    "...KRRRRWWRRK...",
    "..KRRRRRRRWRRK..",
    "..KrrrrrrrrrrK..",
    "..KHHHHHHHHHHK..",
    "..KHHHHHHHHHHK..",
    "..KHHHHHHHHHHK..",
    "..KHHHHHHHHHHK..",
    "...KHHHHHHHHK...",
    "....KYYYYYYK....",
    "...KBBGGGGBBK...",
    "..KSKBGGGGBKSK..",
    "...KKBBBBBBKK...",
};

const Art kPlayerSide = {
    "......KKKKK.....",
    ".....KRRRRRK....",
    "....KRRRRWWRK...",
    "...KRRRRRRRWRK..",
    "...KrrrrrrrrrrrK",
    "...KHHHSSSSSSK..",
    "...KHHSSSSSESK..",
    "...KHHSSSSSESK..",
    "...KHSSSSSSPSK..",
    "....KSSSSSKKK...",
    ".....KYYYYYK....",
    "....KBBBBBBYK...",
    "....KBBBSSKBK...",
    "....KKBBBBBKK...",
};

const Art kFeetFrontIdle = {
    "....KNNKKNNK....",
    ".....KK..KK.....",
};
const Art kFeetFrontStep = {
    "...KNNK..KNNK...",
    "....KK....KK....",
};
const Art kFeetSideIdle = {
    ".....KNNKNNK....",
    "......KK.KK.....",
};
const Art kFeetSideStep = {
    "....KNNK.KNNK...",
    ".....KK...KK....",
};

// --- HUD hearts (9x8) --------------------------------------------------------

const Art kHeartFull = {
    ".KK...KK.",
    "KRRK.KRRK",
    "KRWRKRRRK",
    "KRRRRRRRK",
    ".KRRRRRK.",
    "..KRRRK..",
    "...KRK...",
    "....K....",
};

const Art kHeartEmpty = {
    ".KK...KK.",
    "KggK.KggK",
    "KgwgKgggK",
    "KgggggggK",
    ".KgggggK.",
    "..KgggK..",
    "...KgK...",
    "....K....",
};

// --- Coin (12x12, 3 spin frames; the 4th is frame 1 mirrored) ---------------

const Art kCoin0 = {
    "...KKKKKK...",
    "..KYYYYYYK..",
    ".KYYWWYYYyK.",
    "KYYWYYYYYYyK",
    "KYYWYYKYYYyK",
    "KYYYYYKYYYyK",
    "KYYYYYKYYYyK",
    "KYYYYYKYYYyK",
    "KYYYYYYYYyyK",
    ".KyYYYYYyyK.",
    "..KyyyyyyK..",
    "...KKKKKK...",
};
const Art kCoin1 = {
    "....KKKK....",
    "...KYYYYK...",
    "..KYWYYYyK..",
    "..KYWYYYyK..",
    "..KYYYKYyK..",
    "..KYYYKYyK..",
    "..KYYYKYyK..",
    "..KYYYKYyK..",
    "..KYYYYyyK..",
    "..KyYYYyyK..",
    "...KyyyyK...",
    "....KKKK....",
};
const Art kCoin2 = {
    ".....KK.....",
    "....KYyK....",
    "....KWyK....",
    "....KWyK....",
    "....KYyK....",
    "....KYyK....",
    "....KYyK....",
    "....KYyK....",
    "....KYyK....",
    "....KYyK....",
    "....KYyK....",
    ".....KK.....",
};

// --- Enemies (16x16) ---------------------------------------------------------

const Art kSlime0 = {
    "................",
    "................",
    "................",
    "................",
    "......KKKK......",
    "....KKCCCCKK....",
    "...KCCWWCCCCK...",
    "..KCCWCCCCCCCK..",
    "..KCCCCCCCCCCK..",
    ".KCCCKCCCCKCCCK.",
    ".KCCCKCCCCKCCCK.",
    ".KCCCCCCCCCCCCK.",
    ".KcCCCCKKCCCCcK.",
    "..KccCCCCCCccK..",
    "...KKccccccKK...",
    "................",
};
const Art kSlime1 = { // squashed: the "about to hop" telegraph
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....KKKKKK.....",
    "...KKCCWWCCKK...",
    "..KCCWCCCCCCCK..",
    ".KCCCKCCCCKCCCK.",
    ".KCCCKCCCCKCCCK.",
    "KCCCCCCCCCCCCCCK",
    "KcCCCCCKKCCCCCcK",
    "KccCCCCCCCCCCccK",
    ".KKccccccccccKK.",
    "................",
};
const Art kBeetle0 = { // seen from above, head to the right
    "................",
    "................",
    "...K..K..K......",
    "...KKKKKKKKK....",
    "..KVVVVvVVVVK...",
    ".KVVWWVvVVVVVKK.",
    ".KVVWVVvVVVVKNNK",
    ".KVVVVVvVVVVKNEK",
    ".KVVVVVvVVVVKNEK",
    ".KVVVVVvVVVVKNNK",
    ".KvVVVVvVVVVVKK.",
    "..KvvVVvVVVvK...",
    "...KKKKKKKKK....",
    "...K..K..K......",
    "................",
    "................",
};
const Art kBeetle1 = { // legs mid-step
    "................",
    "................",
    "....K..K..K.....",
    "...KKKKKKKKK....",
    "..KVVVVvVVVVK...",
    ".KVVWWVvVVVVVKK.",
    ".KVVWVVvVVVVKNNK",
    ".KVVVVVvVVVVKNEK",
    ".KVVVVVvVVVVKNEK",
    ".KVVVVVvVVVVKNNK",
    ".KvVVVVvVVVVVKK.",
    "..KvvVVvVVVvK...",
    "...KKKKKKKKK....",
    "....K..K..K.....",
    "................",
    "................",
};
const Art kMushroom = {
    "................",
    ".....KKKKKK.....",
    "...KKRRRRRRKK...",
    "..KRRWWRRRRRRK..",
    ".KRRWWWRRRWWRRK.",
    ".KRRRWRRRRWWRRK.",
    "KRRRRRRRWRRRRRRK",
    "KRWWRRRWWWRRRRRK",
    "KrrrrrrrrrrrrrrK",
    ".KKKKMMMMMMKKKK.",
    "....KMEMMEMK....",
    "....KMEMMEMK....",
    "....KMMMMMMK....",
    "....KMPMMPMK....",
    ".....KKKKKK.....",
    "................",
};

// --- Checkpoint flag (16x16); the unreached version is drawn in grey -----------

const Art kCheckpoint = {
    "................",
    "....KK..........",
    "....KWKKKKK.....",
    "....KWKAAAAK....",
    "....KWKAAAAAK...",
    "....KWKAAAAAAK..",
    "....KWKAAAAAK...",
    "....KWKAAAAK....",
    "....KWKKKKK.....",
    "....KWK.........",
    "....KWK.........",
    "....KWK.........",
    "..KKKKKKK.......",
    "..KgggggK.......",
    "..KKKKKKK.......",
    "................",
};

// --- Items (12x12): star, gem and power-up icons ------------------------------
// The star is drawn white and recoloured: gold (collectible), grey (empty
// HUD slot) or tinted at runtime (rainbow star icon).

const Art kStar = {
    ".....KK.....",
    "....KWWK....",
    "....KWWK....",
    "KKKKKWWKKKKK",
    "KWWWWWWWWWWK",
    ".KWWKWWKWWK.",
    "..KWWWWWWK..",
    "..KWWWWWWK..",
    ".KWWWKKWWWK.",
    ".KWWK..KWWK.",
    "KWWK....KWWK",
    "KKK......KKK",
};
const Art kGem = {
    "............",
    "............",
    "..KKKKKKKK..",
    ".KWWAAAAAAK.",
    "KWAAAAAAAAcK",
    "KKKKKKKKKKKK",
    ".KAAAAAAAcK.",
    "..KAAAAAAK..",
    "...KAAAAK...",
    "....KAAK....",
    ".....KK.....",
    "............",
};
const Art kIconSpeed = { // lightning bolt
    "......KKKK..",
    ".....KYYYK..",
    "....KYYYK...",
    "...KYYYK....",
    "..KYYYYKKK..",
    "..KYYYYYYK..",
    "..KKKKYYK...",
    "....KYYK....",
    "...KYYK.....",
    "...KYK......",
    "..KYK.......",
    "..KK........",
};
const Art kIconShield = { // bubble
    "....KKKK....",
    "..KKAAAAKK..",
    ".KAWWAAAAAK.",
    ".KAWAAAAAAK.",
    "KAAAAAAAAAAK",
    "KAAAAAAAAAAK",
    "KAAAAAAAAAAK",
    "KAAAAAAAAAAK",
    ".KAAAAAAAAK.",
    ".KAAAAAAAAK.",
    "..KKAAAAKK..",
    "....KKKK....",
};
const Art kIconMagnet = {
    "KKKK....KKKK",
    "KWWK....KWWK",
    "KWWK....KWWK",
    "KRRK....KRRK",
    "KRRK....KRRK",
    "KRRK....KRRK",
    "KRRRK..KRRRK",
    "KRRRRKKRRRRK",
    ".KRRRRRRRRK.",
    "..KKRRRRKK..",
    "....KKKK....",
    "............",
};
const Art kIconSuperDash = { // >>
    "............",
    "KK....KK....",
    "KVK...KVK...",
    "KVVK..KVVK..",
    ".KVVK..KVVK.",
    "..KVVK..KVVK",
    "..KVVK..KVVK",
    ".KVVK..KVVK.",
    "KVVK..KVVK..",
    "KVK...KVK...",
    "KK....KK....",
    "............",
};
const Art kIconDoubleCoins = { // coin with a 2
    "...KKKKKK...",
    "..KYYYYYYK..",
    ".KYYKKKKYyK.",
    "KYYYYYYKYYyK",
    "KYYYYYYKYYyK",
    "KYYYKKKKYYyK",
    "KYYYKYYYYYyK",
    "KYYYKYYYYYyK",
    "KYYYKKKKYyyK",
    ".KyYYYYYYyK.",
    "..KyyyyyyK..",
    "...KKKKKK...",
};
const Art kIconTiny = { // shrink arrow (Giant is this flipped, in orange)
    "....KKKK....",
    "....KGGK....",
    "....KGGK....",
    "....KGGK....",
    "....KGGK....",
    ".KKKKGGKKKK.",
    ".KGGGGGGGGK.",
    "..KGGGGGGK..",
    "...KGGGGK...",
    "....KGGK....",
    ".....KK.....",
    "............",
};

struct NamedArt {
    const char* name;
    const Art* art;
    int width;
};

const NamedArt kAllArt[] = {
    {"player_front", &kPlayerFront, Sprites::kPlayerFrameW},
    {"player_back", &kPlayerBack, Sprites::kPlayerFrameW},
    {"player_side", &kPlayerSide, Sprites::kPlayerFrameW},
    {"feet_front_idle", &kFeetFrontIdle, Sprites::kPlayerFrameW},
    {"feet_front_step", &kFeetFrontStep, Sprites::kPlayerFrameW},
    {"feet_side_idle", &kFeetSideIdle, Sprites::kPlayerFrameW},
    {"feet_side_step", &kFeetSideStep, Sprites::kPlayerFrameW},
    {"heart_full", &kHeartFull, Sprites::kHeartW},
    {"heart_empty", &kHeartEmpty, Sprites::kHeartW},
    {"coin0", &kCoin0, Sprites::kCoinSize},
    {"coin1", &kCoin1, Sprites::kCoinSize},
    {"coin2", &kCoin2, Sprites::kCoinSize},
    {"slime0", &kSlime0, Sprites::kEnemyFrame},
    {"slime1", &kSlime1, Sprites::kEnemyFrame},
    {"beetle0", &kBeetle0, Sprites::kEnemyFrame},
    {"beetle1", &kBeetle1, Sprites::kEnemyFrame},
    {"mushroom", &kMushroom, Sprites::kEnemyFrame},
    {"checkpoint", &kCheckpoint, Sprites::kEnemyFrame},
    {"star", &kStar, Sprites::kItemSize},
    {"gem", &kGem, Sprites::kItemSize},
    {"icon_speed", &kIconSpeed, Sprites::kItemSize},
    {"icon_shield", &kIconShield, Sprites::kItemSize},
    {"icon_magnet", &kIconMagnet, Sprites::kItemSize},
    {"icon_superdash", &kIconSuperDash, Sprites::kItemSize},
    {"icon_double", &kIconDoubleCoins, Sprites::kItemSize},
    {"icon_tiny", &kIconTiny, Sprites::kItemSize},
};

// `from`/`to` optionally swap one palette character (e.g. a grey variant).
void blitArt(SDL_Surface* surface, const Art& art, int ox, int oy, char from = 0, char to = 0) {
    for (size_t y = 0; y < art.size(); ++y) {
        const char* row = art[y];
        for (int x = 0; row[x] != '\0'; ++x) {
            SDL_Color c{};
            const char ch = (from && row[x] == from) ? to : row[x];
            if (!paletteColor(ch, c) || c.a == 0) continue;
            const SDL_Rect px{ox + x, oy + static_cast<int>(y), 1, 1};
            SDL_FillRect(surface, &px, SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a));
        }
    }
}

SurfacePtr makeSurface(int w, int h) {
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32));
    if (s) SDL_FillRect(s.get(), nullptr, SDL_MapRGBA(s->format, 0, 0, 0, 0));
    return s;
}

TexturePtr toTexture(SDL_Renderer* r, SDL_Surface* s) {
    if (!s) return nullptr;
    TexturePtr t(SDL_CreateTextureFromSurface(r, s));
    if (t) SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
    return t;
}

TexturePtr artTexture(SDL_Renderer* r, const Art& art, int width) {
    SurfacePtr s = makeSurface(width, static_cast<int>(art.size()));
    if (!s) return nullptr;
    blitArt(s.get(), art, 0, 0);
    return toTexture(r, s.get());
}

// Loads assets/sprites/<name>.png if it exists and is at least the expected size.
TexturePtr loadOverride(SDL_Renderer* r, const std::string& dir, const char* name, int minW, int minH) {
    const std::string path = dir + name + ".png";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) return nullptr;
    TexturePtr t(IMG_LoadTexture(r, path.c_str()));
    if (!t) {
        SDL_Log("[sprites] Failed to load %s: %s", path.c_str(), IMG_GetError());
        return nullptr;
    }
    int w = 0;
    int h = 0;
    SDL_QueryTexture(t.get(), nullptr, nullptr, &w, &h);
    if (w < minW || h < minH) {
        SDL_Log("[sprites] %s is %dx%d, expected at least %dx%d - using placeholder", path.c_str(), w, h, minW, minH);
        return nullptr;
    }
    SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
    SDL_Log("[sprites] Using %s", path.c_str());
    return t;
}

} // namespace

bool Sprites::validateBuiltinArt(std::string* error) {
    for (const auto& entry : kAllArt) {
        for (size_t y = 0; y < entry.art->size(); ++y) {
            const char* row = (*entry.art)[y];
            if (std::strlen(row) != static_cast<size_t>(entry.width)) {
                if (error)
                    *error = std::string(entry.name) + " row " + std::to_string(y) + " is " +
                             std::to_string(std::strlen(row)) + " wide, expected " + std::to_string(entry.width);
                return false;
            }
            for (const char* p = row; *p; ++p) {
                SDL_Color c{};
                if (!paletteColor(*p, c)) {
                    if (error) *error = std::string(entry.name) + " uses unknown palette char '" + *p + "'";
                    return false;
                }
            }
        }
    }
    const int bodyRows = kPlayerFrameH - static_cast<int>(kFeetFrontIdle.size());
    for (const Art* body : {&kPlayerFront, &kPlayerBack, &kPlayerSide}) {
        if (static_cast<int>(body->size()) != bodyRows) {
            if (error) *error = "player body art must have " + std::to_string(bodyRows) + " rows";
            return false;
        }
    }
    return true;
}

bool Sprites::create(SDL_Renderer* renderer, const std::string& spriteDir) {
    const int sheetW = kPlayerFrameW * kPlayerFrames;
    const int sheetH = kPlayerFrameH * kPlayerRows;

    player_ = loadOverride(renderer, spriteDir, "player", sheetW, sheetH);
    if (!player_) {
        SurfacePtr sheet = makeSurface(sheetW, sheetH);
        if (!sheet) return false;
        const int feetY = kPlayerFrameH - static_cast<int>(kFeetFrontIdle.size());
        const Art* bodies[kPlayerRows] = {&kPlayerFront, &kPlayerBack, &kPlayerSide};
        for (int row = 0; row < kPlayerRows; ++row) {
            const bool side = row == kRowSide;
            const Art* feet[kPlayerFrames] = {side ? &kFeetSideIdle : &kFeetFrontIdle,
                                              side ? &kFeetSideStep : &kFeetFrontStep};
            for (int frame = 0; frame < kPlayerFrames; ++frame) {
                const int ox = frame * kPlayerFrameW;
                const int oy = row * kPlayerFrameH;
                blitArt(sheet.get(), *bodies[row], ox, oy);
                blitArt(sheet.get(), *feet[frame], ox, oy + feetY);
            }
        }
        player_ = toTexture(renderer, sheet.get());
    }

    heartFull_ = loadOverride(renderer, spriteDir, "heart_full", kHeartW, kHeartH);
    if (!heartFull_) heartFull_ = artTexture(renderer, kHeartFull, kHeartW);
    heartEmpty_ = loadOverride(renderer, spriteDir, "heart_empty", kHeartW, kHeartH);
    if (!heartEmpty_) heartEmpty_ = artTexture(renderer, kHeartEmpty, kHeartW);

    coin_ = loadOverride(renderer, spriteDir, "coin", kCoinSize * kCoinFrames, kCoinSize);
    if (!coin_) {
        SurfacePtr sheet = makeSurface(kCoinSize * kCoinFrames, kCoinSize);
        if (!sheet) return false;
        const Art* frames[kCoinFrames] = {&kCoin0, &kCoin1, &kCoin2};
        for (int i = 0; i < kCoinFrames; ++i) blitArt(sheet.get(), *frames[i], i * kCoinSize, 0);
        coin_ = toTexture(renderer, sheet.get());
    }

    enemies_ = loadOverride(renderer, spriteDir, "enemies", kEnemyFrame * 2, kEnemyFrame * kEnemyRows);
    if (!enemies_) {
        SurfacePtr sheet = makeSurface(kEnemyFrame * 2, kEnemyFrame * kEnemyRows);
        if (!sheet) return false;
        const Art* frames[kEnemyRows][2] = {{&kSlime0, &kSlime1}, {&kBeetle0, &kBeetle1}, {&kMushroom, &kMushroom}};
        for (int row = 0; row < kEnemyRows; ++row)
            for (int f = 0; f < 2; ++f) blitArt(sheet.get(), *frames[row][f], f * kEnemyFrame, row * kEnemyFrame);
        enemies_ = toTexture(renderer, sheet.get());
    }

    checkpoint_ = loadOverride(renderer, spriteDir, "checkpoint", kEnemyFrame * 2, kEnemyFrame);
    if (!checkpoint_) {
        SurfacePtr sheet = makeSurface(kEnemyFrame * 2, kEnemyFrame);
        if (!sheet) return false;
        blitArt(sheet.get(), kCheckpoint, 0, 0, 'A', 'g'); // frame 0: not reached yet
        blitArt(sheet.get(), kCheckpoint, kEnemyFrame, 0);  // frame 1: active
        checkpoint_ = toTexture(renderer, sheet.get());
    }

    items_ = loadOverride(renderer, spriteDir, "items", kItemSize * kItemFrames, kItemSize);
    if (!items_) {
        SurfacePtr sheet = makeSurface(kItemSize * kItemFrames, kItemSize);
        if (!sheet) return false;
        auto at = [](int frame) { return frame * kItemSize; };
        blitArt(sheet.get(), kStar, at(kItemStar), 0, 'W', 'Y');
        blitArt(sheet.get(), kStar, at(kItemStarEmpty), 0, 'W', 'g');
        blitArt(sheet.get(), kGem, at(kItemGem), 0);
        blitArt(sheet.get(), kIconSpeed, at(itemFrame(PowerUpType::SpeedShoes)), 0);
        blitArt(sheet.get(), kIconShield, at(itemFrame(PowerUpType::ShieldBubble)), 0);
        blitArt(sheet.get(), kIconMagnet, at(itemFrame(PowerUpType::Magnet)), 0);
        blitArt(sheet.get(), kIconSuperDash, at(itemFrame(PowerUpType::SuperDash)), 0);
        blitArt(sheet.get(), kIconDoubleCoins, at(itemFrame(PowerUpType::DoubleCoins)), 0);
        blitArt(sheet.get(), kIconTiny, at(itemFrame(PowerUpType::TinyMode)), 0);
        // Giant: the tiny arrow upside down, in orange.
        const Art flipped(kIconTiny.rbegin(), kIconTiny.rend());
        blitArt(sheet.get(), flipped, at(itemFrame(PowerUpType::GiantMode)), 0, 'G', 'O');
        blitArt(sheet.get(), kStar, at(itemFrame(PowerUpType::RainbowStar)), 0); // white: tinted when drawn
        items_ = toTexture(renderer, sheet.get());
    }

    const bool ok = player_ && heartFull_ && heartEmpty_ && coin_ && enemies_ && checkpoint_ && items_;
    if (!ok) SDL_Log("[sprites] Failed to create sprites: %s", SDL_GetError());
    return ok;
}

} // namespace pd
