#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "backend.h"
#include "error_context.h"
#include "error_guards.hpp"
#include "exceptions.hpp"
#include "rc.h"
#include "simul_efun.h"
#include "simulate.h"

extern "C"
int run_mudlib_startup_guarded(void) {
  error_context_t econ;

  eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

  try
    {
      neolith::error_boundary_guard boundary(&econ);
      set_context_transport_mode(&econ, ERROR_CONTEXT_TRANSPORT_EXCEPTION);

      try
        {
          current_time = time(NULL);

          debug_message("{}\t----- loading simul efuns -----");
          init_simul_efun(CONFIG_STR(__SIMUL_EFUN_FILE__));

          debug_message("{}\t----- loading master -----");
          init_master(CONFIG_STR(__MASTER_FILE__), NULL);

          debug_message("{}\t----- epilogue -----");
          preload_objects(MAIN_OPTION(epilog_level));

          return 1;
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          return 0;
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      return 0;
    }
}
