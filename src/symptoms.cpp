#include "symptoms.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

struct SymptomNames { const char* display; const char* token; };

// Index matches the Symptom enum.
constexpr SymptomNames kSymptoms[kSymptomCount] = {
    {"Cramps",        "cramps"},
    {"Headache",      "headache"},
    {"Bloating",      "bloating"},
    {"Fatigue",       "fatigue"},
    {"Nausea",        "nausea"},
    {"Backache",      "backache"},
    {"Sore breasts",  "sore_breasts"},
    {"Acne",          "acne"},
};

// Splits "a|b|c" on '|'.
std::vector<std::string> splitOnPipe(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(text);
    while (std::getline(stream, current, '|')) {
        if (!current.empty()) {
            parts.push_back(current);
        }
    }
    return parts;
}

// Splits a CSV line into exactly `count` fields, tolerating missing trailing
// ones so a row written by an older version still loads.
std::vector<std::string> splitCsv(const std::string& line, std::size_t count) {
    std::vector<std::string> fields;
    std::string current;
    std::istringstream stream(line);
    while (std::getline(stream, current, ',')) {
        fields.push_back(current);
    }
    fields.resize(count);
    return fields;
}

} // namespace

const char* symptomName(Symptom s) {
    return kSymptoms[static_cast<std::size_t>(s)].display;
}

const char* symptomToken(Symptom s) {
    return kSymptoms[static_cast<std::size_t>(s)].token;
}

const char* flowName(Flow f) {
    switch (f) {
    case Flow::Light:  return "Light";
    case Flow::Medium: return "Medium";
    case Flow::Heavy:  return "Heavy";
    case Flow::None:   break;
    }
    return "";
}

const char* flowToken(Flow f) {
    switch (f) {
    case Flow::Light:  return "light";
    case Flow::Medium: return "medium";
    case Flow::Heavy:  return "heavy";
    case Flow::None:   break;
    }
    return "";
}

const char* moodName(Mood m) {
    switch (m) {
    case Mood::Good:      return "Good";
    case Mood::Calm:      return "Calm";
    case Mood::Irritable: return "Irritable";
    case Mood::Anxious:   return "Anxious";
    case Mood::Low:       return "Low";
    case Mood::None:      break;
    }
    return "";
}

const char* moodToken(Mood m) {
    switch (m) {
    case Mood::Good:      return "good";
    case Mood::Calm:      return "calm";
    case Mood::Irritable: return "irritable";
    case Mood::Anxious:   return "anxious";
    case Mood::Low:       return "low";
    case Mood::None:      break;
    }
    return "";
}

int DayEntry::symptomCount() const {
    return static_cast<int>(std::count(symptoms.begin(), symptoms.end(), true));
}

bool DayEntry::empty() const {
    return flow == Flow::None && mood == Mood::None && symptomCount() == 0;
}

std::string SymptomLog::path() {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = home ? std::filesystem::path(home)
                                     : std::filesystem::current_path();
    dir /= "Library/Application Support/PeriodTracker";
    return (dir / "symptoms.csv").string();
}

void SymptomLog::load() {
    entries_.clear();

    std::ifstream in(path());
    if (!in) {
        return;   // nothing recorded yet
    }

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;   // blank line, or the header comment written by save()
        }

        const std::vector<std::string> fields = splitCsv(line, 4);
        Date day;
        if (!parseISO(fields[0], day)) {
            continue;   // not a date - skip rather than abort the whole file
        }

        DayEntry entry;
        for (Flow f : {Flow::Light, Flow::Medium, Flow::Heavy}) {
            if (fields[1] == flowToken(f)) {
                entry.flow = f;
            }
        }
        for (const std::string& token : splitOnPipe(fields[2])) {
            for (std::size_t i = 0; i < kSymptomCount; ++i) {
                if (token == kSymptoms[i].token) {
                    entry.symptoms[i] = true;
                }
            }
        }
        for (Mood m : {Mood::Good, Mood::Calm, Mood::Irritable, Mood::Anxious, Mood::Low}) {
            if (fields[3] == moodToken(m)) {
                entry.mood = m;
            }
        }

        if (!entry.empty()) {
            entries_[day] = entry;
        }
    }
}

void SymptomLog::save() const {
    const std::filesystem::path file = path();
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);

    // Written to a temporary file and renamed, so an interrupted save can never
    // leave a half-written log behind.
    const std::filesystem::path tmp = file.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return;
        }
        out << "# date,flow,symptoms,mood\n";
        for (const auto& [day, entry] : entries_) {
            std::string symptoms;
            for (std::size_t i = 0; i < kSymptomCount; ++i) {
                if (entry.symptoms[i]) {
                    if (!symptoms.empty()) {
                        symptoms += '|';
                    }
                    symptoms += kSymptoms[i].token;
                }
            }
            out << formatISO(day) << ',' << flowToken(entry.flow) << ','
                << symptoms << ',' << moodToken(entry.mood) << '\n';
        }
    }
    std::filesystem::rename(tmp, file, ec);
}

bool SymptomLog::has(Date d) const {
    return entries_.count(d) > 0;
}

DayEntry SymptomLog::entryFor(Date d) const {
    if (auto it = entries_.find(d); it != entries_.end()) {
        return it->second;
    }
    return DayEntry{};
}

void SymptomLog::set(Date d, const DayEntry& entry) {
    if (entry.empty()) {
        entries_.erase(d);
    } else {
        entries_[d] = entry;
    }
}

std::vector<std::pair<Symptom, int>> SymptomLog::mostCommon() const {
    std::vector<std::pair<Symptom, int>> counts;
    for (std::size_t i = 0; i < kSymptomCount; ++i) {
        int total = 0;
        for (const auto& [day, entry] : entries_) {
            (void)day;
            if (entry.symptoms[i]) {
                ++total;
            }
        }
        if (total > 0) {
            counts.emplace_back(static_cast<Symptom>(i), total);
        }
    }
    std::sort(counts.begin(), counts.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    return counts;
}
