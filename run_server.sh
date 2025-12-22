#!/bin/bash
# Script to run the Monopoly server

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR"

# Build if needed
if [ ! -f "./build/server/monopoly_server" ]; then
    echo "Building server..."
    make server
fi

./build/server/monopoly_server -p 8888 -d monopoly.db "$@"
