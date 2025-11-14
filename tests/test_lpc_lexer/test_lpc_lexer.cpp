#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <filesystem>
#include <gtest/gtest.h>

extern "C" {
    #include "std.h"
    #include "rc.h"
    #include "lpc/lex.h"
    #include "lpc/compiler.h"
    #include "grammar.h"
}

using namespace testing;

class LPCLexerTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        debug_set_log_with_date (0);
        setlocale(LC_ALL, "C.UTF-8"); // force UTF-8 locale for consistent string handling
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
        init_instrs();
        init_identifiers();
        set_inc_list (CONFIG_STR (__INCLUDE_DIRS__)); // automatically freed in deinit_lpc_compiler()

        // predefs are not added here; each test should add them as needed
    }

    void TearDown() override {
        deinit_identifiers();
        deinit_num_args();
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
        deinit_config();
    }
};

TEST_F(LPCLexerTest, getOpcodeName) {
    // LPC reserved words and operators are translated to instruction numbers (aka. opcodes)
    // before it can be interpreted by the LPC compiler.

    EXPECT_NO_THROW(query_opcode_name(0)); // not used

    // first eoperator is F_POP_VALUE = 1
    EXPECT_STREQ(query_opcode_name(F_LT), "<"); // eoperators

    // BASE is 114 (first efun)
    EXPECT_STREQ(query_opcode_name(F_CALL_OTHER), "call_other"); // from efuns

    // test out-of-bounds
    EXPECT_NO_THROW(query_opcode_name(-1));
    EXPECT_NO_THROW(query_opcode_name(NUM_OPCODES + 1));
    EXPECT_NO_THROW(query_opcode_name(9999));
}

TEST_F(LPCLexerTest, startNewFile) {
    int fd = open("master.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open include file master.c";
    current_file = make_shared_string ("master.c");
    current_file_id = 0;

    // run lexer until EOF
    start_new_file (fd, 0); // adds __FILE__ and __DIR__
    int n = 0;
    while (yylex() != -1) n++;
    debug_message("Lexed %d tokens from master.c", n);
    end_new_file ();

    close(fd);
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, handleInclude) {
    int fd = open("user.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open include file user.c";
    current_file = make_shared_string ("user.c");
    current_file_id = 0;

    // run lexer until EOF, which will process #include directives
    start_new_file (fd, 0);
    int n = 0;
    while (yylex() != -1) n++;
    debug_message("Lexed %d tokens from user.c", n);
    end_new_file ();

    close(fd);
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, parseNumber) {
    debug_message("Size of yylval: %zu", sizeof(yylval));
    current_file = make_shared_string ("number_test");
    current_file_id = 0;
    // start_new_file (-1, "12345 0x1A3F 0755 3.1415926 2.71828e10\n");
    start_new_file (-1, "12345 0x1A3F 3.1415926 2.71828e10d 4e+2f\n");
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 12345);
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 0x1A3F);
    // EXPECT_EQ(yylex(), L_NUMBER);
    // EXPECT_EQ(yylval.number, 0755); // LPC does not support octal literals
    EXPECT_EQ(yylex(), L_REAL);
    EXPECT_DOUBLE_EQ(yylval.real, 3.1415926);
    EXPECT_EQ(yylex(), L_REAL);
    EXPECT_DOUBLE_EQ(yylval.real, 2.71828e10);
    EXPECT_EQ(yylex(), L_REAL);
    EXPECT_DOUBLE_EQ(yylval.real, 400.0);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, parseStringLiteral) {
    current_file = make_shared_string ("string_test");
    current_file_id = 0;
    start_new_file (-1, "\"Hello world\" \"你好\" L\"こんにちは\"\n");
    EXPECT_EQ(yylex(), L_STRING);
    EXPECT_STREQ(yylval.string, "Hello world");
    EXPECT_EQ(yylex(), L_STRING);
    EXPECT_STREQ(yylval.string, "你好");
    EXPECT_EQ(yylex(), L_STRING);
    EXPECT_STREQ(yylval.string, "こんにちは"); // wide string literal (Neolith extension to LPC)
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, parseCharLiteral) {
    current_file = make_shared_string ("char_test");
    current_file_id = 0;
    start_new_file (-1, "'c' '\\n' '\\\\' L'は'\n");
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 'c');
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, '\n');
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, '\\');
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, L'は'); // wide character literal (Neolith extension to LPC)
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, skipComments) {
    current_file = make_shared_string ("comment_test");
    current_file_id = 0;
    start_new_file (-1, "// cxx comments\n/* c comments\n still work */\n\"Hello world\"\n");
    EXPECT_EQ(yylex(), L_STRING);
    EXPECT_STREQ(yylval.string, "Hello world");
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, parseReservedWords) {
    current_file = make_shared_string ("reserved_word_test");
    current_file_id = 0;
    start_new_file (-1, "for\nwhile\nif\nelse\nreturn\n");
    EXPECT_EQ(yylex(), L_FOR);
    EXPECT_EQ(yylex(), L_WHILE);
    EXPECT_EQ(yylex(), L_IF);
    EXPECT_EQ(yylex(), L_ELSE);
    EXPECT_EQ(yylex(), L_RETURN);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, parseEfuns) {
    current_file = make_shared_string ("efun_test");
    current_file_id = 0;
    start_new_file (-1, "call_other\nfoobar\n");
    EXPECT_EQ(yylex(), L_DEFINED_NAME);
    EXPECT_EQ(yylval.ihe->token & IHE_EFUN, IHE_EFUN);
    EXPECT_EQ(yylex(), L_IDENTIFIER);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}
