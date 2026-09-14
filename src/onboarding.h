// The first-run setup screen, also reachable later via the Setup button.
//
// It asks a short series of optional questions, one per screen. Nothing here is
// required - every step can be skipped - but each answer gives the predictions
// something to work from before any cycles have been logged.
#pragma once

#include <SDL.h>

#include <vector>

#include "profile.h"
#include "ui.h"

class Onboarding {
public:
    // `existing` pre-fills the controls, so reopening this to change one answer
    // doesn't mean re-entering all of them.
    explicit Onboarding(const Profile& existing, bool editing);

    // Called once per frame before input is handled, so that what you can click
    // is always exactly what was drawn.
    void layout(int windowW, int windowH);

    void handleClick(int x, int y);
    void handleKey(SDL_Keycode key);
    void draw(SDL_Renderer* r, TextRenderer& text, int windowW, int windowH,
              int mouseX, int mouseY);

    bool finished() const { return finished_; }
    bool cancelled() const { return cancelled_; }

    // Valid once finished() is true.
    const Profile& result() const { return profile_; }

    // If the last period start was given, the app logs those days for you so
    // there's something on the calendar immediately.
    bool shouldLogLastPeriod() const { return finished_ && lastPeriodGiven_; }
    Date lastPeriodStart() const { return addDays(today(), -daysAgo_); }
    int  lastPeriodLength() const;

private:
    enum class Step { Welcome, CycleLength, PeriodLength, Regularity, Age, LastPeriod, Summary };
    enum class Action {
        None, Begin, SkipSetup, Back, Continue, NotSure,
        Minus, Plus,
        SetRegular, SetSomewhat, SetIrregular,
        Finish,
    };

    struct Hit { SDL_Rect rect; Action action; };

    void   perform(Action action);
    void   goNext();
    void   goBack();
    void   commit();
    int    firstStepIndex() const;
    Step   step() const { return steps_[stepIndex_]; }
    int&   activeValue();           // the number the +/- buttons change
    void   clampActiveValue();
    bool   activeValueGiven() const;
    void   markActiveGiven(bool given);

    Profile profile_;
    bool    editing_   = false;
    bool    finished_  = false;
    bool    cancelled_ = false;

    // Working copies of the answers, kept separate from `profile_` until the
    // last step so backing out of setup changes nothing.
    int  cycle_  = 28;
    int  period_ = 5;
    int  age_    = 25;
    int  daysAgo_ = 14;
    Profile::Regularity regularity_ = Profile::Regularity::Unknown;
    bool cycleGiven_ = false, periodGiven_ = false, ageGiven_ = false,
         lastPeriodGiven_ = false;

    std::vector<Step> steps_;
    int stepIndex_ = 0;

    // Rebuilt every frame by layout().
    std::vector<Hit> hits_;
    SDL_Rect panel_{};
    SDL_Rect minusButton_{}, plusButton_{};
    SDL_Rect backButton_{}, continueButton_{}, notSureButton_{};
    std::vector<SDL_Rect> choiceButtons_;
};
