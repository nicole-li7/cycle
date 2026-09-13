#include "tracker.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>

namespace {

// Sensible fallbacks used until there is enough history to average.
constexpr int kDefaultCycleDays  = 28;
constexpr int kDefaultPeriodDays = 5;

// Cycle lengths outside this range are almost certainly a typo or a missed
// log rather than a real cycle, so they're ignored when averaging.
constexpr int kMinPlausibleCycle = 15;
constexpr int kMaxPlausibleCycle = 60;

// Only the recent past is representative, so averages use a sliding window.
constexpr std::size_t kAverageWindow = 6;

// How far ahead to project predictions, so scrolling forward keeps showing them.
constexpr int kFutureCycles = 13;

// Ovulation is estimated by the luteal-phase rule: it tends to fall a roughly
// fixed number of days *before* the next period, not after the last one.
constexpr int kLutealPhaseDays = 14;
constexpr int kFertileBefore   = 5;  // sperm survival
constexpr int kFertileAfter    = 1;  // egg survival

int averageOf(const std::vector<int>& values) {
    if (values.empty()) {
        return 0;
    }
    const int sum = std::accumulate(values.begin(), values.end(), 0);
    // Round to nearest rather than truncating.
    return (sum + static_cast<int>(values.size()) / 2) / static_cast<int>(values.size());
}

} // namespace

Tracker::Tracker() {
    load();
}

// --- Storage ---------------------------------------------------------------

std::string Tracker::dataPath() const {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = home ? std::filesystem::path(home) : std::filesystem::current_path();
    dir /= "Library/Application Support/PeriodTracker";
    return (dir / "log.csv").string();
}

void Tracker::load() {
    logged_.clear();

    std::ifstream in(dataPath());
    if (in) {  // a missing file just means "nothing logged yet"
        std::string line;
        while (std::getline(in, line)) {
            // Tolerate trailing whitespace / CR from hand-edited files.
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            Date d;
            if (parseISO(line, d)) {
                logged_.insert(d);
            }
        }
    }
    recompute();
}

void Tracker::save() const {
    const std::filesystem::path path = dataPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    // Write to a temporary file and rename over the real one, so an interrupted
    // save can never leave a half-written log behind.
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return;
        }
        for (Date d : logged_) {
            out << formatISO(d) << '\n';
        }
    }
    std::filesystem::rename(tmp, path, ec);
}

// --- Editing ---------------------------------------------------------------

void Tracker::toggle(Date d) {
    if (auto it = logged_.find(d); it != logged_.end()) {
        logged_.erase(it);
    } else {
        logged_.insert(d);
    }
    recompute();
    save();
}

// --- Analysis --------------------------------------------------------------

void Tracker::recompute() {
    cycles_.clear();
    predictedPeriod_.clear();
    fertile_.clear();
    ovulation_.clear();
    prediction_ = Prediction{};

    if (logged_.empty()) {
        return;
    }

    // Walk the sorted days and group consecutive ones into runs. Each run is
    // one period; the day a run starts is the day that cycle began.
    for (auto it = logged_.begin(); it != logged_.end();) {
        const Date start = *it;
        int length = 1;
        auto next = std::next(it);
        while (next != logged_.end() && *next == addDays(start, length)) {
            ++length;
            ++next;
        }
        cycles_.push_back(Cycle{start, length, 0});
        it = next;
    }

    // Fill in how long each cycle lasted (start-to-start). The final cycle has
    // no successor, so its length stays 0 = unknown.
    for (std::size_t i = 0; i + 1 < cycles_.size(); ++i) {
        cycles_[i].cycleDays = daysBetween(cycles_[i].start, cycles_[i + 1].start);
    }

    // Average the recent, plausible cycle lengths.
    std::vector<int> cycleLengths;
    for (std::size_t i = 0; i + 1 < cycles_.size(); ++i) {
        const int len = cycles_[i].cycleDays;
        if (len >= kMinPlausibleCycle && len <= kMaxPlausibleCycle) {
            cycleLengths.push_back(len);
        }
    }
    if (cycleLengths.size() > kAverageWindow) {
        cycleLengths.erase(cycleLengths.begin(), cycleLengths.end() - static_cast<long>(kAverageWindow));
    }

    // Average period length, skipping the most recent one if it might still be
    // ongoing - counting a period that's only half logged would drag the
    // average down.
    std::vector<int> periodLengths;
    for (std::size_t i = 0; i < cycles_.size(); ++i) {
        const bool isLast = (i + 1 == cycles_.size());
        const Date lastDay = addDays(cycles_[i].start, cycles_[i].periodDays - 1);
        const bool maybeOngoing = isLast && daysBetween(lastDay, today()) <= 1;
        if (!maybeOngoing) {
            periodLengths.push_back(cycles_[i].periodDays);
        }
    }
    if (periodLengths.size() > kAverageWindow) {
        periodLengths.erase(periodLengths.begin(), periodLengths.end() - static_cast<long>(kAverageWindow));
    }

    prediction_.valid     = true;
    prediction_.estimated = cycleLengths.empty();
    prediction_.avgCycleDays =
        cycleLengths.empty() ? kDefaultCycleDays : averageOf(cycleLengths);
    prediction_.avgPeriodDays =
        periodLengths.empty() ? kDefaultPeriodDays : averageOf(periodLengths);

    const int cycleLen  = prediction_.avgCycleDays;
    const int periodLen = prediction_.avgPeriodDays;

    // Project forward from the most recent period. If the log is stale, keep
    // stepping until the prediction is actually in the future.
    Date next = addDays(cycles_.back().start, cycleLen);
    const Date now = today();
    while (daysBetween(next, now) > 0) {
        next = addDays(next, cycleLen);
    }
    prediction_.nextStart = next;

    for (int cycle = 0; cycle < kFutureCycles; ++cycle) {
        const Date start = addDays(next, cycle * cycleLen);

        for (int i = 0; i < periodLen; ++i) {
            predictedPeriod_.insert(addDays(start, i));
        }

        // Ovulation is counted back from the period it precedes.
        const Date ovulation = addDays(start, -kLutealPhaseDays);
        ovulation_.insert(ovulation);
        for (int i = -kFertileBefore; i <= kFertileAfter; ++i) {
            fertile_.insert(addDays(ovulation, i));
        }
    }
}

// --- Queries ---------------------------------------------------------------

DayKind Tracker::kindFor(Date d) const {
    if (logged_.count(d))          return DayKind::Logged;
    if (predictedPeriod_.count(d)) return DayKind::PredictedPeriod;
    if (ovulation_.count(d))       return DayKind::Ovulation;
    if (fertile_.count(d))         return DayKind::Fertile;
    return DayKind::Normal;
}

int Tracker::currentCycleDay() const {
    if (cycles_.empty()) {
        return 0;
    }
    const int elapsed = daysBetween(cycles_.back().start, today());
    return elapsed >= 0 ? elapsed + 1 : 0;
}

int Tracker::currentPeriodDay() const {
    if (!isLogged(today())) {
        return 0;
    }
    // Walk backwards from today for as long as days are logged.
    int day = 1;
    while (isLogged(addDays(today(), -day))) {
        ++day;
    }
    return day;
}
