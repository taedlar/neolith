#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
extern "C" {
    #include "lpc/preprocess.h"
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
    EXPECT_NE(lookup_define ("M3_CONFIG_H"), nullptr); // from included m3_config.h
    end_new_file ();

    close(fd);
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, preprocessIf) {
    current_file = make_shared_string ("preprocess_if_test");
    current_file_id = 0;
    start_new_file (-1,
        "#if 1 + 1 == 2\n"
        "42\n"
        "#else\n"
        "0\n"
        "#endif\n"
    );
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 42);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, preprocessIfElifElse) {
    current_file = make_shared_string ("preprocess_if_elif_else_test");
    current_file_id = 0;
    start_new_file (-1,
        "#if 0\n"
        "0\n"
        "#elif 2 * 3 == 5\n"
        "1\n"
        "#elif 4 / 2 == 2\n"
        "2\n"
        "#else\n"
        "3\n"
        "#endif\n"
    );
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 2);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}

TEST_F(LPCLexerTest, preprocessNestedIf) {
    current_file = make_shared_string ("preprocess_nested_if_test");
    current_file_id = 0;
    start_new_file (-1,
        "#if 1\n"
        "10\n"
        "#if 2 > 1\n"
        "20\n"
        "#else\n"
        "30\n"
        "#endif\n"
        "#else\n"
        "40\n"
        "#endif\n"
    );
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 10);
    EXPECT_EQ(yylex(), L_NUMBER);
    EXPECT_EQ(yylval.number, 20);
    EXPECT_EQ(yylex(), -1); // EOF
    end_new_file ();
    free_string(current_file);
    current_file = 0;
}
