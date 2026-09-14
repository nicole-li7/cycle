#!/bin/bash
#
# Makes the .app self-contained.
#
# The executable is built against Homebrew's dylibs in /opt/homebrew, which
# means the app would break if Homebrew were removed, cleaned up, or if the app
# were ever copied to another Mac. This walks the dependency tree, copies every
# non-system library into the bundle, and rewrites the recorded paths to point
# inside it.
#
# Run automatically as a post-build step. See CMakeLists.txt.
set -euo pipefail

APP="$1"                      # .../Period Tracker.app
EXE="$APP/Contents/MacOS/$2"  # the executable inside it
FRAMEWORKS="$APP/Contents/Frameworks"

mkdir -p "$FRAMEWORKS"

# Copies every non-system dependency of $1 into Frameworks, recursively, and
# repoints $1 at the copies.
copy_deps() {
    local target="$1"
    local deps
    local lib
    local base
    deps=$(otool -L "$target" | tail -n +2 | awk '{print $1}')

    # `lib` and `base` MUST be local. Bash variables are dynamically scoped, so
    # without this the recursive call below overwrites the caller's loop
    # variable, and the install_name_tool at the end of the loop then rewrites
    # the wrong dependency - silently, and with a plausible-looking result.
    for lib in $deps; do
        case "$lib" in
            /usr/lib/*|/System/*|@*) continue ;;   # system, or already relative
        esac

        base=$(basename "$lib")

        if [ ! -f "$FRAMEWORKS/$base" ]; then
            cp -L "$lib" "$FRAMEWORKS/$base"
            chmod u+w "$FRAMEWORKS/$base"
            # A library refers to itself by its install name; make that relative
            # too, or anything linking it records the absolute path again.
            install_name_tool -id "@rpath/$base" "$FRAMEWORKS/$base"
            copy_deps "$FRAMEWORKS/$base"
        fi

        install_name_tool -change "$lib" "@rpath/$base" "$target"
    done
}

copy_deps "$EXE"

# sdl2-compat loads SDL3 with dlopen rather than linking it, so it never shows
# up in otool output and has to be copied by hand. It looks for the file next to
# itself (@loader_path/libSDL3.dylib), which is now inside Frameworks.
SDL3_SRC=$(otool -L "$FRAMEWORKS/libSDL2-2.0.0.dylib" >/dev/null 2>&1 \
           && ls /opt/homebrew/lib/libSDL3.dylib 2>/dev/null || true)
if [ -n "$SDL3_SRC" ] && [ ! -f "$FRAMEWORKS/libSDL3.dylib" ]; then
    cp -L "$SDL3_SRC" "$FRAMEWORKS/libSDL3.dylib"
    chmod u+w "$FRAMEWORKS/libSDL3.dylib"
    install_name_tool -id "@rpath/libSDL3.dylib" "$FRAMEWORKS/libSDL3.dylib"
    copy_deps "$FRAMEWORKS/libSDL3.dylib"
fi

# Rewriting a binary invalidates its signature, so everything is re-signed.
# This is an ad-hoc signature: enough for macOS to run it locally, not the same
# as a Developer ID signature for distributing to other people.
for lib in "$FRAMEWORKS"/*.dylib; do
    codesign --force --sign - --timestamp=none "$lib" >/dev/null 2>&1
done
codesign --force --sign - --timestamp=none "$APP" >/dev/null 2>&1

echo "bundled $(ls "$FRAMEWORKS" | wc -l | tr -d ' ') libraries into $(basename "$APP")"
