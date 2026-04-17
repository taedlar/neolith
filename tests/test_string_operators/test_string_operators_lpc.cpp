#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "interpret.h"
#include "lpc/compiler.h"
#include "lpc/object.h"
#include "lpc/operator.h"
#include "lpc/program.h"
#include "simulate.h"

#include <gtest/gtest.h>
#include <filesystem>

class StringOperatorsLPCTest : public ::testing::Test {
protected:
    std::filesystem::path previous_cwd_;
    svalue_t *saved_sp_ = nullptr;

    void SetUp() override {
        namespace fs = std::filesystem;

        debug_set_log_with_date(0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);

        fs::path config_dir = fs::current_path();
        if (!fs::exists(config_dir / "m3.conf")) {
            config_dir = fs::current_path().parent_path();
        }

        init_stem(3, (unsigned long)-1, (config_dir / "m3.conf").string().c_str());
        MAIN_OPTION(pedantic) = 1;
        init_config(MAIN_OPTION(config_file));
        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");

        ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));
        auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
        if (mudlib_path.is_relative()) {
            mudlib_path = config_dir / mudlib_path;
        }
        ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;
        previous_cwd_ = fs::current_path();
        fs::current_path(mudlib_path);

        init_strings(8192, 1000000);
        init_lpc_compiler(CONFIG_INT(__MAX_LOCAL_VARIABLES__), CONFIG_STR(__INCLUDE_DIRS__));
        setup_simulate();
        saved_sp_ = sp;

        init_master(CONFIG_STR(__MASTER_FILE__), NULL);
        ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";
    }

    void TearDown() override {
        namespace fs = std::filesystem;

        sp = saved_sp_;

        tear_down_simulate();
        deinit_lpc_compiler();
        deinit_strings();

        if (!previous_cwd_.empty()) {
            fs::current_path(previous_cwd_);
        }
        deinit_config();
    }

    object_t *load_inline_object(const char *name, const char *code) {
        current_object = master_ob;
        object_t *obj = load_object(name, code);
        EXPECT_NE(obj, nullptr) << "Failed to load inline LPC object: " << name;
        return obj;
    }

    lpc::svalue call_noarg(object_t *obj, const char *method) {
        int index = 0;
        int fio = 0;
        int vio = 0;
        lpc::svalue ret;

        program_t *found_prog = find_function(obj->prog, findstring(method, NULL), &index, &fio, &vio);
        EXPECT_NE(found_prog, nullptr) << "find_function failed for method: " << method;

        if (found_prog) {
            int runtime_index = found_prog->function_table[index].runtime_index + fio;
            object_t* saved_current_object = current_object;
            current_object = obj;
            int saved_variable_index_offset = variable_index_offset;
            variable_index_offset = vio;
            call_function(obj->prog, runtime_index, 0, ret.raw());
            variable_index_offset = saved_variable_index_offset;
            current_object = saved_current_object;
        }
        return ret;
    }
};

TEST_F(StringOperatorsLPCTest, LpcConcatReturnsExpectedString) {
    const char *code = R"(
        string run_test() {
          return "Hello" + " " + "World";
        }
    )";

    object_t *obj = load_inline_object("test_lpc_concat.c", code);
    ASSERT_NE(obj, nullptr);

    lpc::svalue ret = call_noarg(obj, "run_test");
    auto ret_view = ret.view();
    ASSERT_TRUE(ret_view.is_string());
    ASSERT_EQ(ret_view.length(), 11u);
    ASSERT_EQ(memcmp(ret_view.c_str(), "Hello World", 11), 0);

    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, LpcEqNeOnConstantAndConcat) {
    const char *code = R"(
        int test_eq() {
          return ("ab" == ("a" + "b"));
        }
        int test_ne() {
          return ("ab" != "abc");
        }
    )";

    object_t *obj = load_inline_object("test_lpc_eq_ne.c", code);
    ASSERT_NE(obj, nullptr);

    lpc::svalue eq_ret = call_noarg(obj, "test_eq");
    auto eq_ret_view = eq_ret.view();
    ASSERT_TRUE(eq_ret_view.is_number());
    ASSERT_EQ(eq_ret_view.number(), 1);

    lpc::svalue ne_ret = call_noarg(obj, "test_ne");
    auto ne_ret_view = ne_ret.view();
    ASSERT_TRUE(ne_ret_view.is_number());
    ASSERT_EQ(ne_ret_view.number(), 1);

    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, LpcRangeSlicesExpectedBytes) {
    const char *code = R"(
        string run_test() {
          string s = "0123456789";
          return s[2..5];
        }
    )";

    object_t *obj = load_inline_object("test_lpc_range.c", code);
    ASSERT_NE(obj, nullptr);

    lpc::svalue ret = call_noarg(obj, "run_test");
    auto ret_view = ret.view();
    ASSERT_TRUE(ret_view.is_string());
    ASSERT_EQ(ret_view.length(), 4u);
    ASSERT_EQ(memcmp(ret_view.c_str(), "2345", 4), 0);

    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocDifferentLengths) {
    svalue_t stack[2] = {};
    auto setup_operands = [&]() {
        free_svalue(&stack[0], "EqNeConstantVsMallocDifferentLengths");
        lpc::svalue_view::from(&stack[0]).set_constant_string("ab");
        free_svalue(&stack[1], "EqNeConstantVsMallocDifferentLengths");
        lpc::svalue_view::from(&stack[1]).set_malloc_string("abc");
        sp = &stack[1];
    };

    setup_operands();

    f_eq();
    lpc::svalue_view result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 0);

    setup_operands();

    f_ne();
    result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 1);
}

TEST_F(StringOperatorsLPCTest, EqNeMallocVsConstantDifferentLengths) {
    svalue_t stack[2] = {};
    auto setup_operands = [&]() {
        free_svalue(&stack[0], "EqNeMallocVsConstantDifferentLengths");
        lpc::svalue_view::from(&stack[0]).set_malloc_string("abc");
        free_svalue(&stack[1], "EqNeMallocVsConstantDifferentLengths");
        lpc::svalue_view::from(&stack[1]).set_constant_string("ab");
        sp = &stack[1];
    };

    setup_operands();

    f_eq();
    lpc::svalue_view result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 0);

    setup_operands();

    f_ne();
    result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 1);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocSameLength) {
    svalue_t stack[2] = {};
    auto setup_operands = [&]() {
        free_svalue(&stack[0], "EqNeConstantVsMallocSameLength");
        lpc::svalue_view::from(&stack[0]).set_constant_string("abc");
        free_svalue(&stack[1], "EqNeConstantVsMallocSameLength");
        lpc::svalue_view::from(&stack[1]).set_malloc_string("abc");
        sp = &stack[1];
    };

    setup_operands();

    f_eq();
    lpc::svalue_view result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 1);

    setup_operands();

    f_ne();
    result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 0);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocSameLengthDifferentBytes) {
    svalue_t stack[2] = {};
    auto setup_operands = [&]() {
        free_svalue(&stack[0], "EqNeConstantVsMallocSameLengthDifferentBytes");
        lpc::svalue_view::from(&stack[0]).set_constant_string("abc");
        free_svalue(&stack[1], "EqNeConstantVsMallocSameLengthDifferentBytes");
        lpc::svalue_view::from(&stack[1]).set_malloc_string("abd");
        sp = &stack[1];
    };

    setup_operands();

    f_eq();
    lpc::svalue_view result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 0);

    setup_operands();

    f_ne();
    result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 1);
}
