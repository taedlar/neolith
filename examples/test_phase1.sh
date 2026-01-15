#!/bin/bash
# Test Phase 1 - POSIX Piped Stdin Support
#
# This script tests that piped input is preserved (not discarded by tcsetattr)

cd "$(dirname "$0")"

DRIVER="../out/build/linux/src/RelWithDebInfo/neolith"
CONFIG="m3.conf"

echo "================================"
echo "Phase 1: POSIX Piped Stdin Test"
echo "================================"
echo

if [ ! -f "$DRIVER" ]; then
    echo "❌ Driver not found: $DRIVER"
    echo "   Build first: cmake --build --preset ci-linux"
    exit 1
fi

echo "Test: Piped input should be preserved, not discarded"
echo "Commands: say Hello!, help, quit"
echo

# Use timeout to kill after 3 seconds (driver will wait for reconnect)
# Redirect stderr to filter out just the user interaction
OUTPUT=$(timeout 3 bash -c 'printf "say Hello from testbot!\nhelp\nquit\n" | '"$DRIVER"' -f '"$CONFIG"' -c 2>&1' | grep -A 15 "Welcome to the M3 Mud")

echo "$OUTPUT"
echo

# Check if all commands were processed
if echo "$OUTPUT" | grep -q "You say: Hello from testbot!" && echo "$OUTPUT" | grep -q "Available commands:" && echo "$OUTPUT" | grep -q "Bye!"; then
    echo "✅ SUCCESS: All piped commands were processed"
    echo "   - Input preserved through tcsetattr calls"
    echo "   - Phase 1 implementation working correctly"
    exit 0
else
    echo "❌ FAILURE: Commands were not processed"
    echo "   - Input may have been discarded"
    exit 1
fi
