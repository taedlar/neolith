#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "lpc/types.hpp"

namespace {

void ExpectArrayItemString(const array_t *arr, int index, const char *expected) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_string());
    ASSERT_STREQ(view.c_str(), expected);
}

void ExpectTopString(const char *expected) {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string());
    ASSERT_STREQ(view.c_str(), expected);
}

void ExpectTopNumber(int64_t expected) {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), expected);
}

void ExpectTopReal(double expected) {
    ASSERT_EQ(sp->type, T_REAL);
    ASSERT_DOUBLE_EQ(sp->u.real, expected);
}

} // namespace

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
    ExpectTopString("HELLO WORLD!");

    f_lower_case();
    ExpectTopString("hello world!");

    f_capitalize();
    ExpectTopString("Hello world!");
}

TEST_F(EfunsTest, floatFunctions) {
    push_constant_string("3.14159");
    f_to_float();
    ExpectTopReal(3.14159);

    push_number(42);
    f_to_float();
    ExpectTopReal(42.0);

    f_floatp();
    ExpectTopNumber(1); // true for float

    pop_n_elems(1);
    push_constant_string("Not a float");
    f_floatp();
    ExpectTopNumber(0); // false for not a float
}

TEST_F(EfunsTest, stringExplode) {
    // Test explode efun
    push_constant_string("Hello World!");
    push_constant_string(" ");
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 2);
    ExpectArrayItemString(sp->u.arr, 0, "Hello");
    ExpectArrayItemString(sp->u.arr, 1, "World!");

    // Explode with empty delimiter string (Neolith extension: splits into wide characters)
    push_constant_string("こんにちは");
    push_constant_string("");
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 5);
    ExpectArrayItemString(sp->u.arr, 0, "こ");
    ExpectArrayItemString(sp->u.arr, 1, "ん");
    ExpectArrayItemString(sp->u.arr, 2, "に");
    ExpectArrayItemString(sp->u.arr, 3, "ち");
    ExpectArrayItemString(sp->u.arr, 4, "は");

    // Explode with multibyte delimiter (Neolith extension: does not break multibyte characters)
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0\x8F"); // "小" in UTF-8
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 2);
    ExpectArrayItemString(sp->u.arr, 0, "");
    ExpectArrayItemString(sp->u.arr, 1, "星星");
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0"); // partial sequence of "小" in UTF-8
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 1);
    ExpectArrayItemString(sp->u.arr, 0, "小星星"); // delimiter not found, not even at character boundary
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
    
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array()) << "Expected array result from explode";
    ASSERT_EQ(sp->u.arr->size, 7) << "Expected 7 characters: 5 ASCII + 2 UTF-8";
    
    // Verify ASCII characters (single bytes)
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[0]).is_string());
    ExpectArrayItemString(sp->u.arr, 0, "H");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[1]).is_string());
    ExpectArrayItemString(sp->u.arr, 1, "e");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[2]).is_string());
    ExpectArrayItemString(sp->u.arr, 2, "l");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[3]).is_string());
    ExpectArrayItemString(sp->u.arr, 3, "l");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[4]).is_string());
    ExpectArrayItemString(sp->u.arr, 4, "o");
    
    // Verify UTF-8 multibyte characters (3 bytes each for Chinese characters)
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[5]).is_string());
    ASSERT_STREQ(lpc::svalue_view::from(&sp->u.arr->item[5]).c_str(), "\xe4\xb8\x96") << "Expected UTF-8 '世'";
    
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[6]).is_string());
    ASSERT_STREQ(lpc::svalue_view::from(&sp->u.arr->item[6]).c_str(), "\xe7\x95\x8c") << "Expected UTF-8 '界'";
    
    pop_stack(); // Clean up the result array
    
    // Test with only UTF-8 characters: "日本語" (Japanese)
    push_constant_string("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    push_constant_string("");
    f_explode();
    
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 3) << "Expected 3 Japanese characters";
    
    ExpectArrayItemString(sp->u.arr, 0, "\xe6\x97\xa5"); // '日'
    ExpectArrayItemString(sp->u.arr, 1, "\xe6\x9c\xac"); // '本'
    ExpectArrayItemString(sp->u.arr, 2, "\xe8\xaa\x9e"); // '語'
    
    pop_stack();
}
