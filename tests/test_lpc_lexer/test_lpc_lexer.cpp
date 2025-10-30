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
        init_num_args();
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
    current_file = make_shared_string ("number_test");
    current_file_id = 0;
    // start_new_file (-1, "12345 0x1A3F 0755 3.14159 2.71828e10\n");
    start_new_file (-1, "12345 0x1A3F 3.14159\n");
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 12345);
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 0x1A3F);
    // EXPECT_EQ(yylex(), L_NUMBER);
    // EXPECT_EQ(yylval.number, 0755); // LPC does not support octal literals
    EXPECT_EQ(yylex(), L_REAL);
    EXPECT_EQ(yylval.real, 3.14159f);
    // EXPECT_EQ(yylex(), L_REAL);
    // EXPECT_EQ(yylval.real, 2.71828e10); // LPC does not support double precision literals
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}