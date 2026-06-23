#pragma once

#include "std.h"
#include "rc/rc.h"
#include "efuns_prototype.h"
#include "src/simul_efun.h"
#include "uids.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/otable.h"

#include <gtest/gtest.h>
#include <filesystem>

using namespace testing;

// according to GoogleTest FAQ, the test suite name and test name should not
// contain underscores to avoid issues on some platforms.
// https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

class EfunsTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        namespace fs = std::filesystem;
        debug_set_log_with_date (false);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling

        // setup stem
        previous_cwd = fs::current_path();
        fs::path config_dir = fs::current_path();
        if (!fs::exists(config_dir / "m3.conf"))
            fs::current_path (config_dir.parent_path()); // change to parent if config not found in current dir
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs
        MAIN_OPTION(pedantic) = true; // enable pedantic mode for stricter checks

        // setup runtime / simulate
        init_config(MAIN_OPTION(config_file));
        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");
        init_strings (8192, 1000000); // LPC compiler needs this since prolog()
        init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__), CONFIG_STR (__INCLUDE_DIRS__));
        setup_simulate();
    }

    void TearDown() override {
        namespace fs = std::filesystem;
        tear_down_simulate();
        deinit_lpc_compiler();
        deinit_strings();

        deinit_config();
        fs::current_path(previous_cwd);
    }
};
