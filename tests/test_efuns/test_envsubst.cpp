#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
  /* MSVC doesn't provide setenv()/unsetenv(); _putenv_s(name, "") removes the variable. */
  #define setenv(name, value, overwrite) _putenv_s((name), (value))
  #define unsetenv(name) _putenv_s((name), "")
#endif

/* Helper: unset a variable, ignoring any error. */
static void unset_var(const char *name)
{
  (void)unsetenv(name);
}


/* -------------------------------------------------------------------------
 * Plain strings (no substitution needed)
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstNoVariablesReturnsInputUnchanged) {
  push_constant_string("hello world");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "hello world");
  pop_stack();
}

TEST_F(EfunsTest, envsubstEmptyStringReturnsEmpty) {
  push_constant_string("");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "");
  pop_stack();
}

/* -------------------------------------------------------------------------
 * $VAR form
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstDollarVarExpandsWhenSet) {
  setenv("NEOLITH_TEST_VAR", "world", 1);
  push_constant_string("hello $NEOLITH_TEST_VAR");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "hello world");
  pop_stack();
  unset_var("NEOLITH_TEST_VAR");
}

TEST_F(EfunsTest, envsubstDollarVarExpandsToEmptyWhenUnset) {
  unset_var("NEOLITH_TEST_UNSET");
  push_constant_string("hello $NEOLITH_TEST_UNSET!");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "hello !");
  pop_stack();
}

TEST_F(EfunsTest, envsubstDollarVarStopsAtNonIdentChar) {
  setenv("NEOLITH_TEST_VAR", "bar", 1);
  push_constant_string("$NEOLITH_TEST_VAR.baz");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "bar.baz");
  pop_stack();
  unset_var("NEOLITH_TEST_VAR");
}

/* -------------------------------------------------------------------------
 * ${VAR} form
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstBraceVarExpandsWhenSet) {
  setenv("NEOLITH_TEST_VAR", "world", 1);
  push_constant_string("hello ${NEOLITH_TEST_VAR}");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "hello world");
  pop_stack();
  unset_var("NEOLITH_TEST_VAR");
}

TEST_F(EfunsTest, envsubstBraceVarExpandsToEmptyWhenUnset) {
  unset_var("NEOLITH_TEST_UNSET");
  push_constant_string("a${NEOLITH_TEST_UNSET}b");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "ab");
  pop_stack();
}

/* -------------------------------------------------------------------------
 * ${VAR:-default} form
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstDefaultUsedWhenUnset) {
  unset_var("NEOLITH_TEST_UNSET");
  push_constant_string("${NEOLITH_TEST_UNSET:-fallback}");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "fallback");
  pop_stack();
}

TEST_F(EfunsTest, envsubstDefaultUsedWhenEmpty) {
  setenv("NEOLITH_TEST_EMPTY", "", 1);
  push_constant_string("${NEOLITH_TEST_EMPTY:-fallback}");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "fallback");
  pop_stack();
  unset_var("NEOLITH_TEST_EMPTY");
}

TEST_F(EfunsTest, envsubstDefaultIgnoredWhenSetAndNonEmpty) {
  setenv("NEOLITH_TEST_VAR", "real", 1);
  push_constant_string("${NEOLITH_TEST_VAR:-fallback}");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "real");
  pop_stack();
  unset_var("NEOLITH_TEST_VAR");
}

TEST_F(EfunsTest, envsubstDefaultCanBeEmpty) {
  unset_var("NEOLITH_TEST_UNSET");
  push_constant_string("${NEOLITH_TEST_UNSET:-}");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "");
  pop_stack();
}

/* -------------------------------------------------------------------------
 * Multiple substitutions in one string
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstMultipleVariablesInOneString) {
  setenv("NEOLITH_HOST", "example.com", 1);
  setenv("NEOLITH_PORT", "8080", 1);
  push_constant_string("https://${NEOLITH_HOST}:${NEOLITH_PORT}/path");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "https://example.com:8080/path");
  pop_stack();
  unset_var("NEOLITH_HOST");
  unset_var("NEOLITH_PORT");
}

/* -------------------------------------------------------------------------
 * Edge cases: literal '$'
 * ---------------------------------------------------------------------- */

TEST_F(EfunsTest, envsubstDollarNotFollowedByNameEmittedLiterally) {
  push_constant_string("price: $5.00");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "price: $5.00");
  pop_stack();
}

TEST_F(EfunsTest, envsubstUnterminatedBraceEmitsDollarLiterally) {
  push_constant_string("${MISSING");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "${MISSING");
  pop_stack();
}

TEST_F(EfunsTest, envsubstTrailingDollarEmittedLiterally) {
  push_constant_string("end$");
  f_envsubst();

  auto view = lpc::svalue_view::from(sp);
  ASSERT_TRUE(view.is_string());
  EXPECT_STREQ(view.c_str(), "end$");
  pop_stack();
}
