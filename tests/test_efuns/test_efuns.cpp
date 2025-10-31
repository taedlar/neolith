#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(EfunsTest, throwError) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        SUCCEED(); // Expected error was thrown
    }
    else {
        // do efun throw() without catching it.
        push_constant_string("Error thrown by efun throw()");
        f_throw();
        FAIL() << "Efun throw() did not throw an LPC error as expected.";
    }
}

TEST_F(EfunsTest, stringCaseConversion) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during string case conversion efun tests.";
    }
    else {
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
}

TEST_F(EfunsTest, floatFunctions) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during float efuns tests.";
    }
    else {
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
}

TEST_F(EfunsTest, stringExplode) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during explode() efun tests.";
    }
    else {
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
}

