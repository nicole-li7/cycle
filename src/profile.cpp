#include "profile.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

// Cycles are typically more variable in the years after menarche and in the
// run-up to menopause, so a prediction in those ranges deserves a wider margin.
constexpr int kYoungerThan = 20;
constexpr int kOlderThan   = 45;

std::string regularityToText(Profile::Regularity r) {
    switch (r) {
    case Profile::Regularity::Regular:   return "regular";
    case Profile::Regularity::Somewhat:  return "somewhat";
    case Profile::Regularity::Irregular: return "irregular";
    case Profile::Regularity::Unknown:   break;
    }
    return "unknown";
}

Profile::Regularity regularityFromText(const std::string& s) {
    if (s == "regular")   return Profile::Regularity::Regular;
    if (s == "somewhat")  return Profile::Regularity::Somewhat;
    if (s == "irregular") return Profile::Regularity::Irregular;
    return Profile::Regularity::Unknown;
}

} // namespace

int Profile::ageNow() const {
    if (birthYear <= 0) {
        return 0;
    }
    return int{YMD{today()}.year()} - birthYear;
}

int Profile::priorStrength() const {
    switch (regularity) {
    case Regularity::Regular:   return 4;
    case Regularity::Somewhat:  return 2;
    case Regularity::Irregular: return 1;
    case Regularity::Unknown:   break;
    }
    // No answer given: trust a supplied number a little, a default not at all.
    return typicalCycle > 0 ? 2 : 1;
}

int Profile::baseSpreadDays() const {
    int spread;
    switch (regularity) {
    case Regularity::Regular:   spread = 2; break;
    case Regularity::Somewhat:  spread = 4; break;
    case Regularity::Irregular: spread = 7; break;
    default:                    spread = 5; break;
    }
    if (hasAge()) {
        const int age = ageNow();
        if (age > 0 && (age < kYoungerThan || age >= kOlderThan)) {
            spread += 1;
        }
    }
    return spread;
}

std::string Profile::path() {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = home ? std::filesystem::path(home)
                                     : std::filesystem::current_path();
    dir /= "Library/Application Support/PeriodTracker";
    return (dir / "profile.txt").string();
}

void Profile::load() {
    std::ifstream in(path());
    if (!in) {
        return;   // no file yet - setup hasn't been run
    }
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key   = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "completed")           completedSetup = (value == "1");
        else if (key == "birthYear")      birthYear      = std::atoi(value.c_str());
        else if (key == "typicalCycle")   typicalCycle   = std::atoi(value.c_str());
        else if (key == "typicalPeriod")  typicalPeriod  = std::atoi(value.c_str());
        else if (key == "regularity")     regularity     = regularityFromText(value);
    }
}

void Profile::save() const {
    const std::filesystem::path file = path();
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);

    const std::filesystem::path tmp = file.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return;
        }
        out << "completed="      << (completedSetup ? 1 : 0) << '\n'
            << "birthYear="      << birthYear      << '\n'
            << "typicalCycle="   << typicalCycle   << '\n'
            << "typicalPeriod="  << typicalPeriod  << '\n'
            << "regularity="     << regularityToText(regularity) << '\n';
    }
    std::filesystem::rename(tmp, file, ec);
}
