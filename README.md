# Period Tracker

A small offline period tracker for macOS. C++20, SDL2, built with CMake.

Everything you log is stored in a plain text file on this machine:

    ~/Library/Application Support/PeriodTracker/log.csv

One ISO date per line, nothing else. No network, no account, no telemetry. You
can read it, back it up, or edit it by hand, and the app will pick up the changes
next time it starts.

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # once, or after editing CMakeLists.txt
cmake --build build
./build/app
```

In VS Code: **Cmd-Shift-B** builds, **F5** runs it under the debugger.

## Using it

| Action | How |
| --- | --- |
| Log / unlog a period day | Click the day |
| Log today | Space |
| Previous / next month | Left / Right arrow, or the `<` `>` buttons |
| Jump back to today | `T`, or the Today button |
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

Until two cycles have been logged there's nothing to average, so it falls back
to a 28-day cycle and a 5-day period and labels the figure as an estimate.

These are averages of your own history, not a medical prediction — cycles shift
with stress, illness, travel and plenty else. Don't use the fertile window as
contraception.

## Layout

| File | What's in it |
| --- | --- |
| `src/main.cpp` | Window, main loop, keyboard and mouse handling |
| `src/tracker.h/.cpp` | The data: logging, cycle analysis, predictions, saving |
| `src/date.h/.cpp` | Date helpers built on C++20 `<chrono>` |
| `src/ui.h/.cpp` | Palette, font handling, calendar layout and drawing |

To add a source file, put it in the `add_executable(app ...)` list in
`CMakeLists.txt` and re-run the configure step.

## Dependencies

```sh
brew install cmake sdl2 sdl2_ttf
```
