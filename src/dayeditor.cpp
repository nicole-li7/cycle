#include "dayeditor.h"

#include <algorithm>
#include <string>

namespace {

constexpr int kPanelWidth   = 460;
constexpr int kPad          = 24;   // inside the panel
constexpr int kChipHeight   = 36;
constexpr int kChipGap      = 10;
constexpr int kSectionGap   = 20;
constexpr int kLabelHeight  = 20;

const Mood kMoods[] = {Mood::Good, Mood::Calm, Mood::Irritable, Mood::Anxious, Mood::Low};
constexpr int kMoodCount = static_cast<int>(sizeof(kMoods) / sizeof(kMoods[0]));

const Flow kFlows[] = {Flow::Light, Flow::Medium, Flow::Heavy};
constexpr int kFlowCount = static_cast<int>(sizeof(kFlows) / sizeof(kFlows[0]));

} // namespace

DayEditor::DayEditor(Date day, bool isPeriod, const DayEntry& entry)
    : day_(day), period_(isPeriod), entry_(entry) {}

// --- Editing ---------------------------------------------------------------

void DayEditor::perform(int action) {
    const int symptomBase = static_cast<int>(Action::FirstSymptom);
    const int moodBase    = static_cast<int>(Action::FirstMood);

    if (action >= moodBase) {
        const Mood picked = kMoods[action - moodBase];
        // Tapping the chosen mood again clears it, so a mis-tap is undoable
        // without a separate "none" option taking up a slot.
        entry_.mood = (entry_.mood == picked) ? Mood::None : picked;
        return;
    }
    if (action >= symptomBase) {
        entry_.toggle(static_cast<Symptom>(action - symptomBase));
        return;
    }

    switch (static_cast<Action>(action)) {
    case Action::Done:
        finished_ = true;
        break;
    case Action::TogglePeriod:
        period_ = !period_;
        if (!period_) {
            // Flow only means anything on a period day.
            entry_.flow = Flow::None;
        }
        break;
    case Action::Clear:
        entry_ = DayEntry{};
        period_ = false;
        break;
    case Action::SetFlowLight:
    case Action::SetFlowMedium:
    case Action::SetFlowHeavy: {
        const Flow picked = kFlows[action - static_cast<int>(Action::SetFlowLight)];
        entry_.flow = (entry_.flow == picked) ? Flow::None : picked;
        break;
    }
    case Action::None:
    case Action::FirstSymptom:
    case Action::FirstMood:
        break;
    }
}

void DayEditor::handleClick(int x, int y) {
    for (const Hit& hit : hits_) {
        if (pointIn(hit.rect, x, y)) {
            perform(hit.action);
            return;
        }
    }
    // A click outside the panel closes it, the way a sheet does.
    if (!pointIn(panel_, x, y)) {
        finished_ = true;
    }
}

void DayEditor::handleKey(SDL_Keycode key) {
    if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        finished_ = true;
    } else if (key == SDLK_SPACE) {
        perform(static_cast<int>(Action::TogglePeriod));
    }
}

// --- Layout ----------------------------------------------------------------

void DayEditor::layout(int windowW, int windowH) {
    hits_.clear();
    flowChips_.clear();
    symptomChips_.clear();
    moodChips_.clear();

    const int panelW = std::min(kPanelWidth, windowW - 60);
    const int innerW = panelW - 2 * kPad;

    // Measured top-down, then the whole panel is centred on what it came to.
    int height = kPad;
    height += 26 + 14;                       // date heading
    const int periodY = height;
    height += 44 + kSectionGap;              // period button

    int flowY = 0;
    if (period_) {
        flowLabelY_ = height;
        height += kLabelHeight;
        flowY = height;
        height += kChipHeight + kSectionGap;
    }

    symptomLabelY_ = height;
    height += kLabelHeight;
    const int symptomY = height;
    const int symptomRows = (static_cast<int>(kSymptomCount) + 1) / 2;
    height += symptomRows * kChipHeight + (symptomRows - 1) * kChipGap + kSectionGap;

    moodLabelY_ = height;
    height += kLabelHeight;
    const int moodY = height;
    height += kChipHeight + kSectionGap;

    const int buttonsY = height;
    height += 40 + kPad;

    panel_ = SDL_Rect{(windowW - panelW) / 2, std::max(20, (windowH - height) / 2),
                      panelW, height};
    const int x = panel_.x + kPad;

    // Everything above was measured relative to the panel top; shift into place.
    const int top = panel_.y;
    flowLabelY_ += top;
    symptomLabelY_ += top;
    moodLabelY_ += top;

    periodButton_ = SDL_Rect{x, top + periodY, innerW, 44};
    hits_.push_back({periodButton_, static_cast<int>(Action::TogglePeriod)});

    if (period_) {
        const int chipW = (innerW - 2 * kChipGap) / kFlowCount;
        for (int i = 0; i < kFlowCount; ++i) {
            SDL_Rect rect{x + i * (chipW + kChipGap), top + flowY, chipW, kChipHeight};
            flowChips_.push_back(rect);
            hits_.push_back({rect, static_cast<int>(Action::SetFlowLight) + i});
        }
    }

    {
        const int chipW = (innerW - kChipGap) / 2;
        for (std::size_t i = 0; i < kSymptomCount; ++i) {
            const int row = static_cast<int>(i) / 2;
            const int col = static_cast<int>(i) % 2;
            SDL_Rect rect{x + col * (chipW + kChipGap),
                          top + symptomY + row * (kChipHeight + kChipGap),
                          chipW, kChipHeight};
            symptomChips_.push_back(rect);
            hits_.push_back({rect, static_cast<int>(Action::FirstSymptom) +
                                   static_cast<int>(i)});
        }
    }

    {
        const int chipW = (innerW - (kMoodCount - 1) * 6) / kMoodCount;
        for (int i = 0; i < kMoodCount; ++i) {
            SDL_Rect rect{x + i * (chipW + 6), top + moodY, chipW, kChipHeight};
            moodChips_.push_back(rect);
            hits_.push_back({rect, static_cast<int>(Action::FirstMood) + i});
        }
    }

    doneButton_  = SDL_Rect{panel_.x + panelW - kPad - 110, top + buttonsY, 110, 40};
    clearButton_ = SDL_Rect{x, top + buttonsY, 96, 40};
    hits_.push_back({doneButton_, static_cast<int>(Action::Done)});
    // Clear is only drawn when there is something to clear, so it must only be
    // clickable then too - an invisible hit region is a trap.
    if (period_ || !entry_.empty()) {
        hits_.push_back({clearButton_, static_cast<int>(Action::Clear)});
    }
}

// --- Drawing ---------------------------------------------------------------

void DayEditor::draw(SDL_Renderer* r, TextRenderer& text, int windowW, int windowH,
                     int mouseX, int mouseY) {
    // Dim whatever is behind, so the panel reads as being on top of the
    // calendar rather than part of it.
    SDL_SetRenderDrawColor(r, color::kText.r, color::kText.g, color::kText.b, 120);
    SDL_Rect full{0, 0, windowW, windowH};
    SDL_RenderFillRect(r, &full);

    fillRoundedRect(r, panel_, 16, color::kBackground);

    const int x = panel_.x + kPad;

    // Heading: the date, plus a quiet marker if it happens to be today.
    text.draw(formatShort(day_), x, panel_.y + kPad, 20, color::kText, Align::Left, true);
    if (day_ == today()) {
        text.draw("TODAY", panel_.x + panel_.w - kPad, panel_.y + kPad + 6, 11,
                  color::kMuted, Align::Right, true);
    }

    drawButton(r, text, periodButton_, period_ ? "Period  -  logged" : "Mark as period day",
               pointIn(periodButton_, mouseX, mouseY),
               period_ ? ButtonStyle::Primary : ButtonStyle::Plain, 15);

    if (period_) {
        text.draw("FLOW", x, flowLabelY_, 11, color::kMuted, Align::Left, true);
        for (std::size_t i = 0; i < flowChips_.size(); ++i) {
            const bool on = entry_.flow == kFlows[i];
            drawButton(r, text, flowChips_[i], flowName(kFlows[i]),
                       pointIn(flowChips_[i], mouseX, mouseY),
                       on ? ButtonStyle::Selected : ButtonStyle::Plain, 14);
        }
    }

    text.draw("SYMPTOMS", x, symptomLabelY_, 11, color::kMuted, Align::Left, true);
    for (std::size_t i = 0; i < symptomChips_.size(); ++i) {
        const Symptom s = static_cast<Symptom>(i);
        drawButton(r, text, symptomChips_[i], symptomName(s),
                   pointIn(symptomChips_[i], mouseX, mouseY),
                   entry_.has(s) ? ButtonStyle::Selected : ButtonStyle::Plain, 14);
    }

    text.draw("MOOD", x, moodLabelY_, 11, color::kMuted, Align::Left, true);
    for (std::size_t i = 0; i < moodChips_.size(); ++i) {
        const bool on = entry_.mood == kMoods[i];
        drawButton(r, text, moodChips_[i], moodName(kMoods[i]),
                   pointIn(moodChips_[i], mouseX, mouseY),
                   on ? ButtonStyle::Selected : ButtonStyle::Plain, 13);
    }

    const bool anything = period_ || !entry_.empty();
    if (anything) {
        drawButton(r, text, clearButton_, "Clear day",
                   pointIn(clearButton_, mouseX, mouseY), ButtonStyle::Plain, 14);
    }
    drawButton(r, text, doneButton_, "Done", pointIn(doneButton_, mouseX, mouseY),
               ButtonStyle::Primary, 15);
}
