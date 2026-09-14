#include "onboarding.h"

#include <algorithm>
#include <string>

namespace {

// Ranges the +/- buttons are allowed to move within.
constexpr int kMinCycle = 20, kMaxCycle = 45;
constexpr int kMinPeriod = 1, kMaxPeriod = 12;
constexpr int kMinAge = 9,   kMaxAge = 65;
constexpr int kMinDaysAgo = 0, kMaxDaysAgo = 90;

constexpr int kPanelWidth = 520;

struct Question {
    const char* title;
    const char* help;
    const char* unit;       // shown after the number on stepper screens
};

Question questionFor(int stepOrdinal) {
    switch (stepOrdinal) {
    case 1: return {"How long is your cycle, usually?",
                    "Counted from the first day of one period to the first day of "
                    "the next. If you're not sure, skip it.", "days"};
    case 2: return {"How many days does your period last?",
                    "A rough average is fine.", "days"};
    case 3: return {"How regular are your cycles?",
                    "This sets how much your answers above are trusted once real "
                    "cycles start being logged.", ""};
    case 4: return {"How old are you?",
                    "Only used to widen the margin of error, since cycles tend to "
                    "vary more in your teens and after about 45.", "years old"};
    case 5: return {"When did your last period start?",
                    "This gives the app something to count from right away.", ""};
    default: return {"", "", ""};
    }
}

} // namespace

Onboarding::Onboarding(const Profile& existing, bool editing)
    : profile_(existing), editing_(editing) {
    // Pre-fill from whatever is already known.
    if (existing.typicalCycle > 0)  { cycle_  = existing.typicalCycle;  cycleGiven_  = true; }
    if (existing.typicalPeriod > 0) { period_ = existing.typicalPeriod; periodGiven_ = true; }
    if (existing.hasAge())          { age_    = existing.ageNow();      ageGiven_    = true; }
    regularity_ = existing.regularity;

    steps_ = {Step::Welcome, Step::CycleLength, Step::PeriodLength,
              Step::Regularity, Step::Age, Step::LastPeriod, Step::Summary};

    // Coming back to change an answer shouldn't replay the welcome screen, and
    // the last period is already logged on the calendar by then.
    if (editing_) {
        steps_ = {Step::CycleLength, Step::PeriodLength, Step::Regularity,
                  Step::Age, Step::Summary};
    }
    stepIndex_ = 0;
}

int Onboarding::firstStepIndex() const { return 0; }

int Onboarding::lastPeriodLength() const {
    return periodGiven_ ? period_ : 5;
}

// --- Which value the +/- buttons act on ------------------------------------

int& Onboarding::activeValue() {
    switch (step()) {
    case Step::CycleLength:  return cycle_;
    case Step::PeriodLength: return period_;
    case Step::Age:          return age_;
    case Step::LastPeriod:   return daysAgo_;
    default:                 return cycle_;   // unused on other steps
    }
}

void Onboarding::clampActiveValue() {
    switch (step()) {
    case Step::CycleLength:  cycle_    = std::clamp(cycle_, kMinCycle, kMaxCycle); break;
    case Step::PeriodLength: period_   = std::clamp(period_, kMinPeriod, kMaxPeriod); break;
    case Step::Age:          age_      = std::clamp(age_, kMinAge, kMaxAge); break;
    case Step::LastPeriod:   daysAgo_  = std::clamp(daysAgo_, kMinDaysAgo, kMaxDaysAgo); break;
    default: break;
    }
}

bool Onboarding::activeValueGiven() const {
    switch (step()) {
    case Step::CycleLength:  return cycleGiven_;
    case Step::PeriodLength: return periodGiven_;
    case Step::Age:          return ageGiven_;
    case Step::LastPeriod:   return lastPeriodGiven_;
    default: return false;
    }
}

void Onboarding::markActiveGiven(bool given) {
    switch (step()) {
    case Step::CycleLength:  cycleGiven_      = given; break;
    case Step::PeriodLength: periodGiven_     = given; break;
    case Step::Age:          ageGiven_        = given; break;
    case Step::LastPeriod:   lastPeriodGiven_ = given; break;
    default: break;
    }
}

// --- Navigation ------------------------------------------------------------

void Onboarding::goNext() {
    if (stepIndex_ + 1 < static_cast<int>(steps_.size())) {
        ++stepIndex_;
    }
}

void Onboarding::goBack() {
    if (stepIndex_ > firstStepIndex()) {
        --stepIndex_;
    } else if (editing_) {
        cancelled_ = true;   // backing out of an edit leaves everything as it was
    }
}

void Onboarding::commit() {
    profile_.completedSetup = true;
    profile_.typicalCycle   = cycleGiven_  ? cycle_  : 0;
    profile_.typicalPeriod  = periodGiven_ ? period_ : 0;
    profile_.regularity     = regularity_;
    profile_.birthYear      = ageGiven_ ? (int{YMD{today()}.year()} - age_) : 0;
    finished_ = true;
}

void Onboarding::perform(Action action) {
    switch (action) {
    case Action::None:
        break;
    case Action::Begin:
        goNext();
        break;
    case Action::SkipSetup:
        // "Skip setup" still counts as completing it, so the screen doesn't
        // reappear on every launch. Everything simply stays unanswered.
        profile_.completedSetup = true;
        finished_ = true;
        break;
    case Action::Back:
        goBack();
        break;
    case Action::Continue:
        markActiveGiven(true);
        goNext();
        break;
    case Action::NotSure:
        markActiveGiven(false);
        if (step() == Step::Regularity) {
            regularity_ = Profile::Regularity::Unknown;
        }
        goNext();
        break;
    case Action::Minus:
        activeValue() -= 1;
        clampActiveValue();
        markActiveGiven(true);
        break;
    case Action::Plus:
        activeValue() += 1;
        clampActiveValue();
        markActiveGiven(true);
        break;
    case Action::SetRegular:
        regularity_ = Profile::Regularity::Regular;
        goNext();
        break;
    case Action::SetSomewhat:
        regularity_ = Profile::Regularity::Somewhat;
        goNext();
        break;
    case Action::SetIrregular:
        regularity_ = Profile::Regularity::Irregular;
        goNext();
        break;
    case Action::Finish:
        commit();
        break;
    }
}

void Onboarding::handleClick(int x, int y) {
    for (const Hit& hit : hits_) {
        if (pointIn(hit.rect, x, y)) {
            perform(hit.action);
            return;
        }
    }
}

void Onboarding::handleKey(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if (step() == Step::Summary) {
            perform(Action::Finish);
        } else if (step() == Step::Welcome) {
            perform(Action::Begin);
        } else if (step() != Step::Regularity) {
            perform(Action::Continue);
        }
        break;
    case SDLK_LEFT:  perform(Action::Minus); break;
    case SDLK_RIGHT: perform(Action::Plus);  break;
    case SDLK_ESCAPE: goBack(); break;
    default: break;
    }
}

// --- Layout ----------------------------------------------------------------

void Onboarding::layout(int windowW, int windowH) {
    hits_.clear();
    choiceButtons_.clear();

    const int panelW = std::min(kPanelWidth, windowW - 80);
    panel_ = SDL_Rect{(windowW - panelW) / 2, 0, panelW, windowH};

    const int cx = panel_.x + panelW / 2;
    const int bottom = windowH - 56;

    // Bottom action row is in the same place on every step.
    backButton_     = SDL_Rect{panel_.x, bottom, 84, 40};
    continueButton_ = SDL_Rect{panel_.x + panelW - 150, bottom, 150, 40};
    notSureButton_  = SDL_Rect{continueButton_.x - 116, bottom, 108, 40};

    // Stepper, centred in the middle of the panel.
    const int stepperY = windowH / 2 - 10;
    minusButton_ = SDL_Rect{cx - 150, stepperY, 46, 46};
    plusButton_  = SDL_Rect{cx + 104, stepperY, 46, 46};

    switch (step()) {
    case Step::Welcome:
        // One primary action, one quiet escape hatch.
        hits_.push_back({continueButton_, Action::Begin});
        hits_.push_back({backButton_, Action::SkipSetup});
        break;

    case Step::CycleLength:
    case Step::PeriodLength:
    case Step::Age:
    case Step::LastPeriod:
        hits_.push_back({minusButton_, Action::Minus});
        hits_.push_back({plusButton_, Action::Plus});
        hits_.push_back({continueButton_, Action::Continue});
        hits_.push_back({notSureButton_, Action::NotSure});
        hits_.push_back({backButton_, Action::Back});
        break;

    case Step::Regularity: {
        const Action actions[] = {Action::SetRegular, Action::SetSomewhat,
                                  Action::SetIrregular};
        const int h = 52;
        int y = windowH / 2 - 100;
        for (const Action a : actions) {
            SDL_Rect rect{panel_.x, y, panelW, h};
            choiceButtons_.push_back(rect);
            hits_.push_back({rect, a});
            y += h + 10;
        }
        hits_.push_back({notSureButton_, Action::NotSure});
        hits_.push_back({backButton_, Action::Back});
        break;
    }

    case Step::Summary:
        hits_.push_back({continueButton_, Action::Finish});
        hits_.push_back({backButton_, Action::Back});
        break;
    }
}

// --- Drawing ---------------------------------------------------------------

void Onboarding::draw(SDL_Renderer* r, TextRenderer& text, int /*windowW*/, int windowH,
                      int mouseX, int mouseY) {
    SDL_SetRenderDrawColor(r, color::kBackground.r, color::kBackground.g,
                           color::kBackground.b, 255);
    SDL_RenderClear(r);

    const int cx = panel_.x + panel_.w / 2;
    const int top = 72;

    // Progress dots, one per step.
    {
        const int count = static_cast<int>(steps_.size());
        const int spacing = 16;
        int dotX = cx - (count - 1) * spacing / 2;
        for (int i = 0; i < count; ++i) {
            SDL_Rect dot{dotX - 4, top - 28, 8, 8};
            fillRoundedRect(r, dot, 4, i == stepIndex_ ? color::kPeriod : color::kDim);
            dotX += spacing;
        }
    }

    const int ordinal = editing_ ? stepIndex_ + 1 : stepIndex_;

    switch (step()) {
    case Step::Welcome: {
        text.draw("Period Tracker", cx, windowH / 2 - 140, 34, color::kText,
                  Align::Center, true);
        const char* lines[] = {
            "A few quick questions so the app can estimate",
            "your next period before you've logged anything.",
            "",
            "All of it is optional, and you can change any of",
            "it later. Nothing leaves this computer.",
        };
        int y = windowH / 2 - 76;
        for (const char* line : lines) {
            text.draw(line, cx, y, 15, color::kMuted, Align::Center);
            y += 26;
        }
        drawButton(r, text, continueButton_, "Get started",
                   pointIn(continueButton_, mouseX, mouseY), ButtonStyle::Primary, 15);
        drawButton(r, text, backButton_, "Skip",
                   pointIn(backButton_, mouseX, mouseY), ButtonStyle::Plain, 14);
        break;
    }

    case Step::Summary: {
        text.draw(editing_ ? "Your answers" : "You're all set", cx, top + 10, 28,
                  color::kText, Align::Center, true);

        struct Row { const char* label; std::string value; };
        const std::vector<Row> rows = {
            {"Cycle length",  cycleGiven_  ? std::to_string(cycle_) + " days"  : "Not given"},
            {"Period length", periodGiven_ ? std::to_string(period_) + " days" : "Not given"},
            {"Regularity",    regularity_ == Profile::Regularity::Regular   ? "Regular"
                            : regularity_ == Profile::Regularity::Somewhat  ? "Fairly regular"
                            : regularity_ == Profile::Regularity::Irregular ? "Irregular"
                                                                            : "Not given"},
            {"Age",           ageGiven_ ? std::to_string(age_) : std::string("Not given")},
        };

        int y = top + 80;
        for (const Row& row : rows) {
            SDL_Rect card{panel_.x, y, panel_.w, 48};
            fillRoundedRect(r, card, 10, color::kCard);
            text.draw(row.label, card.x + 18, card.y + 15, 14, color::kMuted);
            const bool given = row.value != "Not given";
            text.draw(row.value, card.x + card.w - 18, card.y + 14, 15,
                      given ? color::kText : color::kDim, Align::Right, given);
            y += 56;
        }

        y += 8;
        text.draw("These give the app a starting point. Every cycle you", cx, y, 13,
                  color::kMuted, Align::Center);
        text.draw("log from here pulls the estimate closer to your own.", cx, y + 20, 13,
                  color::kMuted, Align::Center);

        drawButton(r, text, continueButton_, editing_ ? "Save" : "Start tracking",
                   pointIn(continueButton_, mouseX, mouseY), ButtonStyle::Primary, 15);
        drawButton(r, text, backButton_, "Back",
                   pointIn(backButton_, mouseX, mouseY), ButtonStyle::Plain, 14);
        break;
    }

    default: {
        const Question q = questionFor(ordinal);
        text.draw(q.title, cx, top + 10, 24, color::kText, Align::Center, true);

        // Help text, wrapped by hand at a sensible width.
        {
            const std::string help = q.help;
            std::string line;
            int y = top + 54;
            std::size_t start = 0;
            while (start <= help.size()) {
                const std::size_t space = help.find(' ', start);
                const std::string word = help.substr(start, space - start);
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (text.width(candidate, 13) > panel_.w - 20 && !line.empty()) {
                    text.draw(line, cx, y, 13, color::kMuted, Align::Center);
                    y += 20;
                    line = word;
                } else {
                    line = candidate;
                }
                if (space == std::string::npos) {
                    break;
                }
                start = space + 1;
            }
            if (!line.empty()) {
                text.draw(line, cx, y, 13, color::kMuted, Align::Center);
            }
        }

        if (step() == Step::Regularity) {
            const char* labels[] = {"Regular  -  within a day or two",
                                    "Fairly regular  -  within a few days",
                                    "Irregular  -  hard to predict"};
            const Profile::Regularity values[] = {Profile::Regularity::Regular,
                                                  Profile::Regularity::Somewhat,
                                                  Profile::Regularity::Irregular};
            for (std::size_t i = 0; i < choiceButtons_.size(); ++i) {
                const bool selected = regularity_ == values[i];
                drawButton(r, text, choiceButtons_[i], labels[i],
                           pointIn(choiceButtons_[i], mouseX, mouseY),
                           selected ? ButtonStyle::Selected : ButtonStyle::Plain, 15);
            }
        } else {
            // Stepper: minus, big value, plus.
            const int value = activeValue();
            drawButton(r, text, minusButton_, "-", pointIn(minusButton_, mouseX, mouseY),
                       ButtonStyle::Plain, 22);
            drawButton(r, text, plusButton_, "+", pointIn(plusButton_, mouseX, mouseY),
                       ButtonStyle::Plain, 22);

            const bool given = activeValueGiven();
            const SDL_Color valueColor = given ? color::kText : color::kDim;

            if (step() == Step::LastPeriod) {
                // A day count is hard to picture, so show the date it lands on.
                const Date when = addDays(today(), -daysAgo_);
                const std::string label =
                    value == 0 ? "Today"
                               : std::to_string(value) + (value == 1 ? " day ago" : " days ago");
                text.draw(label, cx, minusButton_.y + 2, 26, valueColor, Align::Center, true);
                text.draw(formatShort(when), cx, minusButton_.y + 40, 14,
                          color::kMuted, Align::Center);
            } else {
                text.draw(std::to_string(value), cx, minusButton_.y - 4, 38, valueColor,
                          Align::Center, true);
                text.draw(q.unit, cx, minusButton_.y + 44, 13, color::kMuted, Align::Center);
            }
        }

        drawButton(r, text, continueButton_, "Continue",
                   pointIn(continueButton_, mouseX, mouseY), ButtonStyle::Primary, 15);
        drawButton(r, text, notSureButton_, "Not sure",
                   pointIn(notSureButton_, mouseX, mouseY), ButtonStyle::Plain, 14);
        drawButton(r, text, backButton_, editing_ && stepIndex_ == 0 ? "Cancel" : "Back",
                   pointIn(backButton_, mouseX, mouseY), ButtonStyle::Plain, 14);
        break;
    }
    }
}
