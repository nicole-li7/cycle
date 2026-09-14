// Everything to do with drawing: colours, fonts, shared widgets, and the
// calendar layout.
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
// A soft light theme: cream paper, pastel pink sidebar, rose and baby blue.
//
// Every colour in the app comes from here, so this block is the only place to
// edit to re-theme it. The values are tuned so that every text colour clears
// WCAG AA contrast (4.5:1) against the surface it sits on, and every outline
// clears 3:1 - worth re-checking if you change them, since pastel tones on a
// light background fail that easily.

constexpr SDL_Color kBackground  = {250, 245, 239, 255};  // warm cream paper
constexpr SDL_Color kPanel       = {249, 226, 231, 255};  // pastel pink sidebar
constexpr SDL_Color kCard        = {216, 182, 189, 255};  // buttons, cards
constexpr SDL_Color kHover       = {202, 164, 173, 255};

constexpr SDL_Color kText        = { 58,  44,  40, 255};  // deep warm brown
constexpr SDL_Color kMuted       = {120, 100,  89, 255};
constexpr SDL_Color kDim         = {135, 118, 109, 255};  // quiet, still readable

constexpr SDL_Color kPeriod      = {180,  60, 100, 255};  // deep rose
constexpr SDL_Color kPeriodHover = {192,  70, 110, 255};
constexpr SDL_Color kPredicted   = {190, 122, 144, 255};  // soft pink outline

// The fertile window has to be tellable apart from the pinks at a glance, so it
// sits on the cool side: a baby blue fill with a deeper blue ring for the peak.
constexpr SDL_Color kFertile     = {208, 230, 245, 255};
constexpr SDL_Color kOvulation   = { 76, 117, 148, 255};

constexpr SDL_Color kOnPeriod    = {255, 252, 250, 255};  // text on deep rose
} // namespace color

// ---- Drawing primitives ---------------------------------------------------

bool pointIn(const SDL_Rect& r, int x, int y);

void fillRoundedRect(SDL_Renderer* r, SDL_Rect rect, int radius, SDL_Color c);

// Draws only the band of a circle, leaving the middle untouched. Unlike
// strokeRoundedRect this paints no interior, so it can be drawn over something
// that should still show through the hole.
void fillRing(SDL_Renderer* r, int centreX, int centreY, int outerRadius,
              int thickness, SDL_Color c);
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
    int  lineHeight(int size);   // actual rendered height of a line, in pixels

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

// ---- Shared widgets -------------------------------------------------------

enum class ButtonStyle {
    Plain,      // sits on the background, quiet
    Primary,    // the main action on a screen
    Selected,   // a chosen option in a group
};

void drawButton(SDL_Renderer* r, TextRenderer& text, SDL_Rect rect,
                const std::string& label, bool hovered,
                ButtonStyle style = ButtonStyle::Plain, int fontSize = 14);

// ---- Calendar layout ------------------------------------------------------

// Positions are computed once per frame and reused for both drawing and
// click-testing, so the two can never disagree about where a day cell is.
struct Layout {
    SDL_Rect sidebar{};
    SDL_Rect prevButton{};
    SDL_Rect nextButton{};
    SDL_Rect todayButton{};
    SDL_Rect profileButton{};

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
