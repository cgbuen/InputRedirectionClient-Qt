#!/bin/bash

# Build script for InputRedirectionClient-Qt using Qt5
# Auto-detect Qt5 path (supports Intel: /usr/local and Apple Silicon: /opt/homebrew)
if command -v brew >/dev/null 2>&1; then
    QT5_PATH="$(brew --prefix qt@5 2>/dev/null)"
fi

# Fallbacks if brew lookup failed
if [ -z "$QT5_PATH" ] || [ ! -d "$QT5_PATH" ]; then
    for CANDIDATE in \
        /opt/homebrew/Cellar/qt@5/* \
        /usr/local/Cellar/qt@5/* \
        /opt/homebrew/opt/qt@5 \
        /usr/local/opt/qt@5; do
        if [ -d "$CANDIDATE" ]; then
            QT5_PATH="$CANDIDATE"
            break
        fi
    done
fi

if [ -z "$QT5_PATH" ] || [ ! -d "$QT5_PATH" ]; then
    echo "Error: Qt5 not found. Please install with: brew install qt@5" >&2
    exit 1
fi

echo "Building InputRedirectionClient-Qt with Qt5..."
echo "Qt5 path: $QT5_PATH"

# Clean previous build
make clean 2>/dev/null

# Generate Makefile with Qt5 (prefer the opt symlink if QT5_PATH points to a Cellar version)
if [ -x "$QT5_PATH/bin/qmake" ]; then
    "$QT5_PATH/bin/qmake" InputRedirectionClient-Qt.pro
elif [ -x "/opt/homebrew/opt/qt@5/bin/qmake" ]; then
    "/opt/homebrew/opt/qt@5/bin/qmake" InputRedirectionClient-Qt.pro
elif [ -x "/usr/local/opt/qt@5/bin/qmake" ]; then
    "/usr/local/opt/qt@5/bin/qmake" InputRedirectionClient-Qt.pro
else
    echo "Error: qmake (Qt5) not found in $QT5_PATH. Ensure qt@5 is installed." >&2
    exit 1
fi

# Build the project
make

if [ $? -eq 0 ]; then
    echo "Build successful!"
    
    # Bundle Qt libraries for standalone distribution
    echo "Bundling Qt libraries..."
    if [ -x "$QT5_PATH/bin/macdeployqt" ]; then
        "$QT5_PATH/bin/macdeployqt" InputRedirectionClient-Qt.app
    elif [ -x "/opt/homebrew/opt/qt@5/bin/macdeployqt" ]; then
        "/opt/homebrew/opt/qt@5/bin/macdeployqt" InputRedirectionClient-Qt.app
    elif [ -x "/usr/local/opt/qt@5/bin/macdeployqt" ]; then
        "/usr/local/opt/qt@5/bin/macdeployqt" InputRedirectionClient-Qt.app
    else
        echo "Warning: macdeployqt not found. The app may not run standalone from Finder." >&2
    fi
    
    echo "Application is ready for distribution!"
    echo "Executable: ./InputRedirectionClient-Qt.app/Contents/MacOS/InputRedirectionClient-Qt"
    echo "App Bundle: ./InputRedirectionClient-Qt.app (can be double-clicked from Finder)"
else
    echo "Build failed!"
    exit 1
fi 