#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/object.h"


#ifdef F_SET_HEART_BEAT
void
f_set_heart_beat (void)
{
  int tick = (int)(sp--)->u.number;
  set_heart_beat (current_object, tick);
}
#endif


#ifdef F_QUERY_HEART_BEAT
void
f_query_heart_beat (void)
{
  object_t *ob;
  free_object (ob = sp->u.ob, "f_query_heart_beat");
  put_number (query_heart_beat (ob));
}
#endif


/* also Beek */
#ifdef F_HEART_BEATS
void
f_heart_beats (void)
{
  push_refed_array (get_heart_beats ());
}
#endif

