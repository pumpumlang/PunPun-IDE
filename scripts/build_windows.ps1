$ErrorActionPreference = "Stop"
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
& .\build\Release\punpun-ide.exe
