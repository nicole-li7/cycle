#include "ui.h"

#include <algorithm>
#include <cmath>

// ---- Drawing primitives ---------------------------------------------------

bool pointIn(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void fillRoundedRect(SDL_Renderer* r, SDL_Rect rect, int radius, SDL_Color c) {
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }
    radius = std::min({radius, rect.w / 2, rect.h / 2});
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    // The straight middle section is one rectangle...
    SDL_Rect middle{rect.x, rect.y + radius, rect.w, rect.h - 2 * radius};
    SDL_RenderFillRect(r, &middle);

    // ...and the rounded caps are drawn one scanline at a time, insetting each
    // line by however far the circle's edge has curved in at that height.
    for (int i = 0; i < radius; ++i) {
        const double yOffset = radius - i - 0.5;
        const int inset = radius - static_cast<int>(
            std::lround(std::sqrt(static_cast<double>(radius) * radius - yOffset * yOffset)));
        SDL_Rect top{rect.x + inset, rect.y + i, rect.w - 2 * inset, 1};
        SDL_Rect bottom{rect.x + inset, rect.y + rect.h - 1 - i, rect.w - 2 * inset, 1};
        SDL_RenderFillRect(r, &top);
        SDL_RenderFillRect(r, &bottom);
    }
}

void fillRing(SDL_Renderer* r, int centreX, int centreY, int outerRadius,
              int thickness, SDL_Color c) {
    if (outerRadius <= 0 || thickness <= 0) {
        return;
    }
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    const int innerRadius = std::max(0, outerRadius - thickness);

    // One scanline at a time. Each row of a ring is either a single span (near
    // the top and bottom, where the hole hasn't started yet) or two spans with
    // the hole between them.
    for (int y = -outerRadius; y < outerRadius; ++y) {
        const double dy = y + 0.5;
        const double outerSpan = static_cast<double>(outerRadius) * outerRadius - dy * dy;
        if (outerSpan <= 0.0) {
            continue;
        }
        const int outerX = static_cast<int>(std::lround(std::sqrt(outerSpan)));
        const double innerSpan = static_cast<double>(innerRadius) * innerRadius - dy * dy;

        if (innerSpan > 0.0) {
            const int innerX = static_cast<int>(std::lround(std::sqrt(innerSpan)));
            SDL_Rect left{centreX - outerX, centreY + y, outerX - innerX, 1};
            SDL_Rect right{centreX + innerX, centreY + y, outerX - innerX, 1};
            SDL_RenderFillRect(r, &left);
            SDL_RenderFillRect(r, &right);
        } else {
            SDL_Rect span{centreX - outerX, centreY + y, 2 * outerX, 1};
            SDL_RenderFillRect(r, &span);
        }
    }
}

void strokeRoundedRect(SDL_Renderer* r, SDL_Rect rect, int radius, int thickness,
                       SDL_Color stroke, SDL_Color inner) {
    // An outline is just a filled shape with a smaller filled shape punched
    // back over it in whatever colour sits behind.
    fillRoundedRect(r, rect, radius, stroke);
    SDL_Rect hole{rect.x + thickness, rect.y + thickness,
                  rect.w - 2 * thickness, rect.h - 2 * thickness};
    fillRoundedRect(r, hole, std::max(0, radius - thickness), inner);
}

// ---- Text -----------------------------------------------------------------

TextRenderer::~TextRenderer() {
    for (auto& [key, texture] : cache_) {
        SDL_DestroyTexture(texture);
    }
    for (auto& [size, font] : fonts_) {
        TTF_CloseFont(font);
    }
}

bool TextRenderer::init(SDL_Renderer* renderer) {
    renderer_ = renderer;

    // macOS ships different fonts depending on version, so try a few in order
    // of preference and use the first that actually opens.
    const char* candidates[] = {
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Geneva.ttf",
        "/System/Library/Fonts/Supplemental/Verdana.ttf",
    };
    for (const char* path : candidates) {
        if (TTF_Font* probe = TTF_OpenFont(path, 16)) {
            TTF_CloseFont(probe);
            fontPath_ = path;
            return true;
        }
    }
    error_ = "Could not open any system font (tried SFNS, Arial, Geneva, Verdana).";
    return false;
}

TTF_Font* TextRenderer::fontFor(int size) {
    if (auto it = fonts_.find(size); it != fonts_.end()) {
        return it->second;
    }
    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), size);
    fonts_[size] = font;
    return font;
}

SDL_Texture* TextRenderer::textureFor(const std::string& text, int size,
                                      SDL_Color c, bool bold) {
    const Uint32 packed = (Uint32(c.r) << 24) | (Uint32(c.g) << 16) |
                          (Uint32(c.b) << 8) | Uint32(c.a);
    const CacheKey key{text, size, packed, bold};
    if (auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }

    TTF_Font* font = fontFor(size);
    if (font == nullptr) {
        return nullptr;
    }
    TTF_SetFontStyle(font, bold ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
    if (surface == nullptr) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);

    cache_[key] = texture;
    return texture;
}

void TextRenderer::draw(const std::string& text, int x, int y, int size,
                        SDL_Color c, Align align, bool bold) {
    if (text.empty()) {
        return;
    }
    SDL_Texture* texture = textureFor(text, size, c, bold);
    if (texture == nullptr) {
        return;
    }
    int w = 0, h = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);

    SDL_Rect dst{x, y, w, h};
    if (align == Align::Center) {
        dst.x = x - w / 2;
    } else if (align == Align::Right) {
        dst.x = x - w;
    }
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

int TextRenderer::width(const std::string& text, int size, bool bold) {
    TTF_Font* font = fontFor(size);
    if (font == nullptr) {
        return 0;
    }
    TTF_SetFontStyle(font, bold ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

int TextRenderer::lineHeight(int size) {
    TTF_Font* font = fontFor(size);
    return font ? TTF_FontHeight(font) : static_cast<int>(size * 1.35);
}

// ---- Shared widgets -------------------------------------------------------

void drawButton(SDL_Renderer* r, TextRenderer& text, SDL_Rect rect,
                const std::string& label, bool hovered, ButtonStyle style,
                int fontSize) {
    SDL_Color fill  = color::kCard;
    SDL_Color label_ = color::kText;

    switch (style) {
    case ButtonStyle::Primary:
        fill = color::kPeriod;
        label_ = color::kOnPeriod;
        break;
    case ButtonStyle::Selected:
        // A chosen option is outlined rather than filled, so a row of them
        // still reads as a row and not as several competing buttons.
        strokeRoundedRect(r, rect, 9, 2, color::kPeriod, color::kBackground);
        label_ = color::kPeriod;
        text.draw(label, rect.x + rect.w / 2, rect.y + rect.h / 2 - fontSize * 2 / 3,
                  fontSize, label_, Align::Center, true);
        return;
    case ButtonStyle::Plain:
        break;
    }

    if (hovered) {
        // Lift the fill slightly rather than switching colour outright.
        fill = (style == ButtonStyle::Primary) ? color::kPeriodHover : color::kHover;
    }
    fillRoundedRect(r, rect, 9, fill);
    text.draw(label, rect.x + rect.w / 2, rect.y + rect.h / 2 - fontSize * 2 / 3,
              fontSize, label_, Align::Center, style == ButtonStyle::Primary);
}

// ---- Layout ---------------------------------------------------------------

namespace {

constexpr int kSidebarWidth = 320;
constexpr int kPadding      = 26;
constexpr int kHeaderHeight = 96;
constexpr int kWeekdayRow   = 30;
constexpr int kGridRows     = 6;

} // namespace

const Layout::Cell* Layout::cellAt(int x, int y) const {
    for (const Cell& cell : cells) {
        if (pointIn(cell.rect, x, y)) {
            return &cell;
        }
    }
    return nullptr;
}

Layout computeLayout(YMD viewMonth, int windowW, int windowH) {
    Layout layout;
    layout.sidebar = SDL_Rect{windowW - kSidebarWidth, 0, kSidebarWidth, windowH};

    const int calendarW = windowW - kSidebarWidth;

    // Header buttons, right-aligned within the calendar area.
    const int btn = 34;
    const int btnY = kPadding + 6;
    layout.nextButton  = SDL_Rect{calendarW - kPadding - btn, btnY, btn, btn};
    layout.prevButton  = SDL_Rect{layout.nextButton.x - btn - 8, btnY, btn, btn};
    layout.todayButton = SDL_Rect{layout.prevButton.x - 70 - 12, btnY, 70, btn};
    layout.profileButton = SDL_Rect{windowW - 26 - 56, kPadding + 2, 56, 24};

    // The grid always starts on the Sunday on or before the 1st, so the month
    // is shown in the context of whole weeks.
    const Date first = firstOfMonth(viewMonth);
    const Date gridStart = addDays(first, -weekdayIndex(first));

    const int gridTop = kHeaderHeight + kWeekdayRow;
    const int gridW   = calendarW - 2 * kPadding;
    const int gridH   = windowH - gridTop - kPadding;
    const int cellW   = gridW / 7;
    const int cellH   = gridH / kGridRows;

    layout.cells.reserve(kGridRows * 7);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < 7; ++col) {
            const Date date = addDays(gridStart, row * 7 + col);
            Layout::Cell cell;
            cell.rect = SDL_Rect{kPadding + col * cellW, gridTop + row * cellH, cellW, cellH};
            cell.date = date;
            cell.inViewMonth = sameMonth(YMD{date}, viewMonth);
            layout.cells.push_back(cell);
        }
    }
    return layout;
}

// ---- Drawing the app ------------------------------------------------------

namespace {

// Draws one day cell: its background state, then the day number on top.
void drawDayCell(SDL_Renderer* r, TextRenderer& text, const Layout::Cell& cell,
                 const Tracker& tracker, bool hovered) {
    const SDL_Rect& rect = cell.rect;
    const int size = std::min(rect.w, rect.h) - 14;
    SDL_Rect marker{rect.x + (rect.w - size) / 2, rect.y + (rect.h - size) / 2, size, size};
    const int radius = size / 2;

    const DayKind kind = tracker.kindFor(cell.date);
    const bool isToday = (cell.date == today());

    SDL_Color numberColor = cell.inViewMonth ? color::kText : color::kDim;
    bool boldNumber = false;

    // Days outside the month being viewed are shown greyed out and flat, so
    // the eye stays on the current month.
    if (cell.inViewMonth) {
        switch (kind) {
        case DayKind::Logged:
            fillRoundedRect(r, marker, radius, color::kPeriod);
            numberColor = color::kOnPeriod;
            boldNumber = true;
            break;
        case DayKind::PredictedPeriod:
            strokeRoundedRect(r, marker, radius, 2, color::kPredicted, color::kBackground);
            numberColor = color::kPeriod;
            break;
        case DayKind::Ovulation:
            strokeRoundedRect(r, marker, radius, 2, color::kOvulation, color::kBackground);
            numberColor = color::kOvulation;
            boldNumber = true;
            break;
        case DayKind::Fertile:
            fillRoundedRect(r, marker, radius, color::kFertile);
            numberColor = color::kText;
            break;
        case DayKind::Normal:
            if (hovered) {
                fillRoundedRect(r, marker, radius, color::kHover);
            }
            break;
        }
    }

    // Today gets a ring around the whole cell, drawn last so it sits on top of
    // whatever state the day is in.
    if (isToday) {
        // A circle to match every other marker in the calendar, and a ring
        // rather than an outlined shape so a logged or predicted marker inside
        // it still shows through the middle.
        constexpr int kTodayGap       = 4;   // breathing room around the marker
        constexpr int kTodayThickness = 2;
        fillRing(r, marker.x + marker.w / 2, marker.y + marker.h / 2,
                 marker.w / 2 + kTodayGap, kTodayThickness, color::kMuted);
        boldNumber = true;
    }

    const unsigned dayNumber = unsigned{YMD{cell.date}.day()};
    text.draw(std::to_string(dayNumber), rect.x + rect.w / 2,
              rect.y + rect.h / 2 - 11, 17, numberColor, Align::Center, boldNumber);
}

void drawHeader(SDL_Renderer* r, TextRenderer& text, YMD viewMonth,
                const Layout& layout, int mouseX, int mouseY) {
    text.draw(formatMonthYear(viewMonth), kPadding, kPadding + 6, 27,
              color::kText, Align::Left, true);

    struct Button { SDL_Rect rect; const char* label; };
    const Button buttons[] = {
        {layout.todayButton, "Today"},
        {layout.prevButton,  "<"},
        {layout.nextButton,  ">"},
    };
    for (const Button& b : buttons) {
        drawButton(r, text, b.rect, b.label, pointIn(b.rect, mouseX, mouseY));
    }

    // Weekday initials above the grid.
    const int calendarW = layout.sidebar.x;
    const int cellW = (calendarW - 2 * kPadding) / 7;
    for (int col = 0; col < 7; ++col) {
        const std::string label(1, weekdayAbbrev(col)[0]);
        text.draw(label, kPadding + col * cellW + cellW / 2, kHeaderHeight + 6,
                  12, color::kMuted, Align::Center, true);
    }
}

// One labelled row in the sidebar: small grey caption, the value underneath,
// and optionally a quieter note under that.
//
// Every line is placed below the measured height of the line above it, so a
// larger value or a different font can't make them collide. Returns the y to
// start the next stat at.
constexpr int kCaptionSize = 12;
constexpr int kNoteSize    = 13;
constexpr int kStatGap     = 16;

int drawStat(TextRenderer& text, int x, int y, const std::string& caption,
             const std::string& value, SDL_Color valueColor, int valueSize = 19,
             const std::string& note = "") {
    text.draw(caption, x, y, kCaptionSize, color::kMuted);
    y += text.lineHeight(kCaptionSize) + 4;

    text.draw(value, x, y, valueSize, valueColor, Align::Left, true);
    y += text.lineHeight(valueSize);

    if (!note.empty()) {
        y += 6;
        text.draw(note, x, y, kNoteSize, color::kMuted);
        y += text.lineHeight(kNoteSize);
    }
    return y + kStatGap;
}

void drawLegendRow(SDL_Renderer* r, TextRenderer& text, int x, int y,
                   SDL_Color swatch, bool outlined, const std::string& label) {
    SDL_Rect dot{x, y + 2, 13, 13};
    if (outlined) {
        strokeRoundedRect(r, dot, 6, 2, swatch, color::kPanel);
    } else {
        // Filled swatches get a thin border. A pale fill on a pale panel is
        // almost invisible at this size otherwise - the baby blue used for the
        // fertile window is barely distinguishable from the pink sidebar
        // without it. On the calendar the same fill is readable because the
        // shape is far bigger.
        strokeRoundedRect(r, dot, 6, 1, color::kMuted, swatch);
    }
    text.draw(label, x + 22, y, 13, color::kMuted);
}

void drawSidebar(SDL_Renderer* r, TextRenderer& text, const Tracker& tracker,
                 const Layout& layout, int mouseX, int mouseY) {
    const SDL_Rect& panel = layout.sidebar;
    SDL_SetRenderDrawColor(r, color::kPanel.r, color::kPanel.g, color::kPanel.b, 255);
    SDL_RenderFillRect(r, &panel);

    const int x = panel.x + 26;
    int y = kPadding + 4;

    text.draw("Period Tracker", x, y, 20, color::kText, Align::Left, true);
    drawButton(r, text, layout.profileButton, "Setup",
               pointIn(layout.profileButton, mouseX, mouseY), ButtonStyle::Plain, 12);
    y += 40;

    const Prediction& p = tracker.prediction();

    if (!p.valid) {
        text.draw("No period logged yet.", x, y, 15, color::kMuted);
        y += 26;
        text.draw("Click the days of your last", x, y, 13, color::kDim);
        text.draw("period to get started.", x, y + 19, 13, color::kDim);
        y += 52;

        // Setup answers are still worth showing back, so the screen isn't bare.
        if (tracker.profile().typicalCycle > 0) {
            drawStat(text, x, y, "YOU TOLD US",
                     std::to_string(tracker.profile().typicalCycle) + " day cycle",
                     color::kMuted, 17);
        }
        return;
    }

    // Headline: either "you're on day N" or a countdown to the next period.
    if (const int periodDay = tracker.currentPeriodDay(); periodDay > 0) {
        y = drawStat(text, x, y, "RIGHT NOW",
                     "Period day " + std::to_string(periodDay), color::kPeriod, 22);
    } else {
        const int daysAway = daysBetween(today(), p.nextStart);
        std::string headline;
        if (daysAway == 0) {
            headline = "Expected today";
        } else if (daysAway == 1) {
            headline = "In 1 day";
        } else {
            headline = "In " + std::to_string(daysAway) + " days";
        }
        // The date, and how far out it could reasonably be, sit under the
        // headline as part of the same stat.
        const std::string when = formatShort(p.nextStart) + "   \u00b1 " +
                                 std::to_string(p.spreadDays) + " days";
        y = drawStat(text, x, y, "NEXT PERIOD", headline, color::kPeriod, 22, when);
    }

    if (const int cycleDay = tracker.currentCycleDay(); cycleDay > 0) {
        y = drawStat(text, x, y, "CYCLE DAY", std::to_string(cycleDay), color::kText);
    }

    y = drawStat(text, x, y, "AVERAGE CYCLE",
                 std::to_string(p.avgCycleDays) + " days", color::kText);
    y = drawStat(text, x, y, "AVERAGE PERIOD",
                 std::to_string(p.avgPeriodDays) + " days", color::kText);

    const std::vector<Cycle>& cycles = tracker.cycles();
    if (!cycles.empty()) {
        y = drawStat(text, x, y, "LAST PERIOD BEGAN",
                     formatShort(cycles.back().start), color::kText, 17);
    }

    // How much the figures above actually rest on. This is the honest version
    // of "accuracy improves as you log more".
    const int observed = p.observedCycles;
    std::string basis;
    if (observed == 0) {
        basis = "Based on your setup answers";
    } else if (observed == 1) {
        basis = "Based on 1 logged cycle";
    } else {
        basis = "Based on " + std::to_string(observed) + " logged cycles";
    }
    text.draw(basis, x, y, kCaptionSize, color::kMuted);
    y += text.lineHeight(kCaptionSize) + 5;
    if (observed < 3) {
        text.draw("Keep logging to sharpen it.", x, y, kCaptionSize, color::kDim);
        y += text.lineHeight(kCaptionSize);
    }

    // Legend, pinned to the bottom of the panel - but never above the content,
    // so a taller sidebar can't overlap it either.
    const int legendRowGap = 22;
    int legendY = panel.h - 26 - 4 * legendRowGap - 24;
    legendY = std::max(legendY, y + 28);
    text.draw("LEGEND", x, legendY - 24, 11, color::kDim);
    drawLegendRow(r, text, x, legendY, color::kPeriod, false, "Logged period");
    drawLegendRow(r, text, x, legendY + legendRowGap, color::kPredicted, true,
                  "Predicted period");
    drawLegendRow(r, text, x, legendY + 2 * legendRowGap, color::kFertile, false,
                  "Fertile window");
    drawLegendRow(r, text, x, legendY + 3 * legendRowGap, color::kOvulation, true,
                  "Peak day");
}

} // namespace

void drawApp(SDL_Renderer* renderer, TextRenderer& text, const Tracker& tracker,
             YMD viewMonth, const Layout& layout, int windowW, int windowH,
             int mouseX, int mouseY) {
    (void)windowW;
    (void)windowH;

    SDL_SetRenderDrawColor(renderer, color::kBackground.r, color::kBackground.g,
                           color::kBackground.b, 255);
    SDL_RenderClear(renderer);

    drawHeader(renderer, text, viewMonth, layout, mouseX, mouseY);

    for (const Layout::Cell& cell : layout.cells) {
        const bool hovered = cell.inViewMonth && pointIn(cell.rect, mouseX, mouseY);
        drawDayCell(renderer, text, cell, tracker, hovered);
    }

    drawSidebar(renderer, text, tracker, layout, mouseX, mouseY);

    SDL_RenderPresent(renderer);
}
