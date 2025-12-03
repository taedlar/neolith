#pragma once

#include <gtest/gtest.h>
#include <filesystem>

extern "C" {
    #include "std.h"
    #include "rc.h"
}

using namespace testing;

class StackMachineTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        debug_set_log_with_date (0);
        setlocale(LC_ALL, ".UTF-8"); // force UTF-8 locale for consistent string handling
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs

        init_config(MAIN_OPTION(config_file));

        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");
        ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));
        namespace fs = std::filesystem;
        auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__)); // absolute or relative to cwd
        if (mudlib_path.is_relative()) {
            mudlib_path = fs::current_path() / mudlib_path;
        }
        ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;
        previous_cwd = fs::current_path();
        fs::current_path(mudlib_path); // change working directory to mudlib

        init_strings (8192, 1000000); // LPC compiler needs this since prolog()
        // init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__));
        // set_inc_list (CONFIG_STR (__INCLUDE_DIRS__)); // automatically freed in deinit_lpc_compiler()

        init_simulate();
        eval_cost = CONFIG_INT (__MAX_EVAL_COST__); /* simulates calling LPC code from backend */
    }

    void TearDown() override {
        tear_down_simulate();
        // deinit_lpc_compiler();
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
        deinit_config();
    }
};
