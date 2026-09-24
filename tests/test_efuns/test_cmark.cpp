#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "src/apply.h"
#include "lpc/include/origin.h"

#ifdef HAVE_CMARK
namespace {

object_t *load_cmark_test_object() {
    return load_object("/tests/efuns/test_cmark_parse", R"(
        mixed *events = ({});

        mixed render(mixed previous, string node_type, mixed literal, mapping attributes) {
            events += ({ ({ previous, node_type, literal, attributes }) });
            return sizeof(events);
        }

        mixed parse_by_name(string source) {
            return efun::cmark_parse("render", source);
        }

        mixed parse_by_function(string source) {
            return efun::cmark_parse((: render :), source);
        }

        mixed *query_events() {
            return events;
        }
    )");
}

void expect_event(const array_t *events, int index, const char *type, const char *literal) {
    auto event = lpc::svalue_view::from(&events->item[index]);
    ASSERT_TRUE(event.is_array());
    ASSERT_EQ(event.raw()->u.arr->size, 4);

    auto node_type = lpc::svalue_view::from(&event.raw()->u.arr->item[1]);
    ASSERT_TRUE(node_type.is_string());
    EXPECT_STREQ(node_type.c_str(), type);

    auto node_literal = lpc::svalue_view::from(&event.raw()->u.arr->item[2]);
    if (literal) {
        ASSERT_TRUE(node_literal.is_string());
        EXPECT_STREQ(node_literal.c_str(), literal);
    }
    else {
        EXPECT_TRUE(node_literal.is_number());
        EXPECT_EQ(event.raw()->u.arr->item[2].subtype, T_UNDEFINED);
    }

    auto attributes = lpc::svalue_view::from(&event.raw()->u.arr->item[3]);
    EXPECT_EQ(attributes.raw()->type, T_MAPPING);
}

void expect_parse_result(object_t *obj, const char *method) {
    svalue_t *sp_before = sp;
    push_constant_string("Hello *world*");
    svalue_t *result = APPLY_SLOT_CALL(method, obj, 1, ORIGIN_DRIVER);
    ASSERT_NE(result, nullptr);

    auto result_view = lpc::svalue_view::from(result);
    ASSERT_TRUE(result_view.is_number());
    EXPECT_EQ(result_view.number(), 5);
    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before);
}

void expect_events(object_t *obj) {
    svalue_t *sp_before = sp;
    svalue_t *result = APPLY_SLOT_CALL("query_events", obj, 0, ORIGIN_DRIVER);
    ASSERT_NE(result, nullptr);

    auto result_view = lpc::svalue_view::from(result);
    ASSERT_TRUE(result_view.is_array());
    const array_t *events = result->u.arr;
    ASSERT_EQ(events->size, 5);

    expect_event(events, 0, "document", nullptr);
    expect_event(events, 1, "paragraph", nullptr);
    expect_event(events, 2, "text", "Hello ");
    expect_event(events, 3, "emph", nullptr);
    expect_event(events, 4, "text", "world");

    auto initial_value = lpc::svalue_view::from(&events->item[0].u.arr->item[0]);
    EXPECT_TRUE(initial_value.is_number());
    EXPECT_EQ(events->item[0].u.arr->item[0].subtype, T_UNDEFINED);
    for (int index = 1; index < 5; index++) {
        auto previous = lpc::svalue_view::from(&events->item[index].u.arr->item[0]);
        EXPECT_TRUE(previous.is_number());
        EXPECT_EQ(previous.number(), index);
    }

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before);
}

} // namespace

TEST_F(EfunsTest, cmarkParseNamedCallbackTraversesMarkdownAst) {
    object_t *obj = load_cmark_test_object();
    ASSERT_NE(obj, nullptr);

    expect_parse_result(obj, "parse_by_name");
    expect_events(obj);

    destruct_object(obj);
}

TEST_F(EfunsTest, cmarkParseFunctionCallbackTraversesMarkdownAst) {
    object_t *obj = load_cmark_test_object();
    ASSERT_NE(obj, nullptr);

    expect_parse_result(obj, "parse_by_function");
    expect_events(obj);

    destruct_object(obj);
}

TEST_F(EfunsTest, cmarkParseAttributesIncludeHeadingLevel) {
    object_t *obj = load_object("/tests/efuns/test_cmark_heading_attributes", R"(
        mixed render(mixed previous, string node_type, mixed literal, mapping attributes) {
            return node_type == "heading" ? attributes["level"] : previous;
        }

        mixed parse_heading() {
            return efun::cmark_parse("render", "# Heading");
        }
    )");
    ASSERT_NE(obj, nullptr);

    svalue_t *sp_before = sp;
    svalue_t *result = APPLY_SLOT_CALL("parse_heading", obj, 0, ORIGIN_DRIVER);
    ASSERT_NE(result, nullptr);
    auto result_view = lpc::svalue_view::from(result);
    ASSERT_TRUE(result_view.is_number());
    EXPECT_EQ(result_view.number(), 1);
    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before);

    destruct_object(obj);
}

TEST_F(EfunsTest, cmarkPackagePredefineIsVisibleToLpc) {
    object_t *obj = load_object("/tests/efuns/test_cmark_package_predefine", R"(
#ifndef __PACKAGE_CMARK__
#error PACKAGE_CMARK must be defined when cmark_parse is available
#endif
        int cmark_is_available() { return 1; }
    )");
    ASSERT_NE(obj, nullptr);

    destruct_object(obj);
}
#endif