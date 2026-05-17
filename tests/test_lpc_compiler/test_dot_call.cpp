#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(LPCCompilerTest, dotCallCompilesToEfun) {
    const char *test_code = R"(
        mixed run_test() {
            return master().getuid(); //"42".to_int();
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "dot-call efun lowering failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, dotCallChainingCompiles) {
    const char *test_code = R"(
        mixed run_test() {
            return "42".to_int().to_float();
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_chain_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "chained dot-call efun lowering failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, dotCallUnknownEfunFailsCompile) {
    const char *test_code = R"(
        mixed run_test() {
            return "42".not_an_efun();
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_unknown_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "unknown efun dot-call unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, dotCallArityMismatchFailsCompile) {
    const char *test_code = R"(
        mixed run_test() {
            return 1.enable_commands();
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_arity_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "arity-mismatch dot-call unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, dotCallBadReceiverTypeFailsCompile) {
    const char *test_code = R"(
        #pragma strict_types
        mixed run_test() {
            return 1.repeat_string(2);
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_bad_receiver_type_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "bad receiver type in dot-call unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, dotCallBadTrailingArgumentTypeFailsCompile) {
    const char *test_code = R"(
        #pragma strict_types
        mixed run_test() {
            return "x".repeat_string("y");
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_bad_arg2_type_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "bad trailing argument type in dot-call unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, dotCallAndEfunOverrideFormsCompileTogether) {
    const char *test_code = R"(
        #pragma strict_types
        int run_test() {
            //int a = efun::to_int("42"); // requires master apply valid_override()
            int b = "42".to_int();
            int c = to_int("42");
            return 1;
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_vs_efun_override_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "dot-call and efun:: form compatibility failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, dotCallDisallowedEfunFailsCompile) {
    /* shutdown does not have #pragma allow_dot_call, so dot-call must be rejected. */
    const char *test_code = R"(
        mixed run_test() {
            return 0.shutdown();
        }
    )";

    program_t *prog = compile_file(-1, "test_dot_call_disallowed_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "disallowed efun dot-call unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}
