// The data model: which days were logged as period days, what that implies
// about cycle length, and where the next period is predicted to fall.
//
// There is no server and no network. Everything lives in one small text file
// under ~/Library/Application Support/PeriodTracker/.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "date.h"

// One past period: a run of consecutive logged days, plus how long it was
// until the next one began.
struct Cycle {
    Date start{};
    int  periodDays = 0;   // how many consecutive days were logged
    int  cycleDays  = 0;   // days from this start to the next start; 0 if this
                           // is the most recent cycle (not yet known)
};

// What we expect to happen next. `estimated` means there wasn't enough history
// yet, so typical defaults (28-day cycle, 5-day period) were used instead.
struct Prediction {
    bool valid        = false;  // false when nothing has been logged at all
    bool estimated    = true;
    Date nextStart{};
    int  avgCycleDays  = 28;
    int  avgPeriodDays = 5;
};

// How a given day should be shown on the calendar. Ordered by priority:
// a logged day always wins over a prediction.
enum class DayKind {
    Normal,
    Logged,           // user marked this as a period day
    PredictedPeriod,  // expected period, not yet logged
    Fertile,          // estimated fertile window
    Ovulation,        // estimated peak day
};

class Tracker {
public:
    Tracker();

    // --- Reading ---
    bool isLogged(Date d) const { return logged_.count(d) > 0; }
    DayKind kindFor(Date d) const;

    const std::vector<Cycle>& cycles() const { return cycles_; }
    const Prediction& prediction() const { return prediction_; }

    // Day number within the current cycle (1 = first day of the last period).
    // Returns 0 if unknown.
    int currentCycleDay() const;

    // If a period is in progress today, which day of it we're on (1-based),
    // otherwise 0.
    int currentPeriodDay() const;

    // --- Writing --- (both re-run the analysis and save to disk)
    void toggle(Date d);

    // --- Storage ---
    std::string dataPath() const;
    void load();
    void save() const;

private:
    void recompute();

    std::set<Date>     logged_;
    std::vector<Cycle> cycles_;
    Prediction         prediction_;

    // Precomputed so the calendar can colour a day with a single lookup.
    std::set<Date> predictedPeriod_;
    std::set<Date> fertile_;
    std::set<Date> ovulation_;
};
