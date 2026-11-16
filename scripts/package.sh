#!/usr/bin/env bash
# Makes the macOS or Linux release archive from a build:
#
#   scripts/package.sh [build dir (build)] [output dir (dist)]
#
# Installs the "release" component into <output>/stage/<name> and checks it:
# - every program imports only system libraries (macOS: /usr/lib and
#   /System; Linux: the C/C++ runtime, OpenGL, and SDL2 and SDL2_image,
#   which the Linux download takes from the system), and no build paths;
# - macOS: the programs don't need a newer macOS than Info.plist says, and
#   the app is signed (ad hoc) and passes codesign's check;
# - Linux: prints the newest glibc and libstdc++ versions the programs need;
# - the staged engine-sim-export renders an idle clip from another folder;
# - the staged app is still running after 15 seconds (on Linux under Xvfb
#   when there is no display; skipped when neither is available);
# - those two runs, made on a copy, left the copy unchanged (on macOS the
#   app's folder is its signed bundle), and the app still verifies.
# Then writes <output>/<name>.zip (macOS, made with ditto) or .tar.gz (Linux).
# Used by .github/workflows/release.yml; Windows has package-windows.ps1.

set -euo pipefail

build=${1:-build}
out=${2:-dist}

fail() {
    echo "::error::$*" >&2
    exit 1
}
notice() {
    echo "::notice::$*"
}

[ -f "$build/CMakeCache.txt" ] || fail "No CMakeCache.txt in '$build'; configure and build first"
version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")
[ -n "$version" ] || fail "CMAKE_PROJECT_VERSION is not in $build/CMakeCache.txt"

os=$(uname -s)
case "$os" in
    Darwin) platform="macos-$(uname -m)" ;;
    Linux)
        case "$(uname -m)" in
            x86_64) platform=linux-x64 ;;
            *) platform="linux-$(uname -m)" ;;
        esac
        ;;
    *) fail "Unsupported system $os (Windows: scripts/package-windows.ps1)" ;;
esac

name="engine-sim-community-$version-$platform"
mkdir -p "$out"
out=$(cd "$out" && pwd)
stage_root="$out/stage"
stage="$stage_root/$name"
rm -rf "$stage_root"

echo "Installing $name"
cmake --install "$build" --component release --prefix "$stage" > "$out/install.log" \
    || { cat "$out/install.log"; fail "cmake --install failed"; }

if [ "$os" = Darwin ]; then
    app="$stage/Engine Sim.app"
    programs="$app/Contents/MacOS"
    [ -f "$app/Contents/Info.plist" ] || fail "The app has no Info.plist"
    minimum=$(/usr/libexec/PlistBuddy -c "Print :LSMinimumSystemVersion" "$app/Contents/Info.plist")
else
    programs="$stage"
fi
binaries=("$programs/engine-sim-app" "$programs/engine-sim-export")
for b in "${binaries[@]}"; do
    [ -x "$b" ] || fail "$b is missing"
done

# What each program imports
problems=()
if [ "$os" = Darwin ]; then
    for b in "${binaries[@]}"; do
        imports=$(otool -L "$b" | tail -n +2 | awk '{print $1}')
        notice "$(basename "$b") imports: $(echo $imports)"
        for lib in $imports; do
            case "$lib" in
                /usr/lib/*|/System/Library/*) ;;
                *) problems+=("$(basename "$b") imports $lib, which macOS doesn't have") ;;
            esac
        done
        rpaths=$(otool -l "$b" | awk '/LC_RPATH/ {getline; getline; print $2}')
        [ -z "$rpaths" ] || problems+=("$(basename "$b") has run paths: $(echo $rpaths)")
        minos=$(otool -l "$b" | awk '/LC_BUILD_VERSION/ {f = 1} f && $1 == "minos" {print $2; exit}')
        notice "$(basename "$b"): $(lipo -archs "$b"), needs macOS $minos (Info.plist says $minimum)"
        if [ "$(printf '%s\n%s\n' "$minos" "$minimum" | sort -V | tail -1)" != "$minimum" ]; then
            problems+=("$(basename "$b") needs macOS $minos, newer than $minimum")
        fi
    done
else
    allowed=" libSDL2-2.0.so.0 libSDL2_image-2.0.so.0 libGL.so.1 libOpenGL.so.0 libGLX.so.0 libstdc++.so.6 libm.so.6 libgcc_s.so.1 libc.so.6 libpthread.so.0 libdl.so.2 librt.so.1 ld-linux-x86-64.so.2 "
    for b in "${binaries[@]}"; do
        needed=$(readelf -d "$b" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
        notice "$(basename "$b") needs: $(echo $needed)"
        for lib in $needed; do
            case "$allowed" in
                *" $lib "*) ;;
                *) problems+=("$(basename "$b") needs $lib, which isn't part of the expected system libraries") ;;
            esac
        done
        rpath=$(readelf -d "$b" | sed -n 's/.*(R\(UN\)\{0,1\}PATH).*\[\(.*\)\]/\2/p')
        [ -z "$rpath" ] || problems+=("$(basename "$b") has a run path: $rpath")
        glibc=$(objdump -T "$b" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
        glibcxx=$(objdump -T "$b" | grep -o 'GLIBCXX_[0-9.]*' | sort -uV | tail -1)
        notice "$(basename "$b") needs $glibc and $glibcxx"
    done
fi
if [ ${#problems[@]} -gt 0 ]; then
    for p in "${problems[@]}"; do echo "::error::$p" >&2; done
    exit 1
fi

if [ "$os" = Darwin ]; then
    # Installing rewrote the programs, which breaks the linker's signature;
    # Apple Silicon only runs signed code, so sign the whole app again
    codesign --force --deep --sign - "$app"
    codesign --verify --deep --strict --verbose=2 "$app" || fail "The signed app doesn't verify"
fi

# The tests run a copy, so nothing they do ends up in the archive. Afterwards
# the copy must still match: the programs mustn't write into their own
# folder, which on macOS is the signed app.
test_root=$(mktemp -d)
if [ "$os" = Darwin ]; then
    ditto "$stage" "$test_root/$name"
    test_programs="$test_root/$name/Engine Sim.app/Contents/MacOS"
else
    cp -a "$stage" "$test_root/$name"
    test_programs="$test_root/$name"
fi

# The exporter, started from another folder with no library paths set, finds
# its scripts on its own
smoke=$(mktemp -d)
(
    cd "$smoke"
    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH -u ENGINE_SIM_DATA_ROOT \
        "$test_programs/engine-sim-export" --clips idle --out "$smoke/out"
) || fail "engine-sim-export failed in the staged folder"
clips=$(find "$smoke/out" -type f \( -name '*.wav' -o -name '*.ogg' -o -name '*.mp3' \) -size +1k)
echo "$clips" | grep -q '\.wav$' || fail "The smoke test wrote no WAV clip"
formats=$(echo "$clips" | sed 's/.*\.//' | sort -u | tr '\n' ' ')
notice "Smoke test wrote $(echo "$clips" | wc -l | tr -d ' ') clips ($formats)"
rm -rf "$smoke"

# The app starts and keeps running. Without a display on Linux, Xvfb stands
# in (with Mesa's software OpenGL); the dummy audio driver stands in for a
# sound card.
xvfb_pid=
app_display=${DISPLAY:-}
if [ "$os" = Linux ] && [ -z "$app_display" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    if command -v Xvfb > /dev/null; then
        Xvfb :97 -screen 0 1920x1080x24 > /dev/null 2>&1 &
        xvfb_pid=$!
        app_display=:97
        sleep 2
    fi
fi
if [ "$os" = Darwin ] || [ -n "$app_display" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    app_log="$out/app-start.log"
    app_home=$(mktemp -d)
    (
        cd "$app_home"
        exec env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH -u ENGINE_SIM_DATA_ROOT \
            HOME="$app_home" DISPLAY="$app_display" SDL_AUDIODRIVER=dummy \
            "$test_programs/engine-sim-app" > "$app_log" 2>&1
    ) &
    app_pid=$!
    sleep 15
    if kill -0 "$app_pid" 2> /dev/null; then
        kill "$app_pid" 2> /dev/null || true
        wait "$app_pid" 2> /dev/null || true
        notice "The staged app was still running after 15 seconds"
    else
        status=0
        wait "$app_pid" || status=$?
        cat "$app_log"
        [ -z "$xvfb_pid" ] || kill "$xvfb_pid" 2> /dev/null || true
        fail "The staged app exited after less than 15 seconds (status $status)"
    fi
    rm -rf "$app_home"
else
    notice "No display and no Xvfb: skipped starting the app"
fi
[ -z "$xvfb_pid" ] || kill "$xvfb_pid" 2> /dev/null || true

changed=$(diff -rq "$stage" "$test_root/$name" 2>&1 || true)
if [ -n "$changed" ]; then
    echo "$changed"
    fail "Running the programs changed files in their own folder"
fi
rm -rf "$test_root"

if [ "$os" = Darwin ]; then
    codesign --verify --deep --strict "$app" || fail "The app's signature no longer verifies"
    archive="$out/$name.zip"
    rm -f "$archive"
    # ditto keeps the app's signature and attributes intact
    ditto -c -k --keepParent "$stage" "$archive"
else
    archive="$out/$name.tar.gz"
    rm -f "$archive"
    tar -C "$stage_root" -czf "$archive" "$name"
fi

if command -v sha256sum > /dev/null; then
    sha256sum "$archive" | sed "s|$out/||"
else
    shasum -a 256 "$archive" | sed "s|$out/||"
fi

if [ -n "${GITHUB_OUTPUT:-}" ]; then
    {
        echo "version=$version"
        echo "archive=$archive"
        echo "stage=$stage"
    } >> "$GITHUB_OUTPUT"
fi
