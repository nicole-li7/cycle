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
// There are three screens. First-time launch shows setup; after that, the
// calendar. The Setup button in the sidebar reopens setup to change an answer,
// and clicking a day opens that day's editor over the calendar.

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <optional>

#include "date.h"
#include "dayeditor.h"
#include "insights.h"
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

    // Open while a day is being edited. It draws over the calendar, so the
    // calendar is still laid out and drawn underneath it.
    std::optional<DayEditor> dayEditor;

    // The symptom summary replaces the calendar rather than sitting over it.
    // Recomputed when opened, not every frame - nothing changes while it's up.
    bool           showSummary = false;
    Insights       insights;
    InsightsScreen summaryScreen;

    // ---- 2. Main loop -----------------------------------------------------
    while (running) {
        SDL_GetWindowSize(window, &windowW, &windowH);

        // Work out where everything is *before* handling clicks, so what you
        // can click is exactly what was drawn.
        Layout layout;
        if (onboarding) {
            onboarding->layout(windowW, windowH);
        } else if (showSummary) {
            summaryScreen.layout(windowW, windowH);
        } else {
            layout = computeLayout(viewMonth, windowW, windowH);
            if (dayEditor) {
                dayEditor->layout(windowW, windowH);
            }
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
                if (dayEditor) {
                    dayEditor->handleClick(x, y);
                    break;
                }
                if (showSummary) {
                    if (pointIn(summaryScreen.closeButton, x, y)) {
                        showSummary = false;
                    }
                    break;
                }

                if (pointIn(layout.summaryButton, x, y)) {
                    insights = computeInsights(tracker);
                    showSummary = true;
                } else if (pointIn(layout.profileButton, x, y)) {
                    onboarding.emplace(tracker.profile(), /*editing=*/true);
                } else if (pointIn(layout.prevButton, x, y)) {
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), -1)};
                } else if (pointIn(layout.nextButton, x, y)) {
                    viewMonth = YMD{addMonths(firstOfMonth(viewMonth), 1)};
                } else if (pointIn(layout.todayButton, x, y)) {
                    viewMonth = YMD{today()};
                } else if (const Layout::Cell* cell = layout.cellAt(x, y)) {
                    // Clicking a day in a neighbouring month jumps to that
                    // month rather than opening an editor for an off-screen day.
                    if (cell->inViewMonth) {
                        dayEditor.emplace(cell->date, tracker.isLogged(cell->date),
                                          tracker.entryFor(cell->date));
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
                if (dayEditor) {
                    dayEditor->handleKey(event.key.keysym.sym);
                    break;
                }
                if (showSummary) {
                    // Escape backs out of the summary rather than quitting.
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        showSummary = false;
                    }
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

        // --- Apply the day editor's changes, if it just closed ---
        if (dayEditor && dayEditor->finished()) {
            const Date edited = dayEditor->day();
            // toggle() is the only way to change a period day, so call it only
            // when the editor actually flipped it.
            if (dayEditor->periodOn() != tracker.isLogged(edited)) {
                tracker.toggle(edited);
            }
            tracker.setEntry(edited, dayEditor->entry());
            dayEditor.reset();
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
        // Each screen only draws; the finished frame is presented once here.
        if (onboarding) {
            onboarding->draw(renderer, text, windowW, windowH, mouseX, mouseY);
        } else if (showSummary) {
            drawInsights(renderer, text, insights, summaryScreen, windowW, windowH,
                         mouseX, mouseY);
        } else {
            drawApp(renderer, text, tracker, viewMonth, layout, windowW, windowH,
                    mouseX, mouseY);
            if (dayEditor) {
                dayEditor->draw(renderer, text, windowW, windowH, mouseX, mouseY);
            }
        }
        SDL_RenderPresent(renderer);
    }

    // ---- 3. Teardown ------------------------------------------------------
    tracker.save();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
