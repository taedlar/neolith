#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "src/std.h"
#include "src/apply.h"
#include "src/interpret.h"
#include "lpc/functional.h"
#include "lpc/include/origin.h"
#include "lpc/mapping.h"
#include "lpc/types.h"

#ifdef HAVE_CMARK
#include <cmark.h>

#include <memory>

namespace {

void push_node_argument(const char *value) {
  if (value) {
    push_malloced_string(int_string_copy(value, value + strlen(value)));
  }
  else {
    push_undefined();
  }
}

void add_string_attribute(mapping_t *attributes, const char *key, const char *value) {
  if (value) {
    add_mapping_string(attributes, key, value);
  }
}

void push_node_attributes(cmark_node *node) {
  mapping_t *attributes = allocate_mapping(8);
  cmark_node_type type = cmark_node_get_type(node);

  add_mapping_pair(attributes, "start_line", cmark_node_get_start_line(node));
  add_mapping_pair(attributes, "start_column", cmark_node_get_start_column(node));
  add_mapping_pair(attributes, "end_line", cmark_node_get_end_line(node));
  add_mapping_pair(attributes, "end_column", cmark_node_get_end_column(node));

  switch (type) {
  case CMARK_NODE_HEADING:
    add_mapping_pair(attributes, "level", cmark_node_get_heading_level(node));
    break;

  case CMARK_NODE_LIST:
    add_mapping_string(attributes, "type",
                       cmark_node_get_list_type(node) == CMARK_BULLET_LIST ? "bullet" : "ordered");
    add_mapping_pair(attributes, "start", cmark_node_get_list_start(node));
    add_mapping_pair(attributes, "tight", cmark_node_get_list_tight(node));
    break;

  case CMARK_NODE_CODE_BLOCK:
    add_string_attribute(attributes, "fence_info", cmark_node_get_fence_info(node));
    break;

  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
    add_string_attribute(attributes, "url", cmark_node_get_url(node));
    add_string_attribute(attributes, "title", cmark_node_get_title(node));
    break;

  case CMARK_NODE_CUSTOM_BLOCK:
  case CMARK_NODE_CUSTOM_INLINE:
    add_string_attribute(attributes, "on_enter", cmark_node_get_on_enter(node));
    add_string_attribute(attributes, "on_exit", cmark_node_get_on_exit(node));
    break;

  default:
    break;
  }

  push_undefined();
  sp->type = T_MAPPING;
  sp->subtype = 0;
  sp->u.map = attributes;
}

void call_renderer(svalue_t *callback, lpc::svalue *previous, cmark_node *node) {
  push_svalue(previous->raw());
  push_node_argument(cmark_node_get_type_string(node));
  push_node_argument(cmark_node_get_literal(node));
  push_node_attributes(node);

  svalue_t *result;
  if (callback->type == T_FUNCTION) {
    result = CALL_FUNCTION_POINTER_SLOT_CALL(callback->u.fp, 4);
  }
  else {
    result = APPLY_SLOT_CALL(SVALUE_STRPTR(callback), current_object, 4, ORIGIN_EFUN);
  }

  if (!result) {
    if (callback->type == T_FUNCTION) {
      CALL_FUNCTION_POINTER_SLOT_FINISH();
    }
    else {
      APPLY_SLOT_FINISH_CALL();
    }
    error("cmark_parse: callback could not be invoked.\n");
  }

  free_svalue(previous->raw(), "cmark_parse callback result");
  assign_svalue_no_free(previous->raw(), result);
  if (callback->type == T_FUNCTION) {
    CALL_FUNCTION_POINTER_SLOT_FINISH();
  }
  else {
    APPLY_SLOT_FINISH_CALL();
  }
}

} // namespace

#ifdef F_CMARK_PARSE
extern "C" void f_cmark_parse(void) {
  svalue_t *arguments = sp - 1;
  const char *source = SVALUE_STRPTR(arguments + 1);
  std::unique_ptr<cmark_node, decltype(&cmark_node_free)> document(
      cmark_parse_document(source, SVALUE_STRLEN(arguments + 1), CMARK_OPT_DEFAULT),
      cmark_node_free);
  std::unique_ptr<cmark_iter, decltype(&cmark_iter_free)> iterator(
      cmark_iter_new(document.get()), cmark_iter_free);
  lpc::svalue previous;

  previous.raw()->subtype = T_UNDEFINED;
  while (cmark_iter_next(iterator.get()) != CMARK_EVENT_DONE) {
    if (cmark_iter_get_event_type(iterator.get()) == CMARK_EVENT_ENTER) {
      call_renderer(arguments, &previous, cmark_iter_get_node(iterator.get()));
    }
  }

  free_svalue(sp, "f_cmark_parse");
  free_svalue(sp - 1, "f_cmark_parse");
  sp--;
  assign_svalue_no_free(sp, previous.raw());
}
#endif
#endif