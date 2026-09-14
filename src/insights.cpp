#include "insights.h"

#include <algorithm>
#include <string>

namespace {

// A note more than this many days after a period start almost certainly belongs
// to a cycle that was never logged, so it isn't placed on the cycle track.
constexpr int kMaxPlacedCycleDay = 60;

// Below this many placed occurrences, "typically day N" would be noise.
constexpr std::size_t kMinForTypical = 3;

int median(std::vector<int> values) {
    if (values.empty()) {
        return 0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values[mid];
    }
    return (values[mid - 1] + values[mid] + 1) / 2;
}

// Which day of which cycle a date falls on, or 0 if it can't be placed.
int cycleDayOf(Date d, const std::vector<Cycle>& cycles) {
    // The last cycle that began on or before this date.
    const Cycle* owner = nullptr;
    for (const Cycle& c : cycles) {
        if (c.start <= d) {
            owner = &c;
        } else {
            break;
        }
    }
    if (owner == nullptr) {
        return 0;   // predates everything logged
    }
    const int day = daysBetween(owner->start, d) + 1;
    return (day >= 1 && day <= kMaxPlacedCycleDay) ? day : 0;
}

} // namespace

Insights computeInsights(const Tracker& tracker) {
    Insights out;

    const Prediction& p = tracker.prediction();
    out.cycleLength  = std::max(10, p.avgCycleDays);
    out.periodLength = std::clamp(p.avgPeriodDays, 1, out.cycleLength);
    out.ovulationDay = std::max(1, out.cycleLength - 14);
    out.fertileFrom  = std::max(1, out.ovulationDay - 5);
    out.fertileTo    = std::min(out.cycleLength, out.ovulationDay + 1);

    const std::vector<Cycle>& cycles = tracker.cycles();
    const auto& entries = tracker.symptomLog().entries();

    std::vector<SymptomStat> stats(kSymptomCount);
    for (std::size_t i = 0; i < kSymptomCount; ++i) {
        stats[i].symptom = static_cast<Symptom>(i);
    }

    for (const auto& [day, entry] : entries) {
        ++out.daysWithNotes;
        const int cycleDay = cycleDayOf(day, cycles);
        if (cycleDay > 0) {
            ++out.placedDays;
        }

        for (std::size_t i = 0; i < kSymptomCount; ++i) {
            if (!entry.symptoms[i]) {
                continue;
            }
            ++stats[i].totalDays;
            if (cycleDay > 0) {
                stats[i].cycleDays.push_back(cycleDay);
            }
        }
        out.moods[static_cast<std::size_t>(entry.mood)] += 1;
        out.flows[static_cast<std::size_t>(entry.flow)] += 1;
    }

    for (SymptomStat& stat : stats) {
        if (stat.totalDays == 0) {
            continue;
        }
        if (stat.cycleDays.size() >= kMinForTypical) {
            stat.typicalCycleDay = median(stat.cycleDays);
        }
        out.symptoms.push_back(stat);
    }
    std::sort(out.symptoms.begin(), out.symptoms.end(),
              [](const SymptomStat& a, const SymptomStat& b) {
                  return a.totalDays > b.totalDays;
              });

    out.hasNotes = out.daysWithNotes > 0;
    return out;
}

// --- The screen ------------------------------------------------------------

namespace {

constexpr int kMargin   = 40;
constexpr int kRowH     = 32;
constexpr int kLabelW   = 150;                    // symptom name column
constexpr int kTrackX   = kMargin + kLabelW;      // where the cycle track starts
constexpr int kCountW   = 80;                     // "5 days" column
constexpr int kTypicalW = 150;                    // "typically day 12" column

// Pale versions of the calendar's colours, for shading the cycle track. They
// only ever sit behind marks, never behind text.
constexpr SDL_Color kTrackBase    = {240, 232, 224, 255};
constexpr SDL_Color kTrackPeriod  = {243, 206, 218, 255};
constexpr SDL_Color kTrackFertile = {214, 232, 243, 255};

std::string plural(int n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

// Maps a cycle day onto a pixel offset within the track.
int dayToX(int day, int trackX, int trackW, int cycleLength) {
    const double t = (day - 1) / static_cast<double>(std::max(1, cycleLength));
    return trackX + static_cast<int>(t * trackW);
}

void drawCycleTrack(SDL_Renderer* r, const Insights& in, SDL_Rect track,
                    const std::vector<int>& marks) {
    fillRoundedRect(r, track, 5, kTrackBase);

    // Shade the period at the start and the fertile window in the middle, so a
    // mark's position reads against the cycle rather than against a bare bar.
    auto shade = [&](int fromDay, int toDay, SDL_Color c) {
        const int x0 = dayToX(fromDay, track.x, track.w, in.cycleLength);
        const int x1 = dayToX(toDay + 1, track.x, track.w, in.cycleLength);
        SDL_Rect band{x0, track.y, std::max(2, x1 - x0), track.h};
        fillRoundedRect(r, band, 5, c);
    };
    shade(1, in.periodLength, kTrackPeriod);
    shade(in.fertileFrom, in.fertileTo, kTrackFertile);

    // How many times each day appears, so repeats show up as taller marks.
    std::vector<int> counts(static_cast<std::size_t>(in.cycleLength) + 2, 0);
    for (int day : marks) {
        if (day >= 1 && day <= in.cycleLength) {
            ++counts[static_cast<std::size_t>(day)];
        }
    }
    const int peak = std::max(1, *std::max_element(counts.begin(), counts.end()));

    for (int day = 1; day <= in.cycleLength; ++day) {
        const int n = counts[static_cast<std::size_t>(day)];
        if (n == 0) {
            continue;
        }
        const int h = 6 + (track.h - 8) * n / peak;
        SDL_Rect mark{dayToX(day, track.x, track.w, in.cycleLength),
                      track.y + (track.h - h) / 2, 3, h};
        fillRoundedRect(r, mark, 1, color::kPeriod);
    }
}

} // namespace

void InsightsScreen::layout(int windowW, int /*windowH*/) {
    closeButton = SDL_Rect{windowW - kMargin - 90, kMargin - 4, 90, 36};
}

void drawInsights(SDL_Renderer* r, TextRenderer& text, const Insights& in,
                  const InsightsScreen& screen, int windowW, int windowH,
                  int mouseX, int mouseY) {
    SDL_SetRenderDrawColor(r, color::kBackground.r, color::kBackground.g,
                           color::kBackground.b, 255);
    SDL_RenderClear(r);

    text.draw("Symptom summary", kMargin, kMargin, 26, color::kText, Align::Left, true);
    drawButton(r, text, screen.closeButton, "Close",
               pointIn(screen.closeButton, mouseX, mouseY), ButtonStyle::Plain, 14);

    if (!in.hasNotes) {
        text.draw("Nothing logged yet.", windowW / 2, windowH / 2 - 40, 20,
                  color::kMuted, Align::Center, true);
        text.draw("Click a day on the calendar and add a symptom,", windowW / 2,
                  windowH / 2, 14, color::kDim, Align::Center);
        text.draw("a flow or a mood. They'll be summarised here.", windowW / 2,
                  windowH / 2 + 22, 14, color::kDim, Align::Center);
        return;
    }

    std::string subtitle = plural(in.daysWithNotes, "day", "days") + " with notes";
    if (in.placedDays < in.daysWithNotes) {
        subtitle += "  -  " + std::to_string(in.placedDays) + " placed in a cycle";
    }
    subtitle += "  -  typical cycle " + plural(in.cycleLength, "day", "days");
    text.draw(subtitle, kMargin, kMargin + 34, 13, color::kMuted);

    int y = kMargin + 76;

    const int trackW = windowW - kTrackX - kMargin - kCountW - kTypicalW;

    if (in.symptoms.empty()) {
        text.draw("No symptoms recorded yet - only flow or mood so far.", kMargin, y,
                  14, color::kMuted);
        y += 40;
    } else {
        text.draw("WHERE THEY FALL IN YOUR CYCLE", kMargin, y, 11, color::kMuted,
                  Align::Left, true);
        y += 26;

        // Track key, aligned with the tracks below it.
        {
            SDL_Rect key{kTrackX, y, trackW, 14};
            drawCycleTrack(r, in, key, {});
            text.draw("day 1", kTrackX, y + 18, 10, color::kDim);
            text.draw("period", kTrackX + 2, y - 14, 10, color::kPeriod, Align::Left, true);
            const int fertileX = dayToX(in.fertileFrom, kTrackX, trackW, in.cycleLength);
            text.draw("fertile", fertileX, y - 14, 10, color::kOvulation, Align::Left, true);
            text.draw("day " + std::to_string(in.cycleLength), kTrackX + trackW, y + 18,
                      10, color::kDim, Align::Right);
            y += 40;
        }

        for (const SymptomStat& stat : in.symptoms) {
            if (y + kRowH > windowH - kMargin - 120) {
                break;   // out of room; the rest are the rarest anyway
            }
            text.draw(symptomName(stat.symptom), kMargin, y + 6, 14, color::kText);

            SDL_Rect track{kTrackX, y, trackW, 22};
            drawCycleTrack(r, in, track, stat.cycleDays);

            text.draw(plural(stat.totalDays, "day", "days"), kTrackX + trackW + 20,
                      y + 6, 13, color::kMuted);

            const int typicalX = kTrackX + trackW + 20 + kCountW;
            if (stat.typicalCycleDay > 0) {
                text.draw("typically day " + std::to_string(stat.typicalCycleDay),
                          typicalX, y + 6, 13, color::kText);
            } else {
                text.draw("not enough yet", typicalX, y + 6, 13, color::kDim);
            }
            y += kRowH + 6;
        }
    }

    // --- Mood and flow, along the bottom ---
    y = windowH - kMargin - 96;
    text.draw("MOOD", kMargin, y, 11, color::kMuted, Align::Left, true);
    text.draw("FLOW", windowW / 2, y, 11, color::kMuted, Align::Left, true);
    y += 24;

    {
        int x = kMargin;
        bool any = false;
        for (Mood m : {Mood::Good, Mood::Calm, Mood::Irritable, Mood::Anxious, Mood::Low}) {
            const int n = in.moods[static_cast<std::size_t>(m)];
            if (n == 0) {
                continue;
            }
            any = true;
            const std::string label = std::string(moodName(m)) + "  " + std::to_string(n);
            const int w = text.width(label, 13) + 22;
            SDL_Rect chip{x, y, w, 28};
            fillRoundedRect(r, chip, 7, color::kCard);
            text.draw(label, x + 11, y + 6, 13, color::kText);
            x += w + 8;
        }
        if (!any) {
            text.draw("None recorded", kMargin, y + 6, 13, color::kDim);
        }
    }
    {
        int x = windowW / 2;
        bool any = false;
        for (Flow f : {Flow::Light, Flow::Medium, Flow::Heavy}) {
            const int n = in.flows[static_cast<std::size_t>(f)];
            if (n == 0) {
                continue;
            }
            any = true;
            const std::string label = std::string(flowName(f)) + "  " + std::to_string(n);
            const int w = text.width(label, 13) + 22;
            SDL_Rect chip{x, y, w, 28};
            fillRoundedRect(r, chip, 7, color::kCard);
            text.draw(label, x + 11, y + 6, 13, color::kText);
            x += w + 8;
        }
        if (!any) {
            text.draw("None recorded", windowW / 2, y + 6, 13, color::kDim);
        }
    }

    text.draw("These are your own notes counted up. They do not change the "
              "predicted dates.",
              kMargin, windowH - kMargin - 10, 12, color::kDim);
}
