#!/bin/bash
# Script to run the Monopoly server

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR"

# Build if needed
if [ ! -f "./build/server/monopoly_server" ]; then
    echo "Building server..."
    make server
fi

# Create logs directory
mkdir -p logs

# Generate log filename with timestamp
LOG_FILE="logs/server_$(date +%Y%m%d_%H%M%S).log"
echo "Logging to $LOG_FILE"

./build/server/monopoly_server -p 8888 -d monopoly.db "$@" 2>&1 | tee "$LOG_FILE"
