#include "Sprites.h"

#include "Canvas.h"

#include <SDL_image.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <system_error>
#include <vector>

namespace pd {

namespace {

// --- Palette: natural, slightly warm colours -----------------------------------
const Col kSkin = Col::rgb(246, 210, 176);
const Col kCap = Col::rgb(206, 56, 60);
const Col kCapDark = Col::rgb(150, 36, 44);
const Col kHair = Col::rgb(112, 74, 46);
const Col kJacket = Col::rgb(58, 112, 186);
const Col kScarf = Col::rgb(242, 176, 52);
const Col kBoots = Col::rgb(96, 62, 40);
const Col kPants = Col::rgb(62, 66, 84);
const Col kEye = Col::rgb(38, 30, 42);
const Col kWhite = Col::rgb(255, 255, 255);
const Col kGold = Col::rgb(246, 196, 62);
const Col kGoldDark = Col::rgb(176, 118, 22);

using Painter = std::function<void(Canvas&)>;

// Little helpers for cute faces.
void eye(Canvas& c, float x, float y, float s) {
    c.fillEllipse(x, y, 1.7f * s, 2.4f * s, kEye);
    c.fillCircle(x + 0.5f * s, y - 0.9f * s, 0.75f * s, kWhite);
}

void cheek(Canvas& c, float x, float y, float s) { c.softEllipse(x, y, 2.6f * s, 1.6f * s, Col::rgb(240, 120, 130, 150)); }

// --- Hero -----------------------------------------------------------------------
// Designed in a 32x40 box (feet on the bottom edge) and scaled by `k`, so the
// same painter makes the in-game sprite and the large title-screen hero.
void paintHero(Canvas& c, float ox, float oy, float k, int facing, int frame) {
    auto P = [&](float x, float y) { return Vec2{ox + x * k, oy + y * k}; };
    auto sphere = [&](float x, float y, float rx, float ry, Col col, float gloss = 0.2f, float amb = 0.55f) {
        const Vec2 p = P(x, y);
        c.sphere(p.x, p.y, rx * k, ry * k, col, gloss, amb);
    };
    auto ellipse = [&](float x, float y, float rx, float ry, Col col) {
        const Vec2 p = P(x, y);
        c.fillEllipse(p.x, p.y, rx * k, ry * k, col);
    };
    auto line = [&](float x0, float y0, float x1, float y1, float w, Col col) { c.line(P(x0, y0), P(x1, y1), w * k, col); };

    const bool side = facing == 2;
    const bool back = facing == 1;
    const bool step = frame == 1;

    // Legs and boots.
    const float lx = side ? (step ? 11.0f : 14.0f) : (step ? 11.5f : 12.5f);
    const float rx = side ? (step ? 21.0f : 18.5f) : (step ? 20.5f : 19.5f);
    const float ly = step ? 36.6f : 37.0f;
    const float ry = step ? 37.4f : 37.0f;
    line(lx, 31.0f, lx, ly - 1.0f, 3.6f, kPants);
    line(rx, 31.0f, rx, ry - 1.0f, 3.6f, kPants);
    sphere(lx + (side ? 1.0f : 0.0f), ly, 3.4f, 2.4f, kBoots, 0.25f);
    sphere(rx + (side ? 1.0f : 0.0f), ry, 3.4f, 2.4f, kBoots, 0.25f);

    // Back arm, body (jacket), front arm.
    if (side) sphere(12.5f, 28.0f, 2.8f, 4.2f, kJacket.scaled(0.8f));
    sphere(16.0f, 28.5f, side ? 7.4f : 8.6f, 7.2f, kJacket, 0.15f, 0.5f);
    if (!side) {
        sphere(7.6f, 28.0f, 2.9f, 4.3f, kJacket.scaled(0.9f));
        sphere(24.4f, 28.0f, 2.9f, 4.3f, kJacket.scaled(0.9f));
        sphere(7.4f, 32.0f, 2.2f, 2.2f, kSkin, 0.1f, 0.6f);
        sphere(24.6f, 32.0f, 2.2f, 2.2f, kSkin, 0.1f, 0.6f);
        if (!back) line(16.0f, 24.5f, 16.0f, 34.0f, 0.8f, kJacket.scaled(0.6f)); // zip
    } else {
        sphere(18.0f, 28.5f, 2.8f, 4.3f, kJacket.scaled(0.95f));
        sphere(18.6f, 32.4f, 2.2f, 2.2f, kSkin, 0.1f, 0.6f);
    }
    if (back) { // backpack
        const Vec2 p = P(10.0f, 22.5f);
        c.fillRoundRect(p.x, p.y, 12.0f * k, 11.0f * k, 3.0f * k, Col::rgb(82, 132, 70));
        c.fillRoundRect(p.x + 1.0f * k, p.y + 1.0f * k, 10.0f * k, 4.0f * k, 2.0f * k, Col::rgb(104, 158, 88));
    }

    // Scarf.
    sphere(side ? 15.0f : 16.0f, 22.4f, side ? 7.0f : 8.0f, 2.6f, kScarf, 0.25f, 0.6f);
    if (!back) sphere(side ? 11.0f : 20.5f, 25.0f, 1.8f, 2.6f, kScarf.scaled(0.9f));

    // Head.
    const float hx = side ? 15.5f : 16.0f;
    if (side) {
        sphere(8.5f, 14.0f, 4.0f, 5.0f, kHair, 0.1f);
    } else if (!back) {
        sphere(6.8f, 14.0f, 3.2f, 4.4f, kHair, 0.1f);
        sphere(25.2f, 14.0f, 3.2f, 4.4f, kHair, 0.1f);
    }
    sphere(hx, 13.8f, side ? 9.6f : 10.4f, 9.8f, back ? kHair : kSkin, 0.12f, 0.6f);

    if (!back) {
        if (side) {
            const Vec2 e = P(20.5f, 15.6f);
            eye(c, e.x, e.y, k);
            sphere(25.0f, 16.6f, 1.6f, 1.4f, kSkin.scaled(0.97f), 0.1f, 0.6f);
            const Vec2 ch = P(20.0f, 19.0f);
            cheek(c, ch.x, ch.y, k);
        } else {
            const Vec2 e1 = P(12.3f, 15.6f);
            const Vec2 e2 = P(19.7f, 15.6f);
            eye(c, e1.x, e1.y, k);
            eye(c, e2.x, e2.y, k);
            const Vec2 c1 = P(9.6f, 19.0f);
            const Vec2 c2 = P(22.4f, 19.0f);
            cheek(c, c1.x, c1.y, k);
            cheek(c, c2.x, c2.y, k);
            line(14.6f, 19.6f, 16.0f, 20.3f, 0.9f, Col::rgb(150, 80, 70));
            line(16.0f, 20.3f, 17.4f, 19.6f, 0.9f, Col::rgb(150, 80, 70));
        }
    }

    // Cap: a shiny dome with a brim (pointing forward when seen from the side).
    sphere(hx, 8.6f, side ? 9.8f : 10.6f, 6.4f, kCap, 0.45f, 0.55f);
    if (side) ellipse(24.0f, 11.6f, 6.4f, 1.9f, kCapDark);
    else if (!back) ellipse(16.0f, 12.0f, 11.2f, 2.4f, kCapDark);
    else ellipse(16.0f, 11.4f, 10.4f, 1.6f, kCapDark.scaled(0.9f));
    if (!back) {
        const Vec2 badge = P(side ? 17.0f : 16.0f, 6.6f);
        c.fillCircle(badge.x, badge.y, 2.0f * k, kWhite.withAlpha(0.95f));
        c.fillCircle(badge.x, badge.y, 1.1f * k, kCap);
    }
}

// --- Hearts, coins, items ---------------------------------------------------------

std::vector<Vec2> heartShape(float cx, float cy, float s) {
    // Smooth heart outline from the classic parametric curve.
    std::vector<Vec2> pts;
    for (int i = 0; i < 48; ++i) {
        const float t = static_cast<float>(i) / 48.0f * 6.2831853f;
        const float x = 16.0f * std::pow(std::sin(t), 3.0f);
        const float y = 13.0f * std::cos(t) - 5.0f * std::cos(2 * t) - 2.0f * std::cos(3 * t) - std::cos(4 * t);
        pts.push_back({cx + x * s, cy - y * s});
    }
    return pts;
}

void paintHeart(Canvas& c, bool full) {
    const float cx = static_cast<float>(c.width()) * 0.5f;
    const float cy = static_cast<float>(c.height()) * 0.5f + 0.5f;
    const float s = static_cast<float>(c.width()) / 34.0f;
    if (full) {
        c.fillPolygon(heartShape(cx, cy, s), Col::rgb(130, 22, 36));
        c.fillPolygon(heartShape(cx, cy - 0.3f, s * 0.86f), Col::rgb(222, 52, 68));
        c.softEllipse(cx - 8.0f * s, cy - 6.0f * s, 6.0f * s, 4.0f * s, Col::rgb(255, 210, 210, 220));
        c.fillEllipse(cx - 8.5f * s, cy - 6.5f * s, 2.4f * s, 1.6f * s, kWhite.withAlpha(0.9f));
    } else {
        c.fillPolygon(heartShape(cx, cy, s), Col::rgb(30, 26, 40, 200));
        c.fillPolygon(heartShape(cx, cy - 0.3f, s * 0.86f), Col::rgb(90, 84, 104, 170));
    }
}

void paintCoin(Canvas& c, float widthScale) {
    const float cx = static_cast<float>(c.width()) * 0.5f;
    const float cy = static_cast<float>(c.height()) * 0.5f;
    const float r = static_cast<float>(c.width()) * 0.46f;
    if (widthScale < 0.2f) { // edge-on: a thin metallic bar
        c.fillRoundRect(cx - 1.8f, cy - r, 3.6f, r * 2.0f, 1.5f, kGoldDark);
        c.fillRoundRect(cx - 0.8f, cy - r + 1.0f, 1.4f, r * 2.0f - 2.0f, 0.7f, kGold.scaled(1.1f));
        return;
    }
    c.fillEllipse(cx, cy, r * widthScale, r, kGoldDark);
    c.sphere(cx, cy, (r - 1.4f) * widthScale, r - 1.4f, kGold, 0.75f, 0.6f);
    if (widthScale > 0.9f) {
        c.ring(cx, cy, r * 0.62f, r * 0.72f, Col::rgb(200, 140, 30, 140));
        // Embossed star in the middle.
        std::vector<Vec2> star;
        for (int i = 0; i < 10; ++i) {
            const float a = -1.5708f + static_cast<float>(i) * 0.6283f;
            const float rr = (i % 2 ? 0.22f : 0.48f) * r;
            star.push_back({cx + std::cos(a) * rr, cy + std::sin(a) * rr});
        }
        c.fillPolygon(star, Col::rgb(255, 232, 140, 200));
    }
}

std::vector<Vec2> starShape(float cx, float cy, float outer, float inner) {
    std::vector<Vec2> pts;
    for (int i = 0; i < 10; ++i) {
        const float a = -1.5708f + static_cast<float>(i) * 0.6283f;
        const float r = i % 2 ? inner : outer;
        pts.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
    }
    return pts;
}

// `fill` decides the look: gold (collectible), translucent grey (empty slot)
// or white (tinted at runtime for the Rainbow Star icon).
void paintStar(Canvas& c, Col fill, Col edge, bool face) {
    const float cx = static_cast<float>(c.width()) * 0.5f;
    const float cy = static_cast<float>(c.height()) * 0.54f;
    const float r = static_cast<float>(c.width()) * 0.48f;
    if (fill.a < 0.5f) {
        // Translucent fill: an empty "slot" drawn as a faint star with an outline.
        const std::vector<Vec2> pts = starShape(cx, cy, r * 0.9f, r * 0.44f);
        c.fillPolygon(pts, fill);
        for (size_t i = 0; i < pts.size(); ++i) c.line(pts[i], pts[(i + 1) % pts.size()], 1.6f, edge);
        return;
    }
    c.fillPolygon(starShape(cx, cy, r, r * 0.48f), edge);
    c.fillPolygon(starShape(cx, cy - 0.3f, r * 0.84f, r * 0.40f), fill);
    c.softEllipse(cx - r * 0.25f, cy - r * 0.35f, r * 0.35f, r * 0.22f, kWhite.withAlpha(0.55f));
    if (face) {
        c.fillEllipse(cx - r * 0.18f, cy + r * 0.02f, r * 0.07f, r * 0.11f, kEye);
        c.fillEllipse(cx + r * 0.18f, cy + r * 0.02f, r * 0.07f, r * 0.11f, kEye);
    }
}

void paintGem(Canvas& c) {
    const float w = static_cast<float>(c.width());
    const float m = w * 0.5f;
    const Col base = Col::rgb(60, 196, 214);
    // Crown facets on top, pavilion facets below, each a slightly different shade.
    c.fillPolygon({{w * 0.22f, w * 0.3f}, {w * 0.78f, w * 0.3f}, {w * 0.95f, w * 0.44f}, {m, w * 0.92f}, {w * 0.05f, w * 0.44f}},
                  base.scaled(0.55f));
    c.fillPolygon({{w * 0.24f, w * 0.33f}, {w * 0.5f, w * 0.33f}, {w * 0.36f, w * 0.44f}, {w * 0.1f, w * 0.44f}},
                  base.scaled(1.25f));
    c.fillPolygon({{w * 0.5f, w * 0.33f}, {w * 0.76f, w * 0.33f}, {w * 0.9f, w * 0.44f}, {w * 0.64f, w * 0.44f}},
                  base.scaled(1.05f));
    c.fillPolygon({{w * 0.36f, w * 0.44f}, {w * 0.5f, w * 0.33f}, {w * 0.64f, w * 0.44f}}, Col::rgb(200, 250, 255));
    c.fillPolygon({{w * 0.1f, w * 0.46f}, {w * 0.36f, w * 0.46f}, {m, w * 0.88f}}, base.scaled(0.9f));
    c.fillPolygon({{w * 0.36f, w * 0.46f}, {w * 0.64f, w * 0.46f}, {m, w * 0.88f}}, base.scaled(1.15f));
    c.fillPolygon({{w * 0.64f, w * 0.46f}, {w * 0.9f, w * 0.46f}, {m, w * 0.88f}}, base.scaled(0.75f));
    c.fillCircle(w * 0.3f, w * 0.37f, w * 0.04f, kWhite);
}

void outlined(Canvas& c, const std::vector<Vec2>& pts, Col fill, Col edge, float width) {
    c.fillPolygon(pts, fill);
    for (size_t i = 0; i < pts.size(); ++i) c.line(pts[i], pts[(i + 1) % pts.size()], width, edge);
}

void paintIcon(Canvas& c, PowerUpType t) {
    const float w = static_cast<float>(c.width());
    const Col ink = Col::rgb(40, 32, 50, 220);
    switch (t) {
    case PowerUpType::SpeedShoes: // lightning bolt
        outlined(c,
                 {{w * 0.58f, w * 0.06f}, {w * 0.2f, w * 0.56f}, {w * 0.46f, w * 0.56f}, {w * 0.36f, w * 0.94f},
                  {w * 0.8f, w * 0.4f}, {w * 0.54f, w * 0.4f}},
                 Col::rgb(255, 214, 60), ink, 1.2f);
        break;
    case PowerUpType::ShieldBubble:
        c.sphere(w * 0.5f, w * 0.5f, w * 0.42f, w * 0.42f, Col::rgb(110, 200, 240, 220), 0.9f, 0.6f);
        c.ring(w * 0.5f, w * 0.5f, w * 0.38f, w * 0.44f, Col::rgb(230, 250, 255, 180));
        break;
    case PowerUpType::Magnet: {
        // A U-shaped horseshoe magnet: two arms joined by a half-circle arc.
        const Col red = Col::rgb(212, 50, 56);
        const float cx = w * 0.5f;
        const float cy = w * 0.5f;
        const float r = w * 0.28f;
        const float thick = w * 0.2f;
        for (int i = 0; i < 16; ++i) {
            const float a0 = 3.14159f * static_cast<float>(i) / 16.0f;
            const float a1 = 3.14159f * static_cast<float>(i + 1) / 16.0f;
            c.line({cx + std::cos(a0) * r, cy + std::sin(a0) * r}, {cx + std::cos(a1) * r, cy + std::sin(a1) * r}, thick, red);
        }
        c.line({cx - r, cy}, {cx - r, w * 0.2f}, thick, red);
        c.line({cx + r, cy}, {cx + r, w * 0.2f}, thick, red);
        c.fillRoundRect(cx - r - thick * 0.5f, w * 0.06f, thick, w * 0.16f, 1.0f, Col::rgb(214, 214, 224));
        c.fillRoundRect(cx + r - thick * 0.5f, w * 0.06f, thick, w * 0.16f, 1.0f, Col::rgb(214, 214, 224));
        break;
    }
    case PowerUpType::SuperDash:
        for (float x0 : {0.12f, 0.46f}) {
            c.line({w * x0, w * 0.2f}, {w * (x0 + 0.3f), w * 0.5f}, w * 0.14f, Col::rgb(150, 100, 230));
            c.line({w * (x0 + 0.3f), w * 0.5f}, {w * x0, w * 0.8f}, w * 0.14f, Col::rgb(150, 100, 230));
        }
        break;
    case PowerUpType::DoubleCoins: {
        paintCoin(c, 1.0f);
        const Col ink2 = Col::rgb(120, 70, 10);
        c.line({w * 0.36f, w * 0.34f}, {w * 0.62f, w * 0.34f}, 2.2f, ink2);
        c.line({w * 0.62f, w * 0.34f}, {w * 0.62f, w * 0.48f}, 2.2f, ink2);
        c.line({w * 0.62f, w * 0.48f}, {w * 0.38f, w * 0.66f}, 2.2f, ink2);
        c.line({w * 0.38f, w * 0.66f}, {w * 0.64f, w * 0.66f}, 2.2f, ink2);
        break;
    }
    case PowerUpType::TinyMode:
        outlined(c,
                 {{w * 0.38f, w * 0.08f}, {w * 0.62f, w * 0.08f}, {w * 0.62f, w * 0.5f}, {w * 0.86f, w * 0.5f},
                  {w * 0.5f, w * 0.92f}, {w * 0.14f, w * 0.5f}, {w * 0.38f, w * 0.5f}},
                 Col::rgb(110, 200, 110), ink, 1.2f);
        break;
    case PowerUpType::GiantMode:
        outlined(c,
                 {{w * 0.38f, w * 0.92f}, {w * 0.62f, w * 0.92f}, {w * 0.62f, w * 0.5f}, {w * 0.86f, w * 0.5f},
                  {w * 0.5f, w * 0.08f}, {w * 0.14f, w * 0.5f}, {w * 0.38f, w * 0.5f}},
                 Col::rgb(244, 148, 52), ink, 1.2f);
        break;
    case PowerUpType::RainbowStar: paintStar(c, kWhite, Col::rgb(180, 180, 200), false); break;
    default: break;
    }
}

void paintKey(Canvas& c) {
    const float w = static_cast<float>(c.width());
    c.ring(w * 0.3f, w * 0.38f, w * 0.1f, w * 0.24f, kGoldDark);
    c.ring(w * 0.3f, w * 0.38f, w * 0.12f, w * 0.21f, kGold);
    c.line({w * 0.5f, w * 0.48f}, {w * 0.9f, w * 0.78f}, w * 0.12f, kGoldDark);
    c.line({w * 0.5f, w * 0.48f}, {w * 0.9f, w * 0.78f}, w * 0.06f, kGold);
    c.line({w * 0.74f, w * 0.66f}, {w * 0.66f, w * 0.78f}, w * 0.1f, kGoldDark);
    c.line({w * 0.86f, w * 0.75f}, {w * 0.78f, w * 0.88f}, w * 0.1f, kGoldDark);
}

// --- Characters (32x32) --------------------------------------------------------------

void paintSlime(Canvas& c, bool squashed) {
    const float rx = squashed ? 13.5f : 11.0f;
    const float ry = squashed ? 7.5f : 10.0f;
    const float cy = squashed ? 23.5f : 20.5f;
    const Col jelly = Col::rgb(82, 168, 236);
    c.sphere(16.0f, cy, rx, ry, jelly, 0.8f, 0.55f);
    c.fillEllipse(16.0f, cy + ry - 2.2f, rx * 0.9f, 2.2f, jelly.scaled(0.7f).withAlpha(0.8f)); // base
    c.softEllipse(11.5f, cy - ry * 0.45f, 3.5f, 2.2f, kWhite.withAlpha(0.8f));
    eye(c, 12.5f, cy + (squashed ? 0.0f : 0.5f), 1.0f);
    eye(c, 19.5f, cy + (squashed ? 0.0f : 0.5f), 1.0f);
    c.line({14.6f, cy + 3.6f}, {17.4f, cy + 3.6f}, 1.0f, kEye.withAlpha(0.7f));
}

void paintBeetle(Canvas& c, int frame) {
    const Col leg = Col::rgb(40, 34, 40);
    const float off = frame ? 1.5f : -1.5f;
    for (int i = 0; i < 3; ++i) {
        const float x = 9.0f + static_cast<float>(i) * 5.0f + (i % 2 ? off : -off);
        c.line({x, 12.0f}, {x - 2.0f, 4.0f}, 1.4f, leg);
        c.line({x, 20.0f}, {x - 2.0f, 28.0f}, 1.4f, leg);
    }
    c.sphere(25.0f, 16.0f, 4.6f, 4.2f, Col::rgb(52, 44, 54), 0.4f); // head
    c.line({27.0f, 13.5f}, {31.0f, 10.0f}, 1.0f, leg);
    c.line({27.0f, 18.5f}, {31.0f, 22.0f}, 1.0f, leg);
    c.fillCircle(27.0f, 14.6f, 1.0f, kWhite);
    c.fillCircle(27.0f, 17.4f, 1.0f, kWhite);
    const Col shell = Col::rgb(128, 72, 168);
    c.sphere(14.0f, 16.0f, 10.5f, 8.6f, shell, 0.9f, 0.5f);
    c.line({4.0f, 16.0f}, {23.5f, 16.0f}, 1.0f, shell.scaled(0.45f));
    c.fillCircle(10.0f, 12.5f, 1.6f, shell.scaled(0.6f));
    c.fillCircle(17.0f, 19.5f, 1.6f, shell.scaled(0.6f));
}

void paintMushroom(Canvas& c) {
    const Col stem = Col::rgb(240, 226, 200);
    c.fillRoundRect(10.0f, 15.0f, 12.0f, 15.0f, 5.0f, stem.scaled(0.8f));
    c.fillRoundRect(10.6f, 15.0f, 9.5f, 14.0f, 4.5f, stem);
    eye(c, 13.5f, 21.0f, 0.9f);
    eye(c, 18.5f, 21.0f, 0.9f);
    cheek(c, 12.0f, 24.5f, 0.8f);
    cheek(c, 20.0f, 24.5f, 0.8f);
    c.fillEllipse(16.0f, 15.0f, 12.5f, 3.2f, Col::rgb(214, 196, 170)); // gills
    c.sphere(16.0f, 12.0f, 14.0f, 9.6f, Col::rgb(210, 50, 52), 0.5f, 0.5f);
    const std::array<std::array<float, 3>, 4> spots = {
        {{9.0f, 9.0f, 2.6f}, {17.5f, 6.0f, 2.2f}, {23.0f, 11.0f, 2.4f}, {13.0f, 14.0f, 1.6f}}};
    for (const auto& s : spots) c.fillEllipse(s[0], s[1], s[2], s[2] * 0.8f, Col::rgb(250, 246, 236, 240));
}

void paintChick(Canvas& c) {
    const Col fluff = Col::rgb(250, 212, 76);
    c.line({13.0f, 26.0f}, {12.0f, 30.0f}, 1.4f, Col::rgb(230, 130, 40));
    c.line({19.0f, 26.0f}, {20.0f, 30.0f}, 1.4f, Col::rgb(230, 130, 40));
    c.sphere(16.0f, 19.5f, 9.5f, 9.0f, fluff, 0.15f, 0.6f);
    c.sphere(23.0f, 20.5f, 3.5f, 4.5f, fluff.scaled(0.92f), 0.1f, 0.6f); // wing
    for (int i = 0; i < 3; ++i)
        c.line({14.0f + static_cast<float>(i) * 2.0f, 11.0f}, {13.0f + static_cast<float>(i) * 2.5f, 7.5f}, 1.2f,
               fluff.scaled(0.9f));
    eye(c, 12.5f, 17.5f, 0.9f);
    eye(c, 19.0f, 17.5f, 0.9f);
    c.fillPolygon({{14.5f, 20.5f}, {17.5f, 20.5f}, {16.0f, 23.0f}}, Col::rgb(240, 140, 40));
    cheek(c, 11.0f, 21.0f, 0.8f);
    cheek(c, 21.0f, 21.0f, 0.8f);
}

// --- Props (32x32) -----------------------------------------------------------------

void paintFlag(Canvas& c, bool reached) {
    c.sphere(10.0f, 29.0f, 6.0f, 2.4f, Col::rgb(150, 148, 145), 0.2f);
    for (int y = 4; y < 29; ++y)
        for (int x = 9; x < 12; ++x) c.blend(x, y, Col::rgb(205, 208, 215).scaled(x == 9 ? 1.1f : x == 11 ? 0.7f : 0.95f));
    c.sphere(10.5f, 3.5f, 2.3f, 2.3f, reached ? Col::rgb(240, 190, 60) : Col::rgb(160, 160, 170), 0.7f);
    const Col cloth = reached ? Col::rgb(60, 196, 170) : Col::rgb(150, 150, 162);
    c.fillPolygon({{12, 5}, {19, 6}, {27, 8.5f}, {19, 12}, {12, 14}}, cloth);
    c.fillPolygon({{19, 6}, {27, 8.5f}, {19, 12}}, Col::rgb(0, 0, 0, 40));
}

void paintSign(Canvas& c) {
    c.softEllipse(16.0f, 29.5f, 9.0f, 2.5f, Col::rgb(10, 30, 10, 120));
    c.fillRoundRect(14.0f, 16.0f, 4.0f, 14.0f, 1.5f, Col::rgb(110, 72, 42));
    c.fillRoundRect(3.0f, 4.0f, 26.0f, 15.0f, 2.5f, Col::rgb(110, 72, 42));
    for (int y = 5; y < 18; ++y)
        for (int x = 4; x < 28; ++x) {
            const float g = tileNoise(static_cast<float>(x) * 0.25f, static_cast<float>(y) * 1.3f, 32, 7u);
            c.blend(x, y, Col::rgb(196, 146, 92).scaled(0.85f + g * 0.25f));
        }
    for (float y : {8.5f, 12.0f, 15.0f}) c.line({7.0f, y}, {y < 14.0f ? 25.0f : 18.0f, y}, 1.2f, Col::rgb(90, 58, 34, 200));
}

// --- Sheet building ---------------------------------------------------------------------

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

// Paints one cell of a sheet.
void cell(SDL_Surface* sheet, int x, int y, int w, int h, const Painter& p) {
    Canvas c(w, h);
    p(c);
    c.blitTo(sheet, x, y);
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
        SDL_Log("[sprites] %s is %dx%d, expected at least %dx%d - using built-in art", path.c_str(), w, h, minW, minH);
        return nullptr;
    }
    SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
    SDL_Log("[sprites] Using %s", path.c_str());
    return t;
}

// Every built-in painter, for the self-check.
std::vector<std::pair<const char*, Painter>> allPainters() {
    std::vector<std::pair<const char*, Painter>> v;
    for (int f = 0; f < 3; ++f)
        for (int fr = 0; fr < 2; ++fr) v.push_back({"hero", [f, fr](Canvas& c) { paintHero(c, 0, 0, 1.0f, f, fr); }});
    v.push_back({"heart", [](Canvas& c) { paintHeart(c, true); }});
    v.push_back({"heart_empty", [](Canvas& c) { paintHeart(c, false); }});
    for (float s : {1.0f, 0.55f, 0.1f}) v.push_back({"coin", [s](Canvas& c) { paintCoin(c, s); }});
    v.push_back({"star", [](Canvas& c) { paintStar(c, kGold, kGoldDark, true); }});
    v.push_back({"gem", [](Canvas& c) { paintGem(c); }});
    for (int i = 1; i < kPowerUpCount; ++i)
        v.push_back({"icon", [i](Canvas& c) { paintIcon(c, static_cast<PowerUpType>(i)); }});
    v.push_back({"key", [](Canvas& c) { paintKey(c); }});
    v.push_back({"slime", [](Canvas& c) { paintSlime(c, false); }});
    v.push_back({"beetle", [](Canvas& c) { paintBeetle(c, 0); }});
    v.push_back({"mushroom", [](Canvas& c) { paintMushroom(c); }});
    v.push_back({"chick", [](Canvas& c) { paintChick(c); }});
    v.push_back({"flag", [](Canvas& c) { paintFlag(c, true); }});
    v.push_back({"sign", [](Canvas& c) { paintSign(c); }});
    return v;
}

} // namespace

bool Sprites::validateBuiltinArt(std::string* error) {
    // Every painter must produce a visible image with valid colours.
    for (const auto& [name, painter] : allPainters()) {
        Canvas c(32, 40);
        painter(c);
        int opaque = 0;
        for (int y = 0; y < c.height(); ++y)
            for (int x = 0; x < c.width(); ++x) {
                const Col p = c.get(x, y);
                if (!(p.a >= 0.0f && p.a <= 1.0001f) || !(p.r >= 0.0f && p.r <= 1.0001f)) {
                    if (error) *error = std::string(name) + " produced an invalid colour";
                    return false;
                }
                if (p.a > 0.5f) ++opaque;
            }
        if (opaque < 20) {
            if (error) *error = std::string(name) + " is (almost) empty";
            return false;
        }
    }
    return true;
}

bool Sprites::create(SDL_Renderer* renderer, const std::string& spriteDir) {
    // Smooth art is drawn with linear filtering, so anything shown slightly
    // scaled (pulsing hearts, the stars on the results panel) stays smooth.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    const int sheetW = kPlayerFrameW * kPlayerFrames;
    const int sheetH = kPlayerFrameH * kPlayerRows;
    player_ = loadOverride(renderer, spriteDir, "player", sheetW, sheetH);
    if (!player_) {
        SurfacePtr sheet = makeSurface(sheetW, sheetH);
        if (!sheet) return false;
        const int facing[kPlayerRows] = {0, 1, 2};
        for (int row = 0; row < kPlayerRows; ++row)
            for (int f = 0; f < kPlayerFrames; ++f)
                cell(sheet.get(), f * kPlayerFrameW, row * kPlayerFrameH, kPlayerFrameW, kPlayerFrameH,
                     [&](Canvas& c) { paintHero(c, 0, 0, 1.0f, facing[row], f); });
        player_ = toTexture(renderer, sheet.get());
    }

    // Large hero for the title screen, painted at 2.5x rather than scaled up.
    {
        SurfacePtr big = makeSurface(kHeroLargeW * 2, kHeroLargeH);
        for (int f = 0; f < 2; ++f)
            cell(big.get(), f * kHeroLargeW, 0, kHeroLargeW, kHeroLargeH,
                 [&](Canvas& c) { paintHero(c, 0, 0, static_cast<float>(kHeroLargeW) / kPlayerFrameW, 0, f); });
        heroLarge_ = toTexture(renderer, big.get());
    }

    heartFull_ = loadOverride(renderer, spriteDir, "heart_full", kHeartW, kHeartH);
    if (!heartFull_) {
        SurfacePtr s = makeSurface(kHeartW, kHeartH);
        cell(s.get(), 0, 0, kHeartW, kHeartH, [](Canvas& c) { paintHeart(c, true); });
        heartFull_ = toTexture(renderer, s.get());
    }
    heartEmpty_ = loadOverride(renderer, spriteDir, "heart_empty", kHeartW, kHeartH);
    if (!heartEmpty_) {
        SurfacePtr s = makeSurface(kHeartW, kHeartH);
        cell(s.get(), 0, 0, kHeartW, kHeartH, [](Canvas& c) { paintHeart(c, false); });
        heartEmpty_ = toTexture(renderer, s.get());
    }

    coin_ = loadOverride(renderer, spriteDir, "coin", kCoinSize * kCoinFrames, kCoinSize);
    if (!coin_) {
        SurfacePtr sheet = makeSurface(kCoinSize * kCoinFrames, kCoinSize);
        const float widths[kCoinFrames] = {1.0f, 0.55f, 0.1f};
        for (int i = 0; i < kCoinFrames; ++i)
            cell(sheet.get(), i * kCoinSize, 0, kCoinSize, kCoinSize, [&](Canvas& c) { paintCoin(c, widths[i]); });
        coin_ = toTexture(renderer, sheet.get());
    }

    enemies_ = loadOverride(renderer, spriteDir, "enemies", kEnemyFrame * 2, kEnemyFrame * kEnemyRows);
    if (!enemies_) {
        SurfacePtr sheet = makeSurface(kEnemyFrame * 2, kEnemyFrame * kEnemyRows);
        const Painter painters[kEnemyRows][2] = {
            {[](Canvas& c) { paintSlime(c, false); }, [](Canvas& c) { paintSlime(c, true); }},
            {[](Canvas& c) { paintBeetle(c, 0); }, [](Canvas& c) { paintBeetle(c, 1); }},
            {[](Canvas& c) { paintMushroom(c); }, [](Canvas& c) { paintMushroom(c); }},
            {[](Canvas& c) { paintChick(c); }, [](Canvas& c) { paintChick(c); }},
        };
        for (int row = 0; row < kEnemyRows; ++row)
            for (int f = 0; f < 2; ++f)
                cell(sheet.get(), f * kEnemyFrame, row * kEnemyFrame, kEnemyFrame, kEnemyFrame, painters[row][f]);
        enemies_ = toTexture(renderer, sheet.get());
    }

    checkpoint_ = loadOverride(renderer, spriteDir, "props", kEnemyFrame * 3, kEnemyFrame);
    if (!checkpoint_) {
        SurfacePtr sheet = makeSurface(kEnemyFrame * 3, kEnemyFrame);
        cell(sheet.get(), kPropFlag * kEnemyFrame, 0, kEnemyFrame, kEnemyFrame, [](Canvas& c) { paintFlag(c, false); });
        cell(sheet.get(), kPropFlagReached * kEnemyFrame, 0, kEnemyFrame, kEnemyFrame,
             [](Canvas& c) { paintFlag(c, true); });
        cell(sheet.get(), kPropSign * kEnemyFrame, 0, kEnemyFrame, kEnemyFrame, [](Canvas& c) { paintSign(c); });
        checkpoint_ = toTexture(renderer, sheet.get());
    }

    items_ = loadOverride(renderer, spriteDir, "items", kItemSize * kItemFrames, kItemSize);
    if (!items_) {
        SurfacePtr sheet = makeSurface(kItemSize * kItemFrames, kItemSize);
        auto at = [&](int frame, const Painter& p) { cell(sheet.get(), frame * kItemSize, 0, kItemSize, kItemSize, p); };
        at(kItemStar, [](Canvas& c) { paintStar(c, kGold, kGoldDark, true); });
        at(kItemStarEmpty, [](Canvas& c) { paintStar(c, Col::rgb(255, 255, 255, 50), Col::rgb(235, 235, 245, 190), false); });
        at(kItemGem, [](Canvas& c) { paintGem(c); });
        for (int i = 1; i < kPowerUpCount; ++i)
            at(itemFrame(static_cast<PowerUpType>(i)), [i](Canvas& c) { paintIcon(c, static_cast<PowerUpType>(i)); });
        at(kItemKey, [](Canvas& c) { paintKey(c); });
        items_ = toTexture(renderer, sheet.get());
    }

    const bool ok = player_ && heroLarge_ && heartFull_ && heartEmpty_ && coin_ && enemies_ && checkpoint_ && items_;
    if (!ok) SDL_Log("[sprites] Failed to create sprites: %s", SDL_GetError());
    return ok;
}

} // namespace pd
