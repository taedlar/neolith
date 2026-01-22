#!/usr/bin/env python3
"""
Testbot - Automated console mode testing robot

This testing robot demonstrates how to test the Neolith driver in console mode by 
piping commands to stdin and validating output. Use this as a template for creating 
more advanced test automation scripts.

Platform support:
- Linux/WSL: ✅ Phase 1 complete - pipes work correctly
- Windows: ✅ Phase 2 complete - pipes work correctly
  See docs/manual/console-testbot-support.md for design details

How it works:
- Uses pexpect.PopenSpawn for robust interactive process control
- Sends commands line-by-line with interactive pattern matching
- Driver detects pipe (not real console) and preserves all input data
- Commands are processed until EOF
- Driver automatically shuts down on pipe closure
- Test validates clean exit with code 0

Usage:
    cd examples
    python testbot.py [driver arguments...]
    
    Examples:
        python testbot.py              # Basic test with default config
        python testbot.py -r5          # Test with max recursion depth 5
        python testbot.py --debug      # Test with debug flags

    Any arguments after the script name are passed directly to the driver.

Requirements:
    pip install pexpect
"""

import sys
import os
from pathlib import Path

try:
    import pexpect
    from pexpect.popen_spawn import PopenSpawn
except ImportError:
    print("❌ pexpect module not found")
    print("   Install it with: pip install pexpect")
    sys.exit(1)

def test_console_mode():
    """Test the driver in console mode with automated input
    
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
    
    config_path = Path("m3.conf")
    
    # Verify files exist
    if not driver_path.exists():
        print(f"❌ Driver not found: {driver_path}")
        print("   Build the driver first with: cmake --build --preset ci-linux")
        return 1
    
    if not config_path.exists():
        print(f"❌ Config not found: {config_path}")
        return 1
    
    print(f"✓ Using driver: {driver_path}")
    print(f"✓ Using config: {config_path}")
    
    # Get additional command line arguments to pass to driver
    extra_args = sys.argv[1:]
    if extra_args:
        print(f"✓ Extra driver args: {' '.join(extra_args)}")
    print()
    
    print("=" * 60)
    print("CONSOLE MODE AUTOMATED TEST")
    print("=" * 60)
    print(f"Platform: {os.name}")
    print()
       
    child = None
    try:
        # Start the driver process using pexpect PopenSpawn
        # PopenSpawn uses subprocess.Popen internally but provides better interactive control
        command = [str(driver_path), "-f", str(config_path), "-c"]
        
        # Append any additional command line arguments
        if extra_args:
            command.extend(extra_args)
        
        print(f"Command: {' '.join(command)}")
        print()
        
        child = PopenSpawn(command, timeout=10, encoding='utf-8', codec_errors='replace')
        child.logfile_read = sys.stdout  # Log all output to stdout
        
        print("Driver started. Sending commands and verifying output...")
        print("-" * 60)
        
        # Wait for initial prompt
        child.expect('Type .help. for available commands.', timeout=5)
        print("✓ Initial prompt received")
        
        # Test 1: Send "say" command and verify output
        print("\nTest 1: Sending 'say Hello from Python test!'")
        child.sendline("say Hello from Python test!")
        child.expect('You say: Hello from Python test!', timeout=5)
        print("✓ Say command verified")
        
        # Test 2: Send "help" command and verify output
        print("\nTest 2: Sending 'help'")
        child.sendline("help")
        child.expect('Available commands:', timeout=5)
        child.expect('shutdown', timeout=5)  # Verify shutdown command is listed
        print("✓ Help command verified")
        
        # Test 3: Send "shutdown" command
        print("\nTest 3: Sending 'shutdown'")
        child.sendline("shutdown")
        child.expect('Shutting down...', timeout=5)
        print("✓ Shutdown command verified")
        
        # Wait for process to exit
        child.expect(pexpect.EOF, timeout=5)
        # captured output until last expect() in child.before
        print("-" * 60)

        # Wait for child process to fully exit
        child.wait()
        exit_code = child.exitstatus
        
        # Check exit code
        if exit_code == 0:
            print("\n✅ ALL TESTS PASSED - Driver exited successfully")
            return 0
        else:
            print(f"\n❌ TEST FAILED - Driver exited with code {exit_code}")
            return 1
            
    except pexpect.TIMEOUT:
        print("\n❌ TEST FAILED - Process timed out waiting for expected output")
        if child:
            print("\nLast output before timeout:")
            print(child.before)
            child.kill(9)
            try:
                child.wait()
            except:
                pass
        return 1
    except pexpect.EOF:
        print("\n❌ TEST FAILED - Unexpected EOF from driver")
        if child:
            print("\nOutput before EOF:")
            print(child.before)
        return 1
    except FileNotFoundError:
        print(f"❌ Could not execute: {driver_path}")
        return 1
    except Exception as e:
        print(f"❌ Error during test: {e}")
        if child and child.isalive():
            child.kill(9)
            try:
                child.wait()
            except:
                pass
        return 1
    finally:
        # Cleanup log file if it exists
        log_file = Path("m3_debug.log")
        if log_file.exists():
            print(f"\n📝 Debug log created: {log_file}")

if __name__ == "__main__":
    sys.exit(test_console_mode())
