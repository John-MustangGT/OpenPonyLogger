# Contributing to OpenPonyLogger

Thanks for helping improve OpenPonyLogger! This guide covers local setup, build/run steps, branching/PR workflow, commit messages, and repo hygiene.

## Getting Started
- Required tools: PlatformIO CLI (or VS Code + PlatformIO extension), Git.
- Target board: ESP32-S3 Feather (esp32s3dev env). ESP32 Dev (esp32dev) may also build.
- Optional hardware: ST7789 TFT, GPS (PA1010D), IMU (ICM20948), battery monitor (MAX17048), BLE OBD-II dongle.

## Build & Run
- Install dependencies via PlatformIO automatically from `platformio.ini`.
- Common commands:
  - Build (ESP32-S3):
    ```bash
    pio run -e esp32s3dev
    ```
  - Upload (ESP32-S3):
    ```bash
    pio run -e esp32s3dev --target upload --upload-port /dev/cu.usbmodemXXXX
    ```
  - Monitor (ESP32-S3):
    ```bash
    pio device monitor -b 115200 --port /dev/cu.usbmodemXXXX
    ```
  - Clean:
    ```bash
    pio run -e esp32s3dev --target clean
    ```

## Branching & Pull Requests
- Create feature branches from `main` (e.g., `feat/ble-obd`, `fix/display-artifacts`).
- Open PRs targeting `main`. Keep changes focused and include a brief summary of rationale and testing.
- Prefer small, reviewable commits. Squash on merge if appropriate.

## Commit Messages (Conventional Commits)
- Use concise, descriptive messages:
  - `feat(display): add label manager to prevent ghosting`
  - `fix(ble): match IOS-Vlink advertised name during scan`
  - `chore(git): ignore .pio/.vscode and prune`
- Types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`, `perf`.

## Code Style & Structure
- C++ (Arduino) with consistent naming and minimal inline comments.
- Keep changes minimal and focused; avoid unrelated refactors.
- Place modules under `lib/` (Drivers, Display, Logger, SensorHAL, WiFi). App entry in `src/main.cpp`.
- Follow existing patterns for drivers and helpers.

## Repo Hygiene (Do Not Commit)
- Build outputs and local editor settings:
  - `.pio/` (PlatformIO build outputs, libdeps)
  - `.vscode/` (editor settings)
  - `.DS_Store` (macOS)
  - `_codeql_detected_source_root/` (generated symlink)
- Keep `.gitignore` up to date to ensure these remain untracked.

## Testing & Verification
- Run `pio run -e esp32s3dev` locally before opening a PR.
- If hardware-dependent, include a short note on how you validated functionality (e.g., BLE scan shows `IOS-Vlink`, display renders cleanly).

## Troubleshooting
- Push rejected (non-fast-forward):
  ```bash
  git pull --rebase origin main
  # resolve conflicts
  git add -A && git rebase --continue
  git push origin main
  ```
- Unwanted files tracked: update `.gitignore` and prune:
  ```bash
  git rm -r --cached .pio .vscode _codeql_detected_source_root
  git commit -m "chore(git): prune build/editor artifacts"
  git push origin main
  ```

Thank you for contributing!