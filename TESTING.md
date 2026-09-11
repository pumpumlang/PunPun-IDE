# Testing PunPun IDE 0.5 on CachyOS

This is a native C++/Qt build. Do not create a Python venv.

```fish
./run.sh --test
```

The launcher installs only missing Arch dependencies, runs the source preflight, builds the native app, executes tests and launches it. After the first successful build, plain `./run.sh` should launch immediately when nothing changed.

## Smoke test

1. Open a normal project folder.
2. Right-click Explorer and create `hello.pp`.
3. Verify `.pp` gets PunPun highlighting and `str`/`String` look like types, not comments or fluorescent UI chrome.
4. Select text with the mouse. No mysterious full-width rectangle or hover card should remain onscreen.
5. Introduce a PunPun error. The editor should show a squiggle/gutter dot and the **Code Assistant** on the right should list it. Hover the squiggle to see the error card.
6. Press `Ctrl+Shift+B` to run an explicit compiler check.
7. Press `Shift+Alt+F` to format through PPC.
8. Press `F5` to run. The Terminal tab should open and accept stdin from interactive programs.
9. Create a `.cpp` file with a warning/error. Live compiler diagnostics should update from the unsaved buffer.
10. Open Tools > Environment Doctor and confirm the Code Assistant shows platform/cwd/runtime/stdlib/compiler environment information.
11. Open Tools > PunPun Toolchain and confirm the IDE discovers the current stable GitHub release and prefers the verified private PPC after installation.
12. Restart the IDE with `./run.sh`. With unchanged sources it should skip pacman/CMake/Ninja and launch directly.

## If a native build fails

The launcher retries one job after a parallel failure so the first compiler error is readable. Copy the first line containing `error:` plus roughly 15 lines around it. Warnings above it are usually just witnesses making noise.
