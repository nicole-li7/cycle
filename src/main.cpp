// Period Tracker - a small offline desktop app.
//
// Everything you log stays in plain text files on this Mac; nothing is sent
// anywhere. See Tracker::dataPath() and Profile::path() for where they live.
//
// The program is the standard three parts of any GUI app:
//   1. setup     - open a window
//   2. main loop - handle input, then redraw  (repeats until you quit)
//   3. teardown  - clean up
//
// There are two screens. First-time launch shows setup; after that, the
// calendar. The Setup button in the sidebar reopens setup to change an answer.

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <optional>

#include "date.h"
#include "onboarding.h"
#include "profile.h"
#include "tracker.h"
#include "ui.h"

namespace {

constexpr int kInitialWidth  = 1040;
constexpr int kInitialHeight = 700;
constexpr int kMinWidth      = 880;
constexpr int kMinHeight     = 640;

void showFatalError(const char* what, const char* detail) {
    std::fprintf(stderr, "%s: %s\n", what, detail);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Period Tracker", detail, nullptr);
}

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    // ---- 1. Setup ---------------------------------------------------------
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        showFatalError("SDL_Init", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        showFatalError("TTF_Init", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Period Tracker",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kInitialWidth, kInitialHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        showFatalError("SDL_CreateWindow", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, kMinWidth, kMinHeight);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        showFatalError("SDL_CreateRenderer", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    // Needed for the semi-transparent "today" ring to blend with what's behind it.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // On a Retina display the drawable is larger than the window in points.
    // Scaling by that ratio keeps the layout in point coordinates while still
    // rendering crisply.
    int windowW = 0, windowH = 0, drawableW = 0, drawableH = 0;
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
    const float dpiScale = windowW > 0 ? static_cast<float>(drawableW) / windowW : 1.0f;
    SDL_RenderSetScale(renderer, dpiScale, dpiScale);

    TextRenderer text;
    if (!text.init(renderer)) {
        showFatalError("Font", text.error().c_str());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // ---- Application state ------------------------------------------------
    Tracker tracker;                 // loads the saved log and profile from disk
    YMD  viewMonth{today()};         // which month the calendar is showing
    bool running = true;
    int  mouseX = 0, mouseY = 0;

    // Setup runs automatically the very first time, and on demand after that.
    std::optional<Onboarding> onboarding;
    if (!tracker.profile().completedSetup) {
        onboarding.emplace(tracker.profile(), /*editing=*/false);
    }

    // ---- 2. Main loop -----------------------------------------------------
    while (running) {
        SDL_GetWindowSize(window, &windowW, &windowH);

        // Work out where everything is *before* handling clicks, so what you
        // can click is exactly what was drawn.
        Layout layout;
        if (onboarding) {
            onboarding->layout(windowW, windowH);
        } else {
            layout = computeLayout(viewMonth, windowW, windowH);
        }

        // --- Handle input ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_MOUSEMOTION:
                mouseX = event.motion.x;
                mouseY = event.motion.y;
                break;

            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button != SDL_BUTTON_LEFT) {
                    break;
                }
                const int x = event.button.x;
                const int y = event.button.y;

                if (onboarding) {
                    onboarding->handleClick(x, y);
                    break;
                }

                if (pointIn(layout.profileButton, x, y)) {
                    onboarding.emplace(tracker.profile(), /*editing=*/true);
                } else if (pointIn(layout.prevButton, x, y)) {
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), -1)};
                } else if (pointIn(layout.nextButton, x, y)) {
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), 1)};
                } else if (pointIn(layout.todayButton, x, y)) {
                    viewMonth = YMD{today()};
                } else if (const Layout::Cell* cell = layout.cellAt(x, y)) {
                    // Clicking a day in a neighbouring month jumps to that
                    // month rather than silently logging an off-screen day.
                    if (cell->inViewMonth) {
                        tracker.toggle(cell->date);   // also saves to disk
                    } else {
                        viewMonth = YMD{cell->date};
                    }
                }
                break;
            }

            case SDL_KEYDOWN:
                if (onboarding) {
                    onboarding->handleKey(event.key.keysym.sym);
                    break;
                }
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), -1)};
                    break;
                case SDLK_RIGHT:
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), 1)};
                    break;
                case SDLK_t:
                    viewMonth = YMD{today()};
                    break;
                case SDLK_SPACE:
                    tracker.toggle(today());
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
                default:
                    break;
                }
                break;

            default:
                break;
            }
        }

        // --- Apply the result of setup, if it just ended ---
        if (onboarding && (onboarding->finished() || onboarding->cancelled())) {
            if (onboarding->finished()) {
                tracker.setProfile(onboarding->result());
                if (onboarding->shouldLogLastPeriod()) {
                    // Put the reported period on the calendar so there's
                    // something to count from straight away. It's ordinary
                    // logged data, so it can be corrected by clicking.
                    tracker.logRange(onboarding->lastPeriodStart(),
                                     onboarding->lastPeriodLength());
                }
                viewMonth = YMD{today()};
            }
            onboarding.reset();
            continue;   // re-run the loop so the calendar lays out this frame
        }

        // --- Draw ---
        if (onboarding) {
            onboarding->draw(renderer, text, windowW, windowH, mouseX, mouseY);
        } else {
            drawApp(renderer, text, tracker, viewMonth, layout, windowW, windowH,
                    mouseX, mouseY);
        }
    }

    // ---- 3. Teardown ------------------------------------------------------
    tracker.save();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
