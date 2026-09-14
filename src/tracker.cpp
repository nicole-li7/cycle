#include "tracker.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>

namespace {

// Sensible fallbacks used when setup was skipped and nothing is logged yet.
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

// Combines what you told us during setup with what has actually been observed.
//
// `prior` is the setup answer and `priorWeight` is how many observed cycles it
// is currently worth. With nothing logged the answer is just the prior; each
// real cycle pulls the result further towards reality. Once priorWeight has
// decayed to zero (see effectivePriorWeight) the setup answer drops out
// entirely and the result is purely your own data.
int blend(int prior, int priorWeight, const std::vector<int>& observed) {
    if (observed.empty()) {
        return prior;
    }
    if (priorWeight <= 0) {
        return static_cast<int>(std::lround(
            std::accumulate(observed.begin(), observed.end(), 0.0) /
            static_cast<double>(observed.size())));
    }
    const double total = static_cast<double>(prior) * priorWeight +
                         std::accumulate(observed.begin(), observed.end(), 0);
    const double weight = priorWeight + static_cast<double>(observed.size());
    return static_cast<int>(std::lround(total / weight));
}

// How much the setup answers should still count for, given how many cycles have
// actually been observed.
//
// This decays to zero rather than levelling off. The averages themselves only
// look at a sliding window of recent cycles, so without this decay the prior
// would keep its share of the weight forever and the estimate could never
// converge on your real cycle length.
int effectivePriorWeight(int priorStrength, int totalObservedCycles) {
    return std::max(0, priorStrength - totalObservedCycles);
}

// Spread of the observed values around their mean, as a whole number of days.
int standardDeviation(const std::vector<int>& values) {
    if (values.size() < 2) {
        return 0;
    }
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                        static_cast<double>(values.size());
    double sum = 0.0;
    for (int v : values) {
        const double diff = v - mean;
        sum += diff * diff;
    }
    return static_cast<int>(std::lround(std::sqrt(sum / static_cast<double>(values.size()))));
}

// Keeps only the most recent `window` entries.
void trimToWindow(std::vector<int>& values, std::size_t window) {
    if (values.size() > window) {
        values.erase(values.begin(), values.end() - static_cast<long>(window));
    }
}

} // namespace

Tracker::Tracker() {
    profile_.load();
    load();
}

// --- Storage ---------------------------------------------------------------

std::string Tracker::dataPath() const {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = home ? std::filesystem::path(home)
                                     : std::filesystem::current_path();
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

void Tracker::logRange(Date start, int days) {
    for (int i = 0; i < days; ++i) {
        logged_.insert(addDays(start, i));
    }
    recompute();
    save();
}

void Tracker::setProfile(const Profile& p) {
    profile_ = p;
    profile_.save();
    recompute();
}

// --- Analysis --------------------------------------------------------------

void Tracker::recompute() {
    cycles_.clear();
    predictedPeriod_.clear();
    fertile_.clear();
    ovulation_.clear();
    prediction_ = Prediction{};

    // Even with nothing logged, the setup answers are worth showing.
    prediction_.avgCycleDays =
        profile_.typicalCycle > 0 ? profile_.typicalCycle : kDefaultCycleDays;
    prediction_.avgPeriodDays =
        profile_.typicalPeriod > 0 ? profile_.typicalPeriod : kDefaultPeriodDays;
    prediction_.spreadDays = profile_.baseSpreadDays();

    if (logged_.empty()) {
        return;   // nothing to anchor a date to yet
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

    // Collect the recent, plausible cycle lengths.
    std::vector<int> cycleLengths;
    for (std::size_t i = 0; i + 1 < cycles_.size(); ++i) {
        const int len = cycles_[i].cycleDays;
        if (len >= kMinPlausibleCycle && len <= kMaxPlausibleCycle) {
            cycleLengths.push_back(len);
        }
    }
    // Counted before the window is applied: the prior should keep fading as you
    // keep logging, even though the average itself only looks at recent cycles.
    const int totalObservedCycles = static_cast<int>(cycleLengths.size());
    trimToWindow(cycleLengths, kAverageWindow);

    // Collect period lengths, skipping the most recent one if it might still be
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
    trimToWindow(periodLengths, kAverageWindow);

    // Blend the setup answers with the observed history.
    const int priorWeight  = effectivePriorWeight(profile_.priorStrength(),
                                                  totalObservedCycles);
    const int priorCycle   = profile_.typicalCycle  > 0 ? profile_.typicalCycle
                                                        : kDefaultCycleDays;
    const int priorPeriod  = profile_.typicalPeriod > 0 ? profile_.typicalPeriod
                                                        : kDefaultPeriodDays;

    prediction_.valid          = true;
    prediction_.observedCycles = totalObservedCycles;
    prediction_.estimated      = cycleLengths.empty();
    prediction_.avgCycleDays   = blend(priorCycle, priorWeight, cycleLengths);
    prediction_.avgPeriodDays  = blend(priorPeriod, priorWeight, periodLengths);

    // The margin of error starts from how regular you said your cycles are and
    // converges on how variable they actually turn out to be.
    const int baseSpread = profile_.baseSpreadDays();
    if (cycleLengths.size() >= 2) {
        const int observedSpread = std::max(1, standardDeviation(cycleLengths));
        const std::vector<int> spreadSamples(cycleLengths.size(), observedSpread);
        prediction_.spreadDays = blend(baseSpread, priorWeight, spreadSamples);
    } else {
        prediction_.spreadDays = baseSpread;
    }
    prediction_.spreadDays = std::clamp(prediction_.spreadDays, 1, 10);

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
