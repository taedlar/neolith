#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "apply.h"
#include "error_context.h"
#include "error_guards.hpp"
#include "exceptions.hpp"
#include "lpc/object.h"

extern "C"
svalue_t *safe_apply_cpp (const char *fun, object_t *ob, int num_arg, int where) {
  svalue_t *ret = 0;
  error_context_t econ;

  if (!ob || (ob->flags & O_DESTRUCTED))
    {
      return 0;
    }

  try
    {
      neolith::error_boundary_guard boundary (&econ);

      try
        {
          ret = apply (fun, ob, num_arg, where);
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore ();
          ret = 0;
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      ret = 0;
    }

  return ret;
}
