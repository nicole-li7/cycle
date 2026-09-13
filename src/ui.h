// Everything to do with drawing: colours, fonts, and the calendar layout.
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "date.h"
#include "tracker.h"

// ---- Palette --------------------------------------------------------------

namespace color {
constexpr SDL_Color kBackground = {22,  24,  29,  255};
constexpr SDL_Color kPanel      = {31,  34,  42,  255};
constexpr SDL_Color kCard       = {39,  43,  53,  255};
constexpr SDL_Color kHover      = {48,  53,  65,  255};
constexpr SDL_Color kText       = {236, 237, 241, 255};
constexpr SDL_Color kMuted      = {139, 146, 163, 255};
constexpr SDL_Color kDim        = {74,  80,  94,  255};
constexpr SDL_Color kPeriod     = {232, 87,  125, 255};
constexpr SDL_Color kPredicted  = {150, 66,  91,  255};
constexpr SDL_Color kFertile    = {40,  78,  86,  255};
constexpr SDL_Color kOvulation  = {78,  205, 196, 255};
constexpr SDL_Color kOnPeriod   = {255, 255, 255, 255};
} // namespace color

// ---- Drawing primitives ---------------------------------------------------

void fillRoundedRect(SDL_Renderer* r, SDL_Rect rect, int radius, SDL_Color c);
void strokeRoundedRect(SDL_Renderer* r, SDL_Rect rect, int radius, int thickness,
                       SDL_Color stroke, SDL_Color inner);

// ---- Text -----------------------------------------------------------------

enum class Align { Left, Center, Right };

// Loads one font family at the sizes we need and caches rendered strings as
// textures, so a label is only rasterised the first time it appears.
class TextRenderer {
public:
    ~TextRenderer();

    bool init(SDL_Renderer* renderer);   // false if no usable system font
    const std::string& error() const { return error_; }

    void draw(const std::string& text, int x, int y, int size, SDL_Color c,
              Align align = Align::Left, bool bold = false);
    int  width(const std::string& text, int size, bool bold = false);
    int  lineHeight(int size) const;

private:
    TTF_Font* fontFor(int size);
    SDL_Texture* textureFor(const std::string& text, int size, SDL_Color c, bool bold);

    SDL_Renderer* renderer_ = nullptr;
    std::string   fontPath_;
    std::string   error_;
    std::map<int, TTF_Font*> fonts_;

    using CacheKey = std::tuple<std::string, int, Uint32, bool>;
    std::map<CacheKey, SDL_Texture*> cache_;
};

// ---- Layout ---------------------------------------------------------------

// Positions are computed once per frame and reused for both drawing and
// click-testing, so the two can never disagree about where a day cell is.
struct Layout {
    SDL_Rect sidebar{};
    SDL_Rect prevButton{};
    SDL_Rect nextButton{};
    SDL_Rect todayButton{};

    struct Cell {
        SDL_Rect rect{};
        Date     date{};
        bool     inViewMonth = false;
    };
    std::vector<Cell> cells;

    // Returns nullptr if the point isn't over a day cell.
    const Cell* cellAt(int x, int y) const;
};

Layout computeLayout(YMD viewMonth, int windowW, int windowH);

void drawApp(SDL_Renderer* renderer, TextRenderer& text, const Tracker& tracker,
             YMD viewMonth, const Layout& layout, int windowW, int windowH,
             int mouseX, int mouseY);
