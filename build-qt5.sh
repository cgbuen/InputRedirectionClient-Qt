#!/bin/bash

# Build script for InputRedirectionClient-Qt using Qt5
QT5_PATH="/usr/local/Cellar/qt@5/5.15.16_2"

echo "Building InputRedirectionClient-Qt with Qt5..."
echo "Qt5 path: $QT5_PATH"

# Clean previous build
make clean 2>/dev/null

# Generate Makefile with Qt5
$QT5_PATH/bin/qmake InputRedirectionClient-Qt.pro

# Build the project
make

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Executable: ./InputRedirectionClient-Qt.app/Contents/MacOS/InputRedirectionClient-Qt"
else
    echo "Build failed!"
    exit 1
fi 