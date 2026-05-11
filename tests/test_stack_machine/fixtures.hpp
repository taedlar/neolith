#pragma once

#include "std.h"
#include "rc.h"

#include <gtest/gtest.h>
#include <filesystem>

using namespace testing;

class StackMachineTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        // init testing environment
        namespace fs = std::filesystem;
        previous_cwd = fs::current_path();
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
        debug_set_log_with_date (false);
        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");

        // setup stem
        fs::path config_dir = fs::current_path();
        if (!fs::exists(config_dir / "m3.conf"))
            fs::current_path(config_dir.parent_path()); // change to parent if config not found in current dir
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs
        MAIN_OPTION(pedantic) = true; // enable pedantic mode for stricter checks

        // setup runtime (without LPC compiler since we are only testing stack machine execution, not compilation)
        init_config(MAIN_OPTION(config_file));
        init_strings (8192, 1000000); // LPC compiler needs this since prolog()
        setup_simulate();
    }

    void TearDown() override {
        namespace fs = std::filesystem;
        tear_down_simulate();
        deinit_strings();
        deinit_config();

        fs::current_path(previous_cwd);
    }
};
