# cmark_parse()
## NAME
**cmark_parse** - parse CommonMark text and render its AST through a callback

## SYNOPSIS
~~~cxx
mixed cmark_parse(string | function renderer, string source);
~~~

## DESCRIPTION
`cmark_parse()` parses `source` as CommonMark Markdown and traverses the
resulting abstract syntax tree in preorder. It invokes `renderer` once for each
node, including the root `document` node.

`renderer` may be a function name in the calling object or a function pointer.
It receives four arguments:

~~~cxx
mixed renderer(mixed previous, string node_type, mixed literal, mapping attributes);
~~~

- `previous` is `undefinedp` for the first node, then the value returned by the
  preceding callback invocation.
- `node_type` is cmark's node-type string, such as `"document"`, `"paragraph"`,
  `"text"`, or `"emph"`.
- `literal` is the node's string content when it has one, otherwise `undefinedp`.
  For example, text and code nodes provide a literal; document and paragraph
  nodes do not.
- `attributes` is a mapping of node metadata. Every node includes `start_line`,
  `start_column`, `end_line`, and `end_column`. Headings include `level`; lists
  include `type`, `start`, and `tight`; code blocks may include `fence_info`;
  links and images include `url` and `title`; custom nodes may include
  `on_enter` and `on_exit`. Optional keys are omitted when cmark has no value.

The efun returns the value produced by the final callback invocation. This makes
the callback sequence a reducer over the Markdown AST.

For example, this callback collects text-node literals:

~~~cxx
string collect_text(mixed previous, string node_type, mixed literal, mapping attributes) {
    if (undefinedp(previous))
        previous = "";
    return node_type == "text" ? previous + literal : previous;
}

string text = cmark_parse("collect_text", "Hello *world*");
~~~

The efun is available only when the driver is built with `PACKAGE_CMARK` enabled
and cmark support is available. Set `-DPACKAGE_CMARK=OFF` when configuring CMake
to exclude it. LPC code can test the predefined macro `__PACKAGE_CMARK__` before
using it.

## SEE ALSO
[functionp()](functionp.md), [undefinedp()](undefinedp.md)