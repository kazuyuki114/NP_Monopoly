#!/bin/bash
# Script to run the Monopoly client with X11 video driver for Wayland compatibility

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR"

# Build if needed
if [ ! -f "./build/client/monopoly_client" ]; then
    echo "Building client..."
    make client
fi

# Set SDL_VIDEODRIVER to x11 for Wayland compatibility
SDL_VIDEODRIVER=x11 ./build/client/monopoly_client "${1:-127.0.0.1}" "${2:-8888}"
