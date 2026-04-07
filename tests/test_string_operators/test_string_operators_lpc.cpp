#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

extern "C" {
#include "std.h"
#include "rc.h"
#include "interpret.h"
#include "lpc/compiler.h"
#include "lpc/object.h"
#include "lpc/operator.h"
#include "lpc/program.h"
#include "simulate.h"
#include "stralloc.h"
}

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

        init_master(CONFIG_STR(__MASTER_FILE__));
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

    void create_malloc_string(svalue_t *sv, const char *content, size_t len) {
        ASSERT_TRUE(sv != nullptr);
        sv->type = T_STRING;
        sv->subtype = STRING_MALLOC;
        sv->u.string = new_string(len, "test");
        ASSERT_TRUE(sv->u.string != nullptr);
        memcpy(sv->u.string, content, len);
        sv->u.string[len] = '\0';
    }

    void create_constant_string(svalue_t *sv, const char *content) {
        ASSERT_TRUE(sv != nullptr);
        sv->type = T_STRING;
        sv->subtype = STRING_CONSTANT;
        sv->u.const_string = content;
    }

    object_t *load_inline_object(const char *name, const char *code) {
        current_object = master_ob;
        object_t *obj = load_object(name, code);
        EXPECT_NE(obj, nullptr) << "Failed to load inline LPC object: " << name;
        return obj;
    }

    svalue_t call_noarg(object_t *obj, const char *method) {
        int index = 0;
        int fio = 0;
        int vio = 0;
        svalue_t ret;
        memset(&ret, 0, sizeof(ret));

        program_t *found_prog = find_function(obj->prog, findstring(method, NULL), &index, &fio, &vio);
        EXPECT_NE(found_prog, nullptr) << "find_function failed for method: " << method;

        if (found_prog) {
            int runtime_index = found_prog->function_table[index].runtime_index + fio;
            object_t* saved_current_object = current_object;
            current_object = obj;
            int saved_variable_index_offset = variable_index_offset;
            variable_index_offset = vio;
            call_function(obj->prog, runtime_index, 0, &ret);
            variable_index_offset = saved_variable_index_offset;
            current_object = saved_current_object;
        }
        return ret;
    }
};

TEST_F(StringOperatorsLPCTest, LpcConcatReturnsExpectedString) {
    const char *code =
        "string run_test() {\n"
        "  return \"Hello\" + \" \" + \"World\";\n"
        "}\n";

    object_t *obj = load_inline_object("test_lpc_concat.c", code);
    ASSERT_NE(obj, nullptr);

    svalue_t ret = call_noarg(obj, "run_test");
    ASSERT_EQ(ret.type, T_STRING);
    ASSERT_EQ(SVALUE_STRLEN(&ret), 11u);
    ASSERT_EQ(memcmp(ret.u.string, "Hello World", 11), 0);

    free_string_svalue(&ret);
    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, LpcEqNeOnConstantAndConcat) {
    const char *code =
        "int test_eq() {\n"
        "  return (\"ab\" == (\"a\" + \"b\"));\n"
        "}\n"
        "int test_ne() {\n"
        "  return (\"ab\" != \"abc\");\n"
        "}\n";

    object_t *obj = load_inline_object("test_lpc_eq_ne.c", code);
    ASSERT_NE(obj, nullptr);

    svalue_t eq_ret = call_noarg(obj, "test_eq");
    ASSERT_EQ(eq_ret.type, T_NUMBER);
    ASSERT_EQ(eq_ret.u.number, 1);

    svalue_t ne_ret = call_noarg(obj, "test_ne");
    ASSERT_EQ(ne_ret.type, T_NUMBER);
    ASSERT_EQ(ne_ret.u.number, 1);

    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, LpcRangeSlicesExpectedBytes) {
    const char *code =
        "string run_test() {\n"
        "  string s = \"0123456789\";\n"
        "  return s[2..5];\n"
        "}\n";

    object_t *obj = load_inline_object("test_lpc_range.c", code);
    ASSERT_NE(obj, nullptr);

    svalue_t ret = call_noarg(obj, "run_test");
    ASSERT_EQ(ret.type, T_STRING);
    ASSERT_EQ(SVALUE_STRLEN(&ret), 4u);
    ASSERT_EQ(memcmp(ret.u.string, "2345", 4), 0);

    free_string_svalue(&ret);
    destruct_object(obj);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocDifferentLengths) {
    svalue_t stack[2];

    create_constant_string(&stack[0], "ab");
    create_malloc_string(&stack[1], "abc", 3);
    sp = &stack[1];

    f_eq();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 0);

    create_constant_string(&stack[0], "ab");
    create_malloc_string(&stack[1], "abc", 3);
    sp = &stack[1];

    f_ne();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 1);
}

TEST_F(StringOperatorsLPCTest, EqNeMallocVsConstantDifferentLengths) {
    svalue_t stack[2];

    create_malloc_string(&stack[0], "abc", 3);
    create_constant_string(&stack[1], "ab");
    sp = &stack[1];

    f_eq();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 0);

    create_malloc_string(&stack[0], "abc", 3);
    create_constant_string(&stack[1], "ab");
    sp = &stack[1];

    f_ne();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 1);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocSameLength) {
    svalue_t stack[2];

    create_constant_string(&stack[0], "abc");
    create_malloc_string(&stack[1], "abc", 3);
    sp = &stack[1];

    f_eq();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 1);

    create_constant_string(&stack[0], "abc");
    create_malloc_string(&stack[1], "abc", 3);
    sp = &stack[1];

    f_ne();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 0);
}

TEST_F(StringOperatorsLPCTest, EqNeConstantVsMallocSameLengthDifferentBytes) {
    svalue_t stack[2];

    create_constant_string(&stack[0], "abc");
    create_malloc_string(&stack[1], "abd", 3);
    sp = &stack[1];

    f_eq();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 0);

    create_constant_string(&stack[0], "abc");
    create_malloc_string(&stack[1], "abd", 3);
    sp = &stack[1];

    f_ne();
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 1);
}
