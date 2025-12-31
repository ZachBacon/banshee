#!/bin/bash
# Quick build and run script for Banshee Media Player

set -e  # Exit on error

echo "================================"
echo "Banshee Media Player - Build Script"
echo "================================"
echo ""

# Check for required dependencies
echo "Checking dependencies..."

check_dependency() {
    if ! pkg-config --exists "$1"; then
        echo "ERROR: $1 not found!"
        echo "Please install the required development packages."
        echo "See README.md for installation instructions."
        exit 1
    else
        VERSION=$(pkg-config --modversion "$1")
        echo "  ✓ $1 (version $VERSION)"
    fi
}

check_dependency "gtk+-3.0"
check_dependency "gstreamer-1.0"
check_dependency "glib-2.0"
check_dependency "sqlite3"

echo ""
echo "All dependencies found!"
echo ""

# Build the project
echo "Building Banshee..."
make clean
make

if [ $? -eq 0 ]; then
    echo ""
    echo "================================"
    echo "Build successful!"
    echo "================================"
    echo ""
    echo "To run the application:"
    echo "  ./build/banshee"
    echo ""
    echo "Or use: make run"
    echo ""
    
    # Ask if user wants to run
    read -p "Run Banshee now? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Starting Banshee Media Player..."
        ./build/banshee
    fi
else
    echo ""
    echo "Build failed! Check the errors above."
    exit 1
fi
