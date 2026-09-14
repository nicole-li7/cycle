// What you tell the app about yourself during first-run setup.
//
// All of it is optional. Every field has a "not given" value, and a prediction
// can still be made without any of it - the answers just narrow it down. See
// Tracker::recompute() for how these get blended with your logged history.
#pragma once

#include <string>

#include "date.h"

struct Profile {
    // How sure you are of your own cycle. This decides how much weight your
    // self-reported numbers keep once real logged cycles start arriving.
    enum class Regularity { Unknown, Regular, Somewhat, Irregular };

    bool       completedSetup = false;  // false = show the setup screen on launch
    int        birthYear      = 0;      // 0 = not given
    int        typicalCycle   = 0;      // days; 0 = not given
    int        typicalPeriod  = 0;      // days; 0 = not given
    Regularity regularity     = Regularity::Unknown;

    // Age is stored as a birth year so it stays correct as time passes.
    int  ageNow() const;
    bool hasAge() const { return birthYear > 0; }

    // How strongly to trust the self-reported numbers, expressed as "worth this
    // many observed cycles". A confident, regular self-report holds its ground
    // longer; an unsure one gets overtaken by real data almost immediately.
    int priorStrength() const;

    // Starting guess at how many days a prediction could be out by, before any
    // cycles have been logged.
    int baseSpreadDays() const;

    static std::string path();
    void load();
    void save() const;
};
