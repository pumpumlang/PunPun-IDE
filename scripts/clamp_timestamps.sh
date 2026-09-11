#!/usr/bin/env bash
# Pull future-dated source files back to now.
#
# ZIP entries store a local time with no timezone, and a source archive is
# often produced on a machine whose clock is ahead of the one unpacking it.
# Ninja then sees every source as permanently newer than the files CMake
# generated from it, so it re-runs CMake, regenerates, decides the result is
# still stale, and loops forever printing "Re-running CMake...".
#
# Sourced by run.sh and scripts/build_appimage.sh. Safe to run repeatedly: it
# only touches files dated in the future, and never reaches into build
# directories or .git.
clamp_future_timestamps() {
    local root="${1:-.}"
    local marker fixed=0
    marker="$(mktemp)"
    touch "$marker"

    while IFS= read -r -d '' file; do
        if [[ "$file" -nt "$marker" ]]; then
            touch -r "$marker" "$file"
            fixed=$((fixed + 1))
        fi
    done < <(find "$root" \
        \( -name build -o -name 'build-*' -o -name .git -o -name dist \) -prune -o \
        -type f -print0)

    rm -f "$marker"
    if (( fixed > 0 )); then
        echo "==> Repaired $fixed future-dated source file(s)"
    fi
}
