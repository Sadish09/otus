#!/bin/bash

echo "Installing otus..."

# Check deps
if ! command -v cmake >/dev/null; then
echo "cmake is required"
exit 1
fi

if ! command -v g++ >/dev/null; then
echo "g++ is required"
exit 1
fi

if [ "$EUID" -eq 0 ]; then
PREFIX="/usr/local"
else
PREFIX="$HOME/.local"
fi

echo "Installing to $PREFIX"

# Build
cmake -B build
cmake --build build

# Install
cmake --install build --prefix "$PREFIX"

echo "Make sure $PREFIX/bin is in your PATH"
