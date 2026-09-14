// What you record about a single day, beyond whether it was a period day.
//
// Period days themselves stay in log.csv, untouched by any of this. Symptoms,
// flow and mood live alongside in symptoms.csv, so an older log file still
// loads and a day can carry notes without being a period day.
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "date.h"

enum class Flow { None, Light, Medium, Heavy };

enum class Mood { None, Good, Calm, Irritable, Anxious, Low };

enum class Symptom {
    Cramps, Headache, Bloating, Fatigue,
    Nausea, Backache, SoreBreasts, Acne,
    Count,   // keep last - the number of symptoms
};

constexpr std::size_t kSymptomCount = static_cast<std::size_t>(Symptom::Count);

// Display names, and the short tokens written to disk. The tokens are kept
// separate from the display names deliberately: renaming a label in the UI
// should never silently orphan everything already saved.
const char* symptomName(Symptom s);
const char* symptomToken(Symptom s);
const char* flowName(Flow f);
const char* flowToken(Flow f);
const char* moodName(Mood m);
const char* moodToken(Mood m);

struct DayEntry {
    Flow flow = Flow::None;
    Mood mood = Mood::None;
    std::array<bool, kSymptomCount> symptoms{};

    bool has(Symptom s) const { return symptoms[static_cast<std::size_t>(s)]; }
    void toggle(Symptom s) {
        auto& slot = symptoms[static_cast<std::size_t>(s)];
        slot = !slot;
    }
    int  symptomCount() const;
    bool empty() const;   // nothing worth saving
};

class SymptomLog {
public:
    static std::string path();

    void load();
    void save() const;

    bool     has(Date d) const;
    DayEntry entryFor(Date d) const;   // a default entry if the day has none

    // Storing an empty entry removes the day rather than keeping a blank row.
    void set(Date d, const DayEntry& entry);

    // How often each symptom has been recorded, most frequent first.
    std::vector<std::pair<Symptom, int>> mostCommon() const;

private:
    std::map<Date, DayEntry> entries_;
};
