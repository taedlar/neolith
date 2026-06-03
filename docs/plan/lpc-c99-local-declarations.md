# LPC C99-Style Local Declarations Plan

## Summary
Enable C99-style declaration placement in compound blocks for LPC grammar and compiler generation.

Status:
- Phase 1 implemented: block-item grammar refactor landed.
- Initial compiler test matrix subset implemented and passing.

Target behavior:
- Allow local declarations to appear after statements inside `{ ... }` blocks.
- Keep current type checks, promotions, and local-variable accounting semantics.

Out of scope for this plan:
- C++-style init-statements in `if (...)` or `switch (...)`.
- Broad switch label/declaration behavior changes beyond preserving current behavior.

## Current State
The grammar currently requires declaration prelude before statements in blocks:
- `block: '{' local_declarations statements '}'`

Local declarations are accumulated as a declaration node chain plus local-count metadata (`decl.node`, `decl.num`) and are popped at enclosing boundaries.

## Design Goals
1. Support C99 mixed declarations/statements in block scope.
2. Preserve existing diagnostics and type semantics for local declaration initializers.
3. Avoid regressions in loop/switch semantics.
4. Keep implementation minimal and incremental.

## Non-Goals
1. No new declaration forms in expression contexts.
2. No semantic change to `for (...)` init in phase 1.
3. No change to switch label rules in phase 1.

## Implementation Plan

### Phase 1: Grammar Refactor to Block Items
Introduce block-item sequencing in grammar while preserving existing semantic actions.

Planned nonterminals:
- `block_items` (returns `<decl>`)
- `block_item` (returns `<decl>`)
- `local_declaration_stmt` (returns `<decl>`)

Target shape:
- `block: '{' block_items '}'`
- `block_items: /* empty */ | block_items block_item`
- `block_item: statement | local_declaration_stmt`

Notes:
- `statement` branch contributes node with `num = 0`.
- `local_declaration_stmt` branch contributes existing local-declaration node and local count.
- Keep declaration statements out of expression grammar.

### Phase 2: Reuse Existing Declaration Semantics
Reuse existing local-declaration logic from current `local_declarations` actions:
- void local rejection
- `current_type` setup
- initializer compatibility checks
- promotions (`do_promotions`)
- generated assignment nodes

Refactor existing rules into reusable `local_declaration_stmt` without changing behavior.

### Phase 3: Scope and Local Pop Validation
Ensure local lifetime remains block-scoped:
- Declarations within a block item sequence are visible from declaration point forward.
- Locals are popped at end of the containing block, not per declaration statement.
- Existing `decl_block`/`pop_n_locals` usage for nested block/loop statements remains valid.

### Phase 4: Switch Stability (No Expansion)
Do not broaden switch declaration placement in this pass.

Maintain compatibility with current switch production behavior and local-pop expectations. Any switch liberalization should be a follow-up plan.

### Phase 5: Optional Follow-Up for `for` Init
Current `first_for_expr` supports either expression or single local-def-with-init. Leave unchanged in this plan unless explicitly requested.

## Risk Register
1. Parser conflicts after introducing block-item alternation.
   - Mitigation: keep declaration-start patterns constrained to existing `basic_type`/`optional_star` path.
2. Local count accounting drift causing pop underflow/overflow.
   - Mitigation: centralize `decl` combination helper logic in grammar actions and test nested scopes.
3. Switch behavior regression from incidental grammar interaction.
   - Mitigation: keep switch productions untouched in phase 1 and add regression tests.

## Test Matrix

### A. New Acceptance Tests (compile should succeed)
1. Mixed declaration after executable statement in same block.
2. Multiple declaration groups interleaved with statements.
3. Nested block shadowing (`int x; { int x; ... }`).
4. Late declaration with initializer using prior computed value.
5. Mixed declarations inside loop body block after non-declaration statement.

### B. New Rejection Tests (compile should fail)
1. Local `void` declaration in mixed position.
2. Type-mismatched initializer in mixed position.
3. Use-before-declaration when strict typing catches unresolved/incorrect symbol use.

### C. Regression Tests (must remain unchanged)
1. Existing LPC compiler suite still passes.
2. Dot-call compiler tests remain unchanged.
3. Save-binary compiler tests remain unchanged.
4. Existing switch parsing and case/default behavior unchanged.

### D. Scope Semantics Checks
1. Declaration visibility starts at declaration point, not block start.
2. Inner-block locals are not visible after block exit.
3. No leaked locals after nested declaration-heavy blocks.

## Test Placement
Add focused tests to:
- `tests/test_lpc_compiler/test_lpc_compiler.cpp` or a new companion file under `tests/test_lpc_compiler/`.

Suggested naming pattern:
- `C99BlockDecl*` test names under `LPCCompilerTest` fixture.

## Execution Order
1. Grammar refactor introducing block items.
2. Compiler test additions for acceptance/rejection/scope semantics.
3. Targeted test run for LPC compiler suite.
4. Broader LPC-labeled run if needed.

## Exit Criteria
1. Mixed declaration/statement blocks compile as expected.
2. Existing declaration diagnostics unchanged.
3. No regressions in LPC compiler tests.
4. Documented follow-up list for switch and `for` init expansion remains explicit.
