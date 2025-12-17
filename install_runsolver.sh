#!/bin/bash
set -e

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

echo "=== Installing RunSolver ==="

# 1. Install Dependencies
echo "[1/4] Checking dependencies..."
if command_exists apt-get; then
    echo "Detected apt package manager. Installing libnuma-dev..."
    # Check if we have sudo privileges or if we are root
    if [ "$EUID" -ne 0 ] && command_exists sudo; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq libnuma-dev build-essential
    elif [ "$EUID" -eq 0 ]; then
        apt-get update -qq
        apt-get install -y -qq libnuma-dev build-essential
    else
        echo "Warning: Cannot run apt-get as root. Please ensure 'libnuma-dev' is installed."
    fi
else
    echo "Warning: apt-get not found. Please ensure 'libnuma-dev' (or equivalent) is installed manually."
fi

# 2. Download
URL="https://www.cril.univ-artois.fr/~roussel/runsolver/runsolver-3.4.1.tar.bz2"
ARCHIVE="runsolver-3.4.1.tar.bz2"
TARGET_DIR="runsolver_pkg"

echo "[2/4] Downloading runsolver from $URL..."
if [ -d "$TARGET_DIR" ]; then
    echo "Directory $TARGET_DIR already exists. Cleaning up..."
    rm -rf "$TARGET_DIR"
fi

if command_exists wget; then
    wget -q --show-progress "$URL" -O "$ARCHIVE"
elif command_exists curl; then
    curl -L "$URL" -o "$ARCHIVE"
else
    echo "Error: Neither wget nor curl found."
    exit 1
fi

# 3. Extract
echo "[3/4] Extracting..."
mkdir -p "$TARGET_DIR"
tar -xjf "$ARCHIVE" -C "$TARGET_DIR" --strip-components=1
rm "$ARCHIVE"

# 4. Build
echo "[4/4] Building..."
cd "$TARGET_DIR/src"
make

echo ""
echo "=== Success! ==="
echo "Runsolver executable is located at: $(pwd)/runsolver"
echo ""
echo "To use it, you can add it to your PATH or symlink it:"
echo "  ln -sf $(pwd)/runsolver ../../runsolver"
