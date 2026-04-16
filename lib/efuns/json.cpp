/*
 * json.cpp — LPC to_json / from_json efuns backed by Boost.JSON.
 *
 * to_json(mixed) → string  — serializes an LPC value tree to a JSON string.
 * from_json(string|buffer) → mixed — parses JSON bytes into an LPC value tree.
 *
 * Conditional compilation: both efuns are absent unless HAVE_BOOST_JSON is
 * defined (i.e. PACKAGE_JSON=ON at cmake configure time with Boost.JSON
 * available).
 *
 * error-boundary safety
 * ---------------------
 * LPC runtime errors are delivered through the driver's exception boundaries.
 * Two strategies are used to avoid leaking live C++ objects across error paths:
 *
 *   f_to_json: validate_for_json() walks the entire LPC value tree and
 *     calls error() (if any mapping key is non-string) BEFORE any Boost.JSON
 *     objects are allocated.  After that point no error() is expected.
 *
 *   f_from_json: uses the error_code overload of boost::json::parse() to
 *     avoid exceptions.  If parse fails, the returned boost::json::value is
 *     a default null (no heap allocation), so early error propagation is safe.
 *     OOM errors inside json_to_lpc() (allocate_array / allocate_mapping)
 *     are catastrophic-context events treated as unrecoverable.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "src/std.h"
#include "src/interpret.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/mapping.h"
#include "lpc/types.h"

/* mapping.h defines max(x,y) for C compatibility; undefine before C++ headers
 * to avoid conflicts with std::max templates in Boost and the C++ standard
 * library. */
#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#ifdef HAVE_BOOST_JSON
#ifdef BOOST_JSON_HEADER_ONLY
#include <boost/json/src.hpp>  // Boost.JSON header-only implementation
#else
#include <boost/json.hpp>
#endif


/* --------------------------------------------------------------------------
 * Validation pass (f_to_json only)
 *
 * Recursively walks the LPC value tree and calls error() if any mapping key
 * is not T_STRING or if nesting exceeds MAX_SAVE_SVALUE_DEPTH.  Must complete
 * before any Boost.JSON objects are allocated.
 * -------------------------------------------------------------------------- */

static void validate_for_json(svalue_t *v, int depth)
{
  mapping_t *m;
  mapping_node_t *node;
  int i;

  if (depth > MAX_SAVE_SVALUE_DEPTH)
    error("to_json: value nested too deep (%d).\n", MAX_SAVE_SVALUE_DEPTH);

  switch (v->type) {
  case T_ARRAY:
    for (i = 0; i < (int)v->u.arr->size; i++)
      validate_for_json(&v->u.arr->item[i], depth + 1);
    break;

  case T_MAPPING:
    m = v->u.map;
    for (i = 0; i <= (int)m->table_size; i++) {
      for (node = m->table[i]; node; node = node->next) {
        if (node->values[0].type != T_STRING)
          error("to_json: mapping has a non-string key.\n");
        validate_for_json(&node->values[1], depth + 1);
      }
    }
    break;

  default:
    break;
  }
}


/* --------------------------------------------------------------------------
 * LPC → JSON conversion
 *
 * Called after a successful validate_for_json() pass; no error() is called
 * from here.  Objects, functions, buffers, and classes serialize as null.
 * -------------------------------------------------------------------------- */

static boost::json::value lpc_to_json(svalue_t *v)
{
  mapping_t *m;
  mapping_node_t *node;
  int i;

  switch (v->type) {
  case T_NUMBER:
    if (v->subtype == T_UNDEFINED)
      return nullptr;
    return v->u.number;

  case T_REAL:
    return v->u.real;

  case T_STRING:
    /* Use explicit byte-span to preserve embedded null bytes.
     * SVALUE_STRPTR(v) alone would use C-string semantics (stop at null).
     * Pass the full span [data, data+length) to Boost.JSON. */
    return boost::json::string(
      boost::json::string_view(SVALUE_STRPTR(v), SVALUE_STRLEN(v))
    );

  case T_ARRAY: {
    boost::json::array arr;
    arr.reserve(v->u.arr->size);
    for (i = 0; i < (int)v->u.arr->size; i++)
      arr.push_back(lpc_to_json(&v->u.arr->item[i]));
    return arr;
  }

  case T_MAPPING: {
    boost::json::object obj;
    m = v->u.map;
    for (i = 0; i <= (int)m->table_size; i++) {
      for (node = m->table[i]; node; node = node->next)
        obj.emplace(
          boost::json::string_view(SVALUE_STRPTR(&node->values[0]),
                                   SVALUE_STRLEN(&node->values[0])),
          lpc_to_json(&node->values[1])
        );
    }
    return obj;
  }

  default:
    /* T_OBJECT, T_FUNCTION, T_BUFFER, T_CLASS → null */
    return nullptr;
  }
}


/* --------------------------------------------------------------------------
 * JSON → LPC conversion
 *
 * Writes a JSON value into a pre-freed svalue_t slot.  Allocates LPC arrays
 * and mappings from the driver heap; recurses for compound types.
 *
 * JSON booleans map to T_NUMBER 0/1.  JSON null maps to T_NUMBER 0 with
 * subtype T_UNDEFINED (same value undefinedp() tests).
 * -------------------------------------------------------------------------- */

static void json_to_lpc(boost::json::value const& jv, svalue_t *out)
{
  switch (jv.kind()) {

  case boost::json::kind::int64:
    out->type = T_NUMBER;
    out->subtype = 0;
    out->u.number = jv.get_int64();
    break;

  case boost::json::kind::uint64:
    out->type = T_NUMBER;
    out->subtype = 0;
    out->u.number = jv.get_uint64() > INT64_MAX ? INT64_MAX : static_cast<int64_t>(jv.get_uint64());
    break;

  case boost::json::kind::double_:
    out->type = T_REAL;
    out->subtype = 0;
    out->u.real = jv.get_double();
    break;

  case boost::json::kind::string: {
    auto const& js = jv.get_string();
    /* Use span-based int_string_copy to preserve embedded null bytes.
     * js.c_str() would truncate at the first null; instead pass the actual
     * byte range [data, data+size). */
    SET_SVALUE_MALLOC_STRING(out, int_string_copy(js.data(), js.data() + js.size()));
    break;
  }

  case boost::json::kind::bool_:
    out->type = T_NUMBER;
    out->subtype = 0;
    out->u.number = jv.get_bool() ? 1 : 0;
    break;

  case boost::json::kind::null:
    out->type = T_NUMBER;
    out->subtype = T_UNDEFINED;
    out->u.number = 0;
    break;

  case boost::json::kind::array: {
    auto const& ja = jv.get_array();
    array_t *arr = allocate_array(ja.size());
    /* allocate_array initialises items to const0 (T_NUMBER 0); safe to
     * overwrite without freeing. */
    for (size_t i = 0; i < ja.size(); i++)
      json_to_lpc(ja[i], &arr->item[i]);
    out->type = T_ARRAY;
    out->subtype = 0;
    out->u.arr = arr;
    break;
  }

  case boost::json::kind::object: {
    auto const& jo = jv.get_object();
    mapping_t *m = allocate_mapping(jo.size());
    for (auto const& kv : jo) {
      /* Build the temporary key from the exact Boost.JSON byte span so
       * embedded null bytes are preserved. Using c_str()-based key setup
       * would truncate keys at the first '\0'. */
      lpc::svalue key;
      auto const& jk = kv.key();
      SET_SVALUE_MALLOC_STRING(key.raw(),
                               int_string_copy(jk.data(), jk.data() + jk.size()));
      svalue_t *val = find_for_insert(m, key.raw(), 1);
      val->type = T_INVALID;
      json_to_lpc(kv.value(), val);
    }
    out->type = T_MAPPING;
    out->subtype = 0;
    out->u.map = m;
    break;
  }
  }
}


/* --------------------------------------------------------------------------
 * Efun bodies
 * -------------------------------------------------------------------------- */

extern "C" {

#ifdef F_TO_JSON
/**
 * @brief to_json(mixed val) → string
 *
 * Serializes an LPC value to a JSON string.  Mapping keys must all be
 * strings; a non-string key raises a runtime error.  Objects, functions,
 * buffers, and classes serialize as JSON null.
 */
void f_to_json(void)
{
  validate_for_json(sp, 0);  /* may call error(); no C++ objects allocated yet */

  {
    boost::json::value jv = lpc_to_json(sp);
    std::string s = boost::json::serialize(jv);
    malloc_str_t result = string_copy(s.c_str(), "f_to_json");
    /* jv and s are destroyed here at end of inner scope, before free_svalue */
    free_svalue(sp, "f_to_json");
    put_malloced_string(result);
  }
}
#endif /* F_TO_JSON */


#ifdef F_FROM_JSON
/**
 * @brief from_json(string|buffer json) → mixed
 *
 * Parses a JSON string into an LPC value.  Raises a runtime error on parse
 * failure.  JSON null becomes the LPC undefined value (undefinedp() returns
 * true).  JSON booleans become LPC integers 0/1.
 */
void f_from_json(void)
{
  boost::json::value* parsed = nullptr;
  const char *input = nullptr;
  size_t input_len = 0;

  if (sp->type == T_STRING) {
    input = SVALUE_STRPTR(sp);
    input_len = SVALUE_STRLEN(sp);
  }
  else if (sp->type == T_BUFFER) {
    input = reinterpret_cast<const char *>(sp->u.buf->item);
    input_len = sp->u.buf->size;
  }
  else {
    bad_argument(sp, T_STRING | T_BUFFER, 1, F_FROM_JSON);
    return;
  }

  {
    boost::system::error_code ec;
    boost::json::value jv = boost::json::parse(
        boost::json::string_view(input, input_len),
        ec
    );

    free_svalue(sp, "f_from_json");
    sp->type = T_INVALID;

    if (ec) {
      /* Build the error message before raising the runtime error.
       * jv is a default null value when parse fails (no heap allocation). */
      char errbuf[256];
      {
        std::string msg = ec.message();
        snprintf(errbuf, sizeof(errbuf), "from_json: invalid JSON: %s\n",
                msg.c_str());
      }
      sp->type = T_NUMBER;
      sp->subtype = 0;
      sp->u.number = 0;
      error("%s", errbuf);
    }
    /* Move parsed result to the heap so ownership stays explicit if
     * json_to_lpc() raises a runtime error (for example on OOM). */
    parsed = new boost::json::value(std::move(jv));
  }

  json_to_lpc(*parsed, sp);
  delete parsed;
}
#endif /* F_FROM_JSON */

}  /* extern "C" */

#endif /* HAVE_BOOST_JSON */
