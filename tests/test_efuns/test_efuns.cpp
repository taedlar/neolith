#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

TEST_F(EfunsTest, throwError) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        debug_message("***** expected: caught error raised by efun throw()");
    }
    else {
        // do efun throw() without catching it.
        push_constant_string("Error thrown by efun throw()");
        f_throw();
        FAIL() << "Efun throw() did not throw an LPC error as expected.";
    }
    pop_context (&econ);
}

TEST_F(EfunsTest, stringCaseConversion) {
    // Test upper_case, lower_case, and capitalize efuns
    push_constant_string("Hello World!");

    f_upper_case();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "HELLO WORLD!");

    f_lower_case();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "hello world!");

    f_capitalize();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "Hello world!");
}

TEST_F(EfunsTest, floatFunctions) {
    push_constant_string("3.14159");
    f_to_float();
    ASSERT_EQ(sp->type, T_REAL);
    ASSERT_DOUBLE_EQ(sp->u.real, 3.14159);

    push_number(42);
    f_to_float();
    ASSERT_EQ(sp->type, T_REAL);
    ASSERT_DOUBLE_EQ(sp->u.real, 42.0);

    f_floatp();
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 1); // true for float

    pop_n_elems(1);
    push_constant_string("Not a float");
    f_floatp();
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 0); // false for not a float
}

TEST_F(EfunsTest, stringExplode) {
    // Test explode efun
    push_constant_string("Hello World!");
    push_constant_string(" ");
    f_explode();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ(sp->u.arr->size, 2);
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "Hello");
    ASSERT_STREQ(sp->u.arr->item[1].u.string, "World!");

    // Explode with empty delimiter string (Neolith extension: splits into wide characters)
    push_constant_string("こんにちは");
    push_constant_string("");
    f_explode();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ(sp->u.arr->size, 5);
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "こ");
    ASSERT_STREQ(sp->u.arr->item[1].u.string, "ん");
    ASSERT_STREQ(sp->u.arr->item[2].u.string, "に");
    ASSERT_STREQ(sp->u.arr->item[3].u.string, "ち");
    ASSERT_STREQ(sp->u.arr->item[4].u.string, "は");

    // Explode with multibyte delimiter (Neolith extension: does not break multibyte characters)
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0\x8F"); // "小" in UTF-8
    f_explode();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ(sp->u.arr->size, 2);
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "");
    ASSERT_STREQ(sp->u.arr->item[1].u.string, "星星");
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0"); // partial sequence of "小" in UTF-8
    f_explode();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ(sp->u.arr->size, 1);
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "小星星"); // delimiter not found, not even at character boundary
}

TEST_F(EfunsTest, stringExplodeUtf8) {
    // Test explode efun with UTF-8 multibyte characters
    // When delimiter is empty, explode should split into individual Unicode characters
    // Each character should be returned as a multibyte UTF-8 string
    
    // Test with mixed ASCII and UTF-8: "Hello世界" 
    // Using hex escapes to ensure proper encoding: Hello\xe4\xb8\x96\xe7\x95\x8c
    push_constant_string("Hello\xe4\xb8\x96\xe7\x95\x8c");
    push_constant_string("");
    f_explode();
    
    ASSERT_EQ(sp->type, T_ARRAY) << "Expected array result from explode";
    ASSERT_EQ(sp->u.arr->size, 7) << "Expected 7 characters: 5 ASCII + 2 UTF-8";
    
    // Verify ASCII characters (single bytes)
    ASSERT_EQ(sp->u.arr->item[0].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "H");
    ASSERT_EQ(sp->u.arr->item[1].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[1].u.string, "e");
    ASSERT_EQ(sp->u.arr->item[2].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[2].u.string, "l");
    ASSERT_EQ(sp->u.arr->item[3].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[3].u.string, "l");
    ASSERT_EQ(sp->u.arr->item[4].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[4].u.string, "o");
    
    // Verify UTF-8 multibyte characters (3 bytes each for Chinese characters)
    ASSERT_EQ(sp->u.arr->item[5].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[5].u.string, "\xe4\xb8\x96"); // '世'
    ASSERT_EQ(strlen(sp->u.arr->item[5].u.string), 3) << "UTF-8 '世' should be 3 bytes";
    
    ASSERT_EQ(sp->u.arr->item[6].type, T_STRING);
    ASSERT_STREQ(sp->u.arr->item[6].u.string, "\xe7\x95\x8c"); // '界'
    ASSERT_EQ(strlen(sp->u.arr->item[6].u.string), 3) << "UTF-8 '界' should be 3 bytes";
    
    pop_stack(); // Clean up the result array
    
    // Test with only UTF-8 characters: "日本語" (Japanese)
    push_constant_string("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    push_constant_string("");
    f_explode();
    
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ(sp->u.arr->size, 3) << "Expected 3 Japanese characters";
    
    ASSERT_STREQ(sp->u.arr->item[0].u.string, "\xe6\x97\xa5"); // '日'
    ASSERT_STREQ(sp->u.arr->item[1].u.string, "\xe6\x9c\xac"); // '本'
    ASSERT_STREQ(sp->u.arr->item[2].u.string, "\xe8\xaa\x9e"); // '語'
    
    pop_stack();
}
