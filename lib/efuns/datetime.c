#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <time.h>
#include "lpc/array.h"
#include "lpc/include/localtime.h"
#include "src/std.h"

#ifdef F_UPTIME
void
f_uptime (void)
{
  push_number (time (NULL) - boot_time);
}
#endif


#ifdef F_TIME
void
f_time (void)
{
  push_number (current_time);
}
#endif


#ifdef F_CTIME
void
f_ctime (void)
{
  char *cp, *nl, *p;
  size_t len;
  time_t t = (time_t)sp->u.number;

  cp = ctime (&t);
  if ((nl = strchr (cp, '\n')))
    len = nl - cp;
  else
    len = strlen (cp);

  p = new_string (len, "f_ctime");
  strncpy (p, cp, len);
  p[len] = '\0';
  put_malloced_string (p);
}
#endif


#ifdef F_LOCALTIME
/* FIXME: most of the #ifdefs here should be based on configure checks
   instead.  Same for rusage() */
void
f_localtime (void)
{
  struct tm local_tm;
  struct tm *tm = &local_tm;
  array_t *vec;
  time_t lt;

#ifdef sequent
  struct timezone tz;
#endif

  lt = (time_t)sp->u.number;
#ifdef _WIN32
  _localtime64_s (&local_tm, (__time64_t *)&sp->u.number);
#else
  tm = localtime (&lt);
#endif
  if (!tm)
    {
      error ("Bad time value %lu passed to localtime()\n", (unsigned long)lt);
    }

  vec = allocate_empty_array (10);
  vec->item[LT_SEC].type = T_NUMBER;
  vec->item[LT_SEC].u.number = tm->tm_sec;
  vec->item[LT_MIN].type = T_NUMBER;
  vec->item[LT_MIN].u.number = tm->tm_min;
  vec->item[LT_HOUR].type = T_NUMBER;
  vec->item[LT_HOUR].u.number = tm->tm_hour;
  vec->item[LT_MDAY].type = T_NUMBER;
  vec->item[LT_MDAY].u.number = tm->tm_mday;
  vec->item[LT_MON].type = T_NUMBER;
  vec->item[LT_MON].u.number = tm->tm_mon;
  vec->item[LT_YEAR].type = T_NUMBER;
  vec->item[LT_YEAR].u.number = tm->tm_year + 1900;
  vec->item[LT_WDAY].type = T_NUMBER;
  vec->item[LT_WDAY].u.number = tm->tm_wday;
  vec->item[LT_YDAY].type = T_NUMBER;
  vec->item[LT_YDAY].u.number = tm->tm_yday;
  vec->item[LT_GMTOFF].type = T_NUMBER;
  vec->item[LT_ZONE].type = T_STRING;
  vec->item[LT_ZONE].subtype = STRING_MALLOC;
#if defined(BSD42) || defined(apollo) || defined(_AUX_SOURCE) \
	|| defined(OLD_ULTRIX)
  /* 4.2 BSD doesn't seem to provide any way to get these last two values */
  vec->item[LT_GMTOFF].u.number = 0;
  vec->item[LT_ZONE].type = T_NUMBER;
  vec->item[LT_ZONE].u.number = 0;
#else /* BSD42 */
#if defined(sequent)
  vec->item[LT_GMTOFF].u.number = 0;
  gettimeofday (NULL, &tz);
  vec->item[LT_GMTOFF].u.number = tz.tz_minuteswest;
  vec->item[LT_ZONE].u.string =
    string_copy (timezone (tz.tz_minuteswest, tm->tm_isdst), "f_localtime");
#else /* sequent */
#if (defined(hpux) || defined(_SEQUENT_) || defined(_AIX) || defined(SunOS_5) \
	|| defined(SVR4) || defined(sgi) || defined(__linux__) || defined(cray) \
	|| defined(LATTICE) || defined(SCO))
  if (!tm->tm_isdst)
    {
      vec->item[LT_GMTOFF].u.number = timezone;
      vec->item[LT_ZONE].u.string = string_copy (tzname[0], "f_localtime");
    }
  else
    {
#if (defined(_AIX) || defined(hpux) || defined(__linux__) || defined(cray) \
	|| defined(LATTICE))
      vec->item[LT_GMTOFF].u.number = timezone;
#else
      vec->item[LT_GMTOFF].u.number = altzone;
#endif
      vec->item[LT_ZONE].u.string = string_copy (tzname[1], "f_localtime");
    }
#else
#ifndef _WIN32
  vec->item[LT_GMTOFF].u.number = tm->tm_gmtoff;
  vec->item[LT_ZONE].u.string = string_copy (tm->tm_zone, "f_localtime");
#else
  vec->item[LT_GMTOFF].u.number = _timezone;
  vec->item[LT_ZONE].u.string = string_copy (_tzname[_daylight ? 1 : 0], "f_localtime");
#endif
#endif
#endif /* sequent */
#endif /* BSD42 */
  put_array (vec);
}
#endif


#ifdef F_IS_DAYLIGHT_SAVINGS_TIME
void
f_is_daylight_savings_time (void)
{
  struct tm *t;
  int time_to_check;
  char *timezone;
  char *old_tz;

  time_to_check = sp->u.number;
  pop_stack ();
  timezone = sp->u.string;
  pop_stack ();

  old_tz = set_timezone (timezone);

  t = localtime ((time_t *) & time_to_check);

  push_number ((t->tm_isdst) > 0);

  reset_timezone (old_tz);
}
#endif


/*
** John Viega (rust@lima.imaginary.com) Jan, 1996
** efuns for doing time zone conversions.  Much friendlier 
** than doing all the lookup tables in LPC.
** most muds have traditionally just used an offset of the 
** mud time or GMT, and this isn't always correct.
*/

#ifdef F_ZONETIME

char *
set_timezone (char *timezone)
{
  char put_tz[20];
  char *old_tz;

  old_tz = getenv ("TZ");
  sprintf (put_tz, "TZ=%s", timezone);
  putenv (put_tz);
  tzset ();
  return old_tz;
}

void
reset_timezone (char *old_tz)
{
  int i = 0;
  int env_size = 0;
  char put_tz[20];

  if (!old_tz)
    {
      while (environ[env_size] != NULL)
	{
	  if (strlen (environ[env_size]) > 3 && environ[env_size][2] == '='
	      && environ[env_size][1] == 'Z' && environ[env_size][0] == 'T')
	    {
	      i = env_size;
	    }
	  env_size++;
	}
      if ((i + 1) == env_size)
	{
	  environ[i] = NULL;
	}
      else
	{
	  environ[i] = environ[env_size - 1];
	  environ[env_size - 1] = NULL;
	}
    }
  else
    {
      sprintf (put_tz, "TZ=%s", old_tz);
      putenv (put_tz);
    }
  tzset ();
}

void
f_zonetime (void)
{
  char *timezone, *old_tz;
  char *retv;
  int time_val;
  int len;

  time_val = sp->u.number;
  pop_stack ();
  timezone = sp->u.string;
  pop_stack ();

  old_tz = set_timezone (timezone);
  retv = ctime ((time_t *) & time_val);
  len = strlen (retv);
  retv[len - 1] = '\0';
  reset_timezone (old_tz);
  push_malloced_string (string_copy (retv, "zonetime"));

}
#endif


