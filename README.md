# Period Tracker

A small offline period tracker for macOS. C++20, SDL2, built with CMake.

Everything is stored in plain text on this machine, under
`~/Library/Application Support/PeriodTracker/`:

| File | Contents |
| --- | --- |
| `log.csv` | One ISO date per line - the days you logged |
| `profile.txt` | Your setup answers, as `key=value` lines |

No network, no account, no telemetry. You can read either file, back them up, or
edit them by hand, and the app will pick up the changes next time it starts.

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # once, or after editing CMakeLists.txt
cmake --build build
./build/app
```

In VS Code: **Cmd-Shift-B** builds, **F5** runs it under the debugger.

## First-run setup

The first time you open the app it asks a short series of questions: usual cycle
length, usual period length, how regular your cycles are, your age, and when your
last period started. Every one of them is optional - "Not sure" skips any
question, and "Skip" on the first screen skips the lot.

The point of the questions is to have something to predict from on day one,
before any cycles have been logged. You can change the answers later with the
**Setup** button at the top of the sidebar.

If you give a last period start date, those days are logged on the calendar for
you. They're ordinary logged days, so click to correct them if the dates are off.

## Using it

| Action | How |
| --- | --- |
| Log / unlog a period day | Click the day |
| Log today | Space |
| Previous / next month | Left / Right arrow, or the `<` `>` buttons |
| Jump back to today | `T`, or the Today button |
| Change your setup answers | The Setup button in the sidebar |
| Quit | Escape, or close the window |

Clicking a greyed-out day from a neighbouring month jumps to that month instead
of logging it.

## What the colours mean

| | |
| --- | --- |
| Solid pink | A day you logged |
| Pink outline | Predicted period |
| Teal fill | Estimated fertile window |
| Teal outline | Estimated peak (ovulation) day |
| White ring | Today |

## How the predictions work

`src/tracker.cpp` holds all of it.

Consecutive logged days are grouped into runs; each run is one period, and the
day a run starts is the day that cycle began. The gap between consecutive starts
is a cycle length.

- **Average cycle** — the mean of the last 6 cycle lengths. Gaps shorter than 15
  or longer than 60 days are skipped, since those are usually a missed log
  rather than a real cycle.
- **Average period** — the mean of the last 6 run lengths. The most recent run is
  skipped if it might still be in progress, so a half-logged period doesn't drag
  the average down.
- **Next period** — the last period's start plus the average cycle length.
- **Ovulation** — counted *backwards* from the next predicted period (14 days
  before it), not forwards from the last one. The luteal phase is the more
  stable half of the cycle, so this is the more reliable direction.
- **Fertile window** — the 5 days before ovulation through 1 day after.

### Why it gets more accurate as you log

Your setup answers act as a starting guess that real data gradually replaces.

The guess is given a weight in "number of cycles it counts for", set by how
regular you said your cycles are - 4 for regular, 2 for fairly regular, 1 for
irregular or unanswered. A confident answer holds its ground longer; an unsure
one is overtaken almost immediately.

That weight then drops by one for every cycle actually observed, so it reaches
zero and the setup answers stop mattering entirely. The decay matters: the
averages only look at a sliding window of recent cycles, so without it the guess
would keep a permanent share of the weight and the estimate could never converge
on your real cycle length.

Told it 30 days, actually 26, "regular":

| Cycles logged | Estimate | Margin |
| --- | --- | --- |
| 0 | 30 days | ± 2 days |
| 1 | 29 days | ± 2 days |
| 2 | 28 days | ± 2 days |
| 3 | 27 days | ± 1 day |
| 4 | 26 days | ± 1 day |

The **margin of error** shown next to the predicted date starts from your
regularity answer (± 2, 4 or 7 days), widened by a day if you're under 20 or over
45, and converges on how variable your cycles actually turn out to be. Age is
used for nothing else.

If setup is skipped entirely and nothing is logged, it falls back to a 28-day
cycle and a 5-day period.

These are averages of your own history, not a medical prediction — cycles shift
with stress, illness, travel and plenty else. Don't use the fertile window as
contraception.

## Layout

| File | What's in it |
| --- | --- |
| `src/main.cpp` | Window, main loop, switching between the two screens |
| `src/tracker.h/.cpp` | The data: logging, cycle analysis, predictions, saving |
| `src/profile.h/.cpp` | Your setup answers, and how much they're trusted |
| `src/onboarding.h/.cpp` | The first-run setup screen |
| `src/date.h/.cpp` | Date helpers built on C++20 `<chrono>` |
| `src/ui.h/.cpp` | Palette, fonts, shared widgets, calendar layout and drawing |

To add a source file, put it in the `add_executable(app ...)` list in
`CMakeLists.txt` and re-run the configure step.

## Dependencies

```sh
brew install cmake sdl2 sdl2_ttf
```
