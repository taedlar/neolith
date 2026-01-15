#!/usr/bin/env python3
"""
Testbot - Automated console mode testing robot

This testing robot demonstrates how to test the Neolith driver in console mode by 
piping commands to stdin and validating output. Use this as a template for creating 
more advanced test automation scripts.

Platform support:
- Linux/WSL: Fully supported (pipes work with poll())
- Windows: Not yet supported - requires piped stdin enhancement
  See docs/plan/windows-piped-stdin-support.md for implementation plan

Usage:
    cd examples
    python testbot.py
"""

import subprocess
import sys
import time
import os
from pathlib import Path

def test_console_mode():
    """Test the driver in console mode with automated input (Linux/WSL only)
    
    This is a template demonstrating basic automated testing. Extend this for:
    - Testing specific LPC functionality
    - Regression testing after code changes
    - Performance benchmarking
    - Stress testing with multiple concurrent operations
    """
    
    # Determine paths (relative to examples/ directory)
    if os.name == 'nt':  # Windows
        driver_path = Path("../out/build/vs16-x64/src/RelWithDebInfo/neolith.exe")
    else:  # Linux/WSL
        driver_path = Path("../out/build/linux/src/RelWithDebInfo/neolith")
    
    config_path = Path("m3.local.conf")
    
    # Verify files exist
    if not driver_path.exists():
        print(f"❌ Driver not found: {driver_path}")
        print("   Build the driver first with: cmake --build --preset ci-vs16-x64")
        return 1
    
    if not config_path.exists():
        print(f"❌ Config not found: {config_path}")
        return 1
    
    print(f"✓ Using driver: {driver_path}")
    print(f"✓ Using config: {config_path}")
    print()
    
    # Prepare test commands
    test_commands = [
        "wizard",          # Username
        "wizard",          # Password
        "say Hello from Python test!",
        "help",
        "quit"
    ]
    
    # Join commands with newlines and encode
    input_data = "\n".join(test_commands) + "\n"
    
    print("=" * 60)
    print("CONSOLE MODE AUTOMATED TEST")
    print("=" * 60)
    
    if os.name == 'nt':
        print("⚠️  WARNING: This test will FAIL on Windows!")
        print("   Windows console mode requires a real console, not piped stdin.")
        print("   Run the driver manually: ..\\out\\build\\vs16-x64\\src\\RelWithDebInfo\\neolith.exe -f m3.local.conf -c")
        print()
    
    print("Input commands:")
    for i, cmd in enumerate(test_commands, 1):
        print(f"  {i}. {cmd}")
    print()
    
    try:
        # Start the driver process
        process = subprocess.Popen(
            [str(driver_path), "-f", str(config_path), "-c"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,  # Line buffered
            encoding='utf-8',
            errors='replace'
        )
        
        print("Driver started. Sending commands...")
        print("-" * 60)
        
        # Send input to the driver
        try:
            stdout_data, _ = process.communicate(input=input_data, timeout=10)
            print(stdout_data)
        except subprocess.TimeoutExpired:
            print("\n⚠️  Process timeout - killing driver")
            process.kill()
            stdout_data, _ = process.communicate()
            print(stdout_data)
            return 1
        
        print("-" * 60)
        
        # Check exit code
        if process.returncode == 0:
            print("✅ TEST PASSED - Driver exited successfully")
            return 0
        else:
            print(f"❌ TEST FAILED - Driver exited with code {process.returncode}")
            return 1
            
    except FileNotFoundError:
        print(f"❌ Could not execute: {driver_path}")
        return 1
    except Exception as e:
        print(f"❌ Error during test: {e}")
        return 1
    finally:
        # Cleanup log file if it exists
        log_file = Path("m3_debug.log")
        if log_file.exists():
            print(f"\n📝 Debug log created: {log_file}")

if __name__ == "__main__":
    sys.exit(test_console_mode())
