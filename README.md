# InputRedirectionClient-Qt
Input redirection client for the 3DS using QtGamepad

Supported platforms:

* Windows (via xinput, if you don't have a Xbox controller you should use x360ce)
* Linux (via evdev)
* OSX
* maybe others?

If you have multiple controllers connected at the same time, this software will combine their inputs.

## Building

### Requirements
- Qt5.9 or later (for QGamepad support)
- C++ compiler with C++11 support

### Building with Qt5 (Recommended)

#### macOS
1. Install Qt5 via Homebrew:
   ```bash
   brew install qt@5
   ```

2. Build the project:
   ```bash
   ./build-qt5.sh
   ```
   
   Or manually:
   ```bash
   /usr/local/Cellar/qt@5/5.15.16_2/bin/qmake InputRedirectionClient-Qt.pro
   make
   ```

3. Run the application:
   ```bash
   ./InputRedirectionClient-Qt.app/Contents/MacOS/InputRedirectionClient-Qt
   ```

4. **For standalone distribution** (can be double-clicked from Finder):
   ```bash
   ./build-qt5.sh
   ```
   This automatically bundles Qt libraries using `macdeployqt` after building.

   Or manually:
   ```bash
   /usr/local/Cellar/qt@5/5.15.16_2/bin/macdeployqt InputRedirectionClient-Qt.app
   ```

**Note**: Without running `macdeployqt`, the app will only work when run from terminal because it depends on Qt5 libraries installed via Homebrew. The `macdeployqt` tool bundles these libraries with the app for standalone distribution.

#### Linux
1. Install Qt5 development packages:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install qt5-default qtbase5-dev qtgamepad5-dev
   
   # Fedora
   sudo dnf install qt5-qtbase-devel qt5-qtgamepad-devel
   ```

2. Build the project:
   ```bash
   qmake InputRedirectionClient-Qt.pro
   make
   ```

#### Windows
1. Install Qt5 from the official Qt website or via vcpkg
2. Open the project in Qt Creator or build from command line:
   ```bash
   qmake InputRedirectionClient-Qt.pro
   make
   ```

### Building with Qt6
The project can also be built with Qt6, but Qt5 is recommended for better compatibility.

## Timer Configuration

The Turbo VC Reset functionality includes configurable timer settings that can be adjusted through the UI:

- **VC Reset Interval (ms)**: Time between VC Reset button presses (default: 500ms)
- **Wait Time (ms)**: Wait time after VC Reset sequence before starting Turbo A (default: 5000ms)
- **Turbo A Duration (ms)**: Total duration of the Turbo A sequence (default: 13500ms)

These settings are automatically saved and restored between application sessions.

## Stop Functionality

The **"STOP TURBO VC RESET"** button allows you to immediately interrupt the Turbo VC Reset sequence at any point:

- **Instant stop**: Clicking the stop button immediately halts the sequence
- **Clean shutdown**: All timers are properly stopped and button states are reset
- **Any stage**: Works during VC Reset sequence, wait period, or Turbo A sequence
- **Debug output**: Console shows when the sequence is stopped

## Features
- Gamepad input redirection to 3DS
- Touch screen simulation
- Button remapping
- Turbo VC Reset functionality (triggers VC Reset 3 times with 0.5s intervals, waits 5s, then triggers A button every 0.25s for 13.5 seconds)
- **Configurable timer settings** - Adjust VC Reset interval, wait time, and Turbo A duration via UI
- **Stop button** - Immediately stop the Turbo VC Reset sequence at any time
- Y-axis inversion
- A/B and X/Y button inversion
