#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "apply.h"
#include "error_context.h"
#include "error_guards.hpp"
#include "exceptions.hpp"
#include "rc.h"
#include "simulate.h"

extern "C"
void invoke_master_crash_handler_guarded(const char *msg) {
  error_context_t econ;

  try
    {
      neolith::error_boundary_guard boundary(&econ);
      set_context_transport_mode(&econ, ERROR_CONTEXT_TRANSPORT_EXCEPTION);

      try
        {
          svalue_t *ret;

          copy_and_push_string(msg);

          if (command_giver)
            push_object(command_giver);
          else
            push_undefined();

          if (current_object)
            push_object(current_object);
          else
            push_undefined();

          ret = apply_master_ob(APPLY_CRASH, 3);
          if (ret && ret != (svalue_t *)-1)
            {
              debug_message("{}\t----- mudlib crash handler finished, shutdown now.");
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          debug_message("{}\t***** error in master::%s(), shutdown now.", APPLY_CRASH);
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      debug_message("{}\t***** error in master::%s(), shutdown now.", APPLY_CRASH);
    }
}
