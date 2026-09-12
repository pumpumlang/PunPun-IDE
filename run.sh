#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

REBUILD=0
RUN_TESTS=0
NO_LAUNCH=0
for arg in "$@"; do
  case "$arg" in
    --rebuild) REBUILD=1 ;;
    --test) RUN_TESTS=1 ;;
    --no-launch) NO_LAUNCH=1 ;;
    -h|--help)
      cat <<'HELP'
PunPun IDE native launcher
  ./run.sh              Build if needed, then launch immediately
  ./run.sh --rebuild    Force a clean rebuild
  ./run.sh --test       Run tests after building
  ./run.sh --no-launch  Build only
HELP
      exit 0
      ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

# Sources may arrive future-dated from a ZIP; that loops Ninja on CMake.
# shellcheck source=scripts/clamp_timestamps.sh
source "$ROOT/scripts/clamp_timestamps.sh"
clamp_future_timestamps "$ROOT"

# Fast path: once built, launch in effectively constant time. Do not invoke
# pacman, CMake, Ninja, or tests unless an input is newer than the executable.
if (( REBUILD == 0 )) && [[ -x build/punpun-ide && -x build/ppide-pp-bridge ]]; then
  if ! find CMakeLists.txt src c_api resources \
      -type f -newer build/punpun-ide -print -quit 2>/dev/null | grep -q .; then
    if (( RUN_TESTS == 1 )); then
      ctest --test-dir build --output-on-failure
    fi
    if (( NO_LAUNCH == 0 )); then
      exec ./build/punpun-ide
    fi
    exit 0
  fi
fi

if (( REBUILD == 1 )); then
  rm -rf build
fi

# Fast source preflight before the compiler avalanche. This catches the exact
# resource/header/API regressions that made the 0.4 series fail late in Ninja.
if command -v python3 >/dev/null 2>&1; then
  echo "==> Running PunPun IDE source preflight"
  python3 scripts/source_audit.py >/dev/null
fi

install_arch_dependencies() {
  local packages=(cmake ninja gcc clang gdb qt6-base qt6-svg qt6-declarative libarchive vulkan-headers)
  local missing=()
  for pkg in "${packages[@]}"; do
    pacman -Q "$pkg" >/dev/null 2>&1 || missing+=("$pkg")
  done
  if (( ${#missing[@]} > 0 )); then
    echo "==> Installing missing build dependencies: ${missing[*]}"
    sudo pacman -S --needed --noconfirm "${missing[@]}"
  fi
}

install_debian_dependencies() {
  local missing=0
  command -v cmake >/dev/null || missing=1
  command -v ninja >/dev/null || missing=1
  pkg-config --exists Qt6Widgets Qt6Network Qt6Svg Qt6Qml libarchive 2>/dev/null || missing=1
  if (( missing == 1 )); then
    echo "==> Installing missing build dependencies"
    sudo apt-get update
    sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev libqt6svg6-dev libarchive-dev libvulkan-dev
  fi
}

if command -v pacman >/dev/null 2>&1; then
  install_arch_dependencies
elif command -v apt-get >/dev/null 2>&1; then
  install_debian_dependencies
else
  echo "Unsupported automatic dependency manager." >&2
  echo "Install: CMake, Ninja, a C++20 compiler, Qt 6 Widgets/Network/Svg/Qml, and libarchive." >&2
  exit 1
fi

if [[ ! -f build/build.ninja ]]; then
  echo "==> Configuring PunPun IDE"
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
fi

echo "==> Building PunPun IDE"
if ! cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"; then
  echo
  echo "==> Parallel build failed. Re-running one job so the first real compiler error is readable." >&2
  cmake --build build --parallel 1
fi

if (( RUN_TESTS == 1 )); then
  echo "==> Running tests"
  ctest --test-dir build --output-on-failure
fi

if (( NO_LAUNCH == 0 )); then
  echo "==> Launching PunPun IDE"
  exec ./build/punpun-ide
fi
