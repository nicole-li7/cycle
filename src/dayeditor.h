// The panel that opens when you click a day: period on/off, flow, symptoms and
// mood, all for that one date.
//
// It edits a working copy and hands it back when closed, so the calendar behind
// it is only updated once, in one place (see main.cpp).
#pragma once

#include <SDL.h>

#include <vector>

#include "symptoms.h"
#include "ui.h"

class DayEditor {
public:
    DayEditor(Date day, bool isPeriod, const DayEntry& entry);

    // Called once per frame before input is handled, so that what you can click
    // is always exactly what was drawn.
    void layout(int windowW, int windowH);

    void handleClick(int x, int y);
    void handleKey(SDL_Keycode key);
    void draw(SDL_Renderer* r, TextRenderer& text, int windowW, int windowH,
              int mouseX, int mouseY);

    bool finished() const { return finished_; }

    Date            day() const { return day_; }
    bool            periodOn() const { return period_; }
    const DayEntry& entry() const { return entry_; }

private:
    enum class Action {
        None, Done, TogglePeriod, Clear,
        SetFlowLight, SetFlowMedium, SetFlowHeavy,
        FirstSymptom,                 // + the symptom's index
        FirstMood = FirstSymptom + 64 // + the mood's index
    };

    struct Hit { SDL_Rect rect; int action; };

    void perform(int action);

    Date     day_;
    bool     period_ = false;
    DayEntry entry_;
    bool     finished_ = false;

    // Rebuilt every frame by layout().
    std::vector<Hit> hits_;
    SDL_Rect panel_{};
    SDL_Rect periodButton_{}, doneButton_{}, clearButton_{};
    std::vector<SDL_Rect> flowChips_;
    std::vector<SDL_Rect> symptomChips_;
    std::vector<SDL_Rect> moodChips_;
    int flowLabelY_ = 0, symptomLabelY_ = 0, moodLabelY_ = 0;
};
