#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtest/gtest.h>
#include <filesystem>

#include "std.h"
#include "rc.h"
#include "command.h"
#include "src/apply.h"
#include "lpc/include/origin.h"
#include "lpc/compiler.h"
#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/types.h"

using namespace testing;

namespace {

class ProcessCommandTest : public Test {
private:
  std::filesystem::path previous_cwd;

protected:
  void SetUp() override {
    debug_set_log_with_date(0);
    setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);
    init_stem(3, (unsigned long)-1, "m3.conf");
    MAIN_OPTION(pedantic) = 1;

    init_config(MAIN_OPTION(config_file));

    ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));
    namespace fs = std::filesystem;
    auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
    if (mudlib_path.is_relative()) {
      mudlib_path = fs::current_path() / mudlib_path;
    }
    ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;

    previous_cwd = fs::current_path();
    fs::current_path(mudlib_path);

    init_strings(8192, 1000000);
    init_lpc_compiler(CONFIG_INT(__MAX_LOCAL_VARIABLES__), CONFIG_STR(__INCLUDE_DIRS__));

    setup_simulate();
    eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

    init_master("/master.c", NULL);
    ASSERT_NE(master_ob, nullptr);
  }

  void TearDown() override {
    if (master_ob && !(master_ob->flags & O_DESTRUCTED)) {
      destruct_object(master_ob);
    }

    tear_down_simulate();
    deinit_lpc_compiler();
    deinit_strings();

    namespace fs = std::filesystem;
    fs::current_path(previous_cwd);
    deinit_config();
  }

  object_t *load_command_object(const char *name) {
    constexpr const char *kLpcCode = R"(
      string last_handler;
      string last_verb_seen;
      string last_arg_seen;
      int call_count;

      void register_actions() {
        add_action("act_multi", "look at", 0);
        add_action("act_short", "say", 1);
        add_action("act_xverb", "pre", 2);
      }

      int act_multi(string arg) {
        last_handler = "multi";
        last_verb_seen = query_verb();
        last_arg_seen = arg;
        call_count++;
        return 1;
      }

      int act_short(string arg) {
        last_handler = "short";
        last_verb_seen = query_verb();
        last_arg_seen = arg;
        call_count++;
        return 1;
      }

      int act_xverb(string arg) {
        last_handler = "xverb";
        last_verb_seen = query_verb();
        last_arg_seen = arg;
        call_count++;
        return 1;
      }

      mixed *state() {
        return ({ last_handler, last_verb_seen, last_arg_seen, call_count });
      }
    )";

    current_object = master_ob;
    object_t *obj = load_object(name, kLpcCode);
    EXPECT_NE(obj, nullptr);
    return obj;
  }

  void register_actions(object_t *obj) {
    ASSERT_NE(obj, nullptr);

    object_t *saved_cg = command_giver;
    command_giver = obj;

    svalue_t *ret = APPLY_SLOT_CALL("register_actions", obj, 0, ORIGIN_DRIVER);
    ASSERT_NE(ret, nullptr);
    APPLY_SLOT_FINISH_CALL();

    command_giver = saved_cg;
    obj->flags |= O_ENABLE_COMMANDS;
  }

  array_t *fetch_state(object_t *obj) {
    svalue_t *ret = APPLY_SLOT_CALL("state", obj, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    auto view = lpc::svalue_view::from(ret);
    EXPECT_TRUE(view.is_array());
    array_t *state = ret->u.arr;
    if (state) {
      state->ref++;
    }
    APPLY_SLOT_FINISH_CALL();
    return state;
  }

  void expect_state(object_t *obj, const char *handler, const char *verb, const char *arg, int64_t call_count) {
    array_t *state = fetch_state(obj);
    ASSERT_NE(state, nullptr);
    ASSERT_EQ(state->size, 4);

    auto handler_view = lpc::svalue_view::from(&state->item[0]);
    ASSERT_TRUE(handler_view.is_string());
    EXPECT_STREQ(handler_view.c_str(), handler);

    auto verb_view = lpc::svalue_view::from(&state->item[1]);
    ASSERT_TRUE(verb_view.is_string());
    EXPECT_STREQ(verb_view.c_str(), verb);

    auto arg_view = lpc::svalue_view::from(&state->item[2]);
    ASSERT_TRUE(arg_view.is_string());
    EXPECT_STREQ(arg_view.c_str(), arg);

    auto count_view = lpc::svalue_view::from(&state->item[3]);
    ASSERT_TRUE(count_view.is_number());
    EXPECT_EQ(count_view.number(), call_count);

    free_array(state);
  }
};

TEST_F(ProcessCommandTest, HandlesMultiWordVerbFromAddAction) {
  object_t *obj = load_command_object("/tests/backend/test_process_command_multi");
  ASSERT_NE(obj, nullptr);
  register_actions(obj);

  char input[] = "look at tree";
  EXPECT_EQ(process_command(input, obj), 1);
  expect_state(obj, "multi", "look at", "tree", 1);

  destruct_object(obj);
}

TEST_F(ProcessCommandTest, HandlesShortVerbVariant) {
  object_t *obj = load_command_object("/tests/backend/test_process_command_short");
  ASSERT_NE(obj, nullptr);
  register_actions(obj);

  char input[] = "say hello world";
  EXPECT_EQ(process_command(input, obj), 1);
  expect_state(obj, "short", "say", "hello world", 1);

  destruct_object(obj);
}

TEST_F(ProcessCommandTest, HandlesNoSpaceXverbVariant) {
  object_t *obj = load_command_object("/tests/backend/test_process_command_xverb");
  ASSERT_NE(obj, nullptr);
  register_actions(obj);

  char input[] = "prefix";
  EXPECT_EQ(process_command(input, obj), 1);
  expect_state(obj, "xverb", "fix", "fix", 1);

  destruct_object(obj);
}

} // namespace
