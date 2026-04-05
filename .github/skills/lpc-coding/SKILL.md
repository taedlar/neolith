---
name: lpc-coding
description: 'Generate and refactor Neolith LPC code with correct driver semantics. Use when writing LPC functions, efun calls, simul_efun-aware code, inheritance calls, strict_types-safe code, inline LPC code (pre_text) in unit-tests, or when prompts mention ambiguity between local/simul/efun resolution. Prefer dot-call syntax receiver.method(args...) when explicit driver-efun intent is needed; dot-call lowers to efun::method(receiver, args...) and remains subject to efun type/arity validation.'
---

# LPC Coding Skill

## When to Use This Skill

- User asks to write or refactor LPC source files.
- Prompt includes efun usage and there is possible local/simul_efun shadowing.
- User asks for unambiguous driver efun dispatch.
- User asks about Neolith-specific LPC syntax or strict type behavior.
- Prompt requests chained transformations where dot-call improves readability.

## Core Rules

- Dot-call is efun-only sugar: `x.method(a, b)` => `efun::method(x, a, b)`.
- Use dot-call or `efun::` when explicit driver efun resolution is required.
- Use plain `method(x)` only when mudlib-aware resolution (local/simul_efun/efun order) is intentionally desired.
- Keep strict types valid; if injected receiver does not match efun contract, treat as compile error.
- Dot-call chaining is valid expression composition.

## Preferred Examples

```c
// dot-call for clarity and chaining. Check /docs/manual/lpc.md for supported efuns.
int n = "42".to_int();
float f = "42".to_int().to_float();
string s = "Hello".lower_case().capitalize();
```

```c
int a = efun::to_int("42"); // compatible with older LPC without dot-call support.
int b = to_int("42");
// Use b-form only when mudlib-aware resolution is desired.
```

## Ambiguity Guidance

- When calling an efun that is commonly overridden by simul_efuns, prefer regular function
- When calling an efun that is part of the native driver contract, prefer dot-call or `efun::`.
  - If code intent is portability to older LPC without dot-call, prefer `efun::`.
- In reviews, flag ambiguous regular function calls when explicit efun intent appears intended.
