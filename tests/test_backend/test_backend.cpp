#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "std.h"
#include "rc.h"
#include "addr_resolver.h"
#include "lpc/compiler.h"

using namespace testing;

class BackendTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        debug_set_log_with_date (0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs
        MAIN_OPTION(pedantic) = 1; // enable pedantic mode for stricter checks

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
        init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__), CONFIG_STR (__INCLUDE_DIRS__));

        setup_simulate();
        eval_cost = CONFIG_INT (__MAX_EVAL_COST__); /* simulates calling LPC code from backend */
    }

    void TearDown() override {
        tear_down_simulate();
        deinit_lpc_compiler();
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
        deinit_config();
    }
};

namespace {

std::string ReadRepoSource(const std::filesystem::path &relative_path) {
    auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto full_path = root / relative_path;
    std::ifstream in(full_path);
    if (!in.is_open()) {
        return std::string();
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST_F(BackendTest, preload) {
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_master ("/master.c");
    // any error during preload_objects() will be caught.
    EXPECT_NO_THROW(preload_objects (0)) << "preload_objects() threw an exception";
    destruct_object(master_ob);
}

TEST_F(BackendTest, preloadRecoveryContinueContractSourceLocked) {
    const std::string source = ReadRepoSource("src/backend.c");
    ASSERT_FALSE(source.empty()) << "Unable to read src/backend.c";

    EXPECT_NE(source.find("/* in case of an error, effectively do a 'continue' */"), std::string::npos)
        << "Preload continue-on-error contract comment missing.";
    EXPECT_NE(source.find("if (setjmp (econ.context))"), std::string::npos)
        << "Preload error boundary missing setjmp guard.";
    EXPECT_NE(source.find("opt_warn (1, \"Error preloading file %d/%d, continuing.\", ix + 1, prefiles->size);"), std::string::npos)
        << "Preload warning/continue message changed unexpectedly.";
    EXPECT_NE(source.find("ix++;"), std::string::npos)
        << "Preload continue behavior requires index increment after caught error.";
    EXPECT_NE(source.find("for (; ix < prefiles->size; ix++)"), std::string::npos)
        << "Preload loop continuation contract changed unexpectedly.";
}

TEST_F(BackendTest, setHeartBeat) {
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_master ("/master.c");

    object_t* ob = master_ob;
    EXPECT_EQ(query_heart_beat(ob), 0); // master_ob has no heart beat initially

    // Enable heart beat
    EXPECT_EQ(set_heart_beat(ob, 1), 1);
    EXPECT_GT(query_heart_beat(ob), 0);

    // Disable heart beat
    EXPECT_EQ(set_heart_beat(ob, 0), 1);
    EXPECT_EQ(query_heart_beat(ob), 0);
}

TEST_F(BackendTest, resolverRuntimeConfigDefaults) {
    addr_resolver_config_t resolver_config;

    EXPECT_EQ(CONFIG_INT(__RESOLVER_FORWARD_CACHE_TTL__), 300);
    EXPECT_EQ(CONFIG_INT(__RESOLVER_REVERSE_CACHE_TTL__), 900);
    EXPECT_EQ(CONFIG_INT(__RESOLVER_NEGATIVE_CACHE_TTL__), 30);
    EXPECT_EQ(CONFIG_INT(__RESOLVER_STALE_REFRESH_WINDOW__), 30);

    stem_get_addr_resolver_config(&resolver_config);

    EXPECT_EQ(resolver_config.forward_cache_ttl, 300);
    EXPECT_EQ(resolver_config.reverse_cache_ttl, 900);
    EXPECT_EQ(resolver_config.negative_cache_ttl, 30);
    EXPECT_EQ(resolver_config.stale_refresh_window, 30);
}
