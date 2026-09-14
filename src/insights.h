// The symptom summary: what you record, and where in your cycle it tends to
// fall.
//
// This is read-only. None of it feeds back into the predictions - working
// backwards from symptoms to guess cycle timing is guesswork, and it has no
// business quietly moving the dates the app shows you.
#pragma once

#include <SDL.h>

#include <array>
#include <vector>

#include "symptoms.h"
#include "tracker.h"
#include "ui.h"

struct SymptomStat {
    Symptom symptom = Symptom::Cramps;
    int totalDays = 0;            // days this was recorded on

    // The cycle day of each occurrence that could be placed in a cycle. A day
    // logged before the first period, or after a long gap, can't be, so this
    // may be shorter than totalDays.
    std::vector<int> cycleDays;

    // Typical cycle day, as a median rather than a mean so one outlier can't
    // drag it. Zero when there aren't enough placed occurrences to say.
    int typicalCycleDay = 0;
};

struct Insights {
    bool hasNotes = false;
    int  daysWithNotes = 0;
    int  placedDays = 0;      // notes that fell inside a known cycle
    int  cycleLength = 28;
    int  periodLength = 5;
    int  ovulationDay = 14;   // counted from the start of the cycle
    int  fertileFrom = 9;
    int  fertileTo = 15;

    std::vector<SymptomStat> symptoms;   // recorded ones only, most frequent first
    std::array<int, 6> moods{};          // indexed by Mood
    std::array<int, 4> flows{};          // indexed by Flow
};

Insights computeInsights(const Tracker& tracker);

// The screen itself. Read-only, so the only thing to click is the way out.
struct InsightsScreen {
    SDL_Rect closeButton{};
    void layout(int windowW, int windowH);
};

void drawInsights(SDL_Renderer* r, TextRenderer& text, const Insights& insights,
                  const InsightsScreen& screen, int windowW, int windowH,
                  int mouseX, int mouseY);
