#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "error_context.h"
#include "exceptions.hpp"
#include "frame.h"
#include "interpret.h"

extern "C"
void do_catch_cpp(const char *p, unsigned short new_pc_offset) {
  error_context_t econ;
  const char *noncatchable_error = nullptr;

  (void)new_pc_offset; /* program counter is restored by restore_context() */

  if (!save_context(&econ)) {
    error("*Can't catch too deep recursion error.");
  }
  set_context_transport_mode(&econ, ERROR_CONTEXT_TRANSPORT_EXCEPTION);

  push_control_stack(FRAME_CATCH);

  try {
    assign_svalue(&catch_value, &const1);
    /* csp->extern_call is not used on this eval boundary */
    eval_instruction(p);
  } catch (const neolith::catchable_runtime_error &) {
    restore_context(&econ);
    sp++;
    *sp = catch_value;
    catch_value = const1;
  } catch (const neolith::noncatchable_runtime_limit &e) {
    restore_context(&econ);

    if (get_error_state(ES_MAX_EVAL_COST)) {
      noncatchable_error = "*Can't catch eval cost too big error.";
    } else if (get_error_state(ES_STACK_FULL)) {
      noncatchable_error = "*Can't catch too deep recursion error.";
    } else {
      noncatchable_error = e.what();
    }
  }

  pop_context(&econ);

  if (noncatchable_error) {
    error("%s", noncatchable_error);
  }
}
