/*  $Id: prototype.lpc,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#define _FUNC_SPEC_

#include <options.h>

/*
 * The following specifies the operators used by the interpreter.
 * Normally, these should not be commented out.
 */

operator pop_value, push, efun0, efun1, efun2, efun3, efunv;

operator number, real, byte, nbyte, string, short_string, const0, const1;

operator aggregate, aggregate_assoc;
#ifdef DEBUG
operator break_point;
#endif

/* these must be set up so that F_BRANCH is the last foward branch and
 * F_BRANCH_X + 3 == F_BBRANCH_X
 */
operator branch_when_zero, branch_when_non_zero, branch;
operator bbranch_when_zero, bbranch_when_non_zero, bbranch;

operator branch_ne, branch_ge, branch_le, branch_eq, bbranch_lt;

operator foreach, next_foreach, exit_foreach;
operator loop_cond_local, loop_cond_number;
operator loop_incr;
operator while_dec;

operator lor, land;

operator catch, end_catch;
operator time_expression, end_time_expression;

operator switch;

operator call_function_by_address, call_inherited, return, return_zero;

/* eq must be first, gt must be last; c.f. is_boolean() */
operator eq, ne, le, lt, ge, gt;

operator inc, dec, pre_inc, post_inc, pre_dec, post_dec;

operator transfer_local;

/* lvalue eops must be the original eop + 1 */
operator local, local_lvalue;
operator global, global_lvalue;
operator member, member_lvalue;
operator index, index_lvalue;
operator rindex, rindex_lvalue;
operator nn_range, nn_range_lvalue, rn_range, rn_range_lvalue;
operator rr_range, rr_range_lvalue, nr_range, nr_range_lvalue;
operator ne_range, re_range;

/* these must all be together */
operator add_eq, sub_eq, and_eq, or_eq, xor_eq, lsh_eq, rsh_eq, mult_eq;
operator div_eq, mod_eq, assign;

operator void_add_eq, void_assign, void_assign_local;

operator add, subtract, multiply, divide, mod, and, or, xor, lsh, rsh;
operator not, negate, compl;

operator function_constructor;
operator simul_efun;

operator sscanf;
operator parse_command;

operator new_class, new_empty_class;
operator expand_varargs;

/*
 * The following specifies types and arguments for efuns.
 * An argument can have two different types with the syntax 'type1 | type2'.
 * An argument is marked as optional if it also takes the type 'void'.
 *
 * Comment out the efuns that you do not want.  Be careful not to comment
 * out something that you need.
 *
 * The order in which the efuns are listed here is the order in which opcode
 * #'s will be assigned.  It is in your interest to move the least frequently
 * used efuns to the bottom of this file (and the most frequently used
 * ones to the top).  The opcprof() efun could help you find out which
 * efuns are most often and least often used.  The reason for ordering
 * the efuns is that only the first 255 efuns are represented using
 * a single byte.  Any additional efuns require two bytes.
 */

/* most frequently used functions */

/* These next few efuns are used internally; do not remove them */
/* used by X->f() */
unknown call_other(object | string | object *, string | mixed *,...);
/* used by (*f)(...) */
mixed evaluate(mixed, ...);
/* default argument for some efuns */
object this_object();
/* used for implicit float/int conversions */
int to_int(string | float | int | buffer);
float to_float(string | float | int);

object clone_object(string, ...);
function bind(function, object);
object this_player(int default: 0);
object this_interactive this_player( int default: 1);
object this_user this_player( int default: 0);
mixed previous_object(int default: 0);
object *all_previous_objects previous_object(int default: -1);
mixed *call_stack(int default: 0);
int sizeof(mixed);
int strlen sizeof(string);
void destruct(object);
string file_name(object default: F_THIS_OBJECT);
string capitalize(string);
string *explode(string, string);
mixed implode(mixed *, string | function, void | mixed);
int call_out(string | function, int,...);
int member_array(mixed, string | mixed *, void | int);
int input_to(string | function,...);
int random(int);

object environment(void | object);
object *all_inventory(object default: F_THIS_OBJECT);
object *deep_inventory(object default: F_THIS_OBJECT);
object first_inventory(object|string default: F_THIS_OBJECT);
object next_inventory(object default: F_THIS_OBJECT);
void say(string, void | object | object *);
void tell_room(object | string, string | object | int | float, void | object *);
object present(object | string, void | object);
void move_object(object | string);

void add_action(string | function, string | string *, void | int);
string query_verb();
int command(string);
int remove_action(string, string);
int living (object default:F_THIS_OBJECT);
mixed *commands();
void disable_commands();
void enable_commands();
void set_living_name(string);
object *livings();
object find_living(string);
object find_player(string);
void notify_fail(string | function);

string lower_case(string);
string replace_string(string, string, string,...);
int restore_object(string, void | int);
int save_object(string, void | int);
string save_variable(mixed);
mixed restore_variable(string);
object *users();
mixed *get_dir(string, int default: 0);
int strsrch(string, string | int, int default: 0);

/* communication functions */

void write(mixed);
void tell_object(object, string);
void shout(string);
void receive(string);
void message(mixed, mixed, string | string * | object | object *,
	          void | object | object *);

/* the find_* functions */

object find_object(string, int default: 0);
object load_object find_object(string, int default: 1);
int find_call_out(int|string);

/* mapping functions */

mapping allocate_mapping(int);
mixed *values(mapping);
mixed *keys(mapping);
void map_delete(mapping, mixed);
mixed match_path(mapping, string);

/* all the *p() type functions */

int clonep(mixed default: F_THIS_OBJECT);
int intp(mixed);
int undefinedp(mixed);
int nullp undefinedp(mixed);
int floatp(mixed);
int stringp(mixed);
int virtualp(object default: F_THIS_OBJECT);
int functionp(mixed);
int pointerp(mixed);
int arrayp pointerp(mixed);
int objectp(mixed);
int classp(mixed);
string typeof(mixed);

#ifndef DISALLOW_BUFFER_TYPE
    int bufferp(mixed);
#endif

int inherits(string, object);
void replace_program(string);

#ifndef DISALLOW_BUFFER_TYPE
    buffer allocate_buffer(int);
#endif
mixed regexp(string | string *, string, void | int);
mixed *reg_assoc(string, string *, mixed *, mixed | void);
mixed *allocate(int);
mixed *call_out_info();

/* 32-bit cyclic redundancy code - see crc32.c and crctab.h */
int crc32(string | buffer);

/* commands operating on files */

#ifndef DISALLOW_BUFFER_TYPE
    mixed read_buffer(string | buffer, void | int, void | int);
#endif

int write_file(string, string, void | int);
int rename(string, string);
int write_bytes(string, int, string);

#ifndef DISALLOW_BUFFER_TYPE
    int write_buffer(string | buffer, int, string | buffer | int);
#endif

int file_size(string);
string read_bytes(string, void | int, void | int);
string read_file(string, void | int, void | int);
int cp(string, string);

    int link(string, string);
    int mkdir(string);
    int rm(string);
    int rmdir(string);

/* the bit string functions */

    string clear_bit(string, int);
    int test_bit(string, int);
    string set_bit(string, int);
    int next_bit(string, int);

    string crypt(string, string | int);
    string oldcrypt(string, string | int);
   
    string ctime(int);
    int exec(object, object);
    mixed *localtime(int);
    string function_exists(string, void | object, void | int);

    object *objects(void | string | function, void | object);
    string query_host_name();
    int query_idle(object default:F_THIS_OBJECT);
    string query_ip_name(void | object);
    string query_ip_number(void | object);
    object query_snoop(object default:F_THIS_OBJECT);
    object query_snooping(object default:F_THIS_OBJECT);
    int remove_call_out(int | void | string);
    void set_heart_beat(int);
    int query_heart_beat(object default:F_THIS_OBJECT);
    void set_hide(int);

    void set_reset(object, void | int);

    object snoop(object, void | object);
    mixed *sort_array(mixed *, int | string | function, ...);
    int tail(string);
    void throw(mixed);
    int time();
    mixed *unique_array(mixed *, string | function, void | mixed);
    mapping unique_mapping(mixed *, string | function, ...);
    string *deep_inherit_list(object default:F_THIS_OBJECT);
    string *shallow_inherit_list(object default:F_THIS_OBJECT);
    string *inherit_list shallow_inherit_list(object default:F_THIS_OBJECT);
    void printf(string,...);
    string sprintf(string,...);
    int mapp(mixed);
    mixed *stat(string, int default: 0);

/*
 * Object properties
 */
    int interactive(object default:F_THIS_OBJECT);
    string in_edit(object default:F_THIS_OBJECT);
    int in_input(object default:F_THIS_OBJECT);
    int userp(object default:F_THIS_OBJECT);

    void enable_wizard();
    void disable_wizard();
    int wizardp(object default:F_THIS_OBJECT);

    object master();

/*
 * various mudlib statistics
 */
    int memory_info(object | void);
    mixed get_config(int);

    int get_char(string | function,...);
    object *children(string);

    void reload_object(object);

    void error(string);
    int uptime();
    int strcmp(string, string);

    mapping rusage();

    void flush_messages(void | object);

    void ed(string | void, string | void, string | int | void, int | void);

#ifdef CACHE_STATS
    string cache_stats();
#endif

    mixed filter(mixed * | mapping, string | function, ...);
    mixed filter_array filter(mixed *, string | function, ...);
    mapping filter_mapping filter(mapping, string | function, ...);

    mixed map(string | mapping | mixed *, string | function, ...);
    mapping map_mapping map(mapping, string | function, ...);
    mixed *map_array map(mixed *, string | function, ...);
/*
 * parser 'magic' functions, turned into efuns
 */
    string malloc_status();
    string mud_status(int default: 0);
    void dumpallobj(string | void);

    string dump_file_descriptors();
    string query_load_average();

    string origin();

/* the infrequently used functions */

    int reclaim_objects();

    int set_eval_limit(int);
    int reset_eval_cost set_eval_limit(int default: 0);
    int eval_cost set_eval_limit(int default: -1);
    int max_eval_cost set_eval_limit(int default: 1);

#ifdef DEBUG_MACRO
    void set_debug_level(int);
#endif

#ifdef PROFILE_FUNCTIONS
    mapping *function_profile(object default:F_THIS_OBJECT);
#endif

    int resolve(string, string);

/* shutdown is at the end because it is only called once per boot cycle :) */
    void shutdown(void | int);

mixed query_notify_fail();
object *named_livings();
mixed copy(mixed);
string *functions(object, int default: 0);
string *variables(object, int default: 0);
object *heart_beats();
#ifdef COMPAT_32
object *heart_beat_info heart_beats();
#endif
int file_length(string);
string upper_case(string);
int replaceable(object, void | string *);
string program_info(void | object);
void store_variable(string, mixed);
mixed fetch_variable(string);
int remove_interactive(object default: F_THIS_OBJECT);
int query_ip_port(void | object);
void debug_message(string);
object function_owner(function);
string repeat_string(string, int);
mapping memory_summary();

    mixed debug_info(int, object);
    int refs(mixed);

/* dump_prog: disassembler... comment out this line if you don't want the
   disassembler compiled in.
*/
    void dump_prog(object,...);

#if defined(PROFILING) && defined(HAS_MONCONTROL)
    void moncontrol(int);
#endif

    float cos(float);
    float sin(float);
    float tan(float);
    float asin(float);
    float acos(float);
    float atan(float);
    float sqrt(float);
    float log(float);
    float pow(float, float);
    float exp(float);
    float floor(float);
    float ceil(float);


/*
 * socket efuns
 */
    int socket_create(int, string | function, string | function | void);
    int socket_bind(int, int);
    int socket_listen(int, string | function);
    int socket_accept(int, string | function, string | function);
    int socket_connect(int, string, string | function, string | function);
    int socket_write(int, mixed, string | void);
    int socket_close(int);
    int socket_release(int, object, string | function);
    int socket_acquire(int, string | function, string | function, string | function);
    string socket_error(int);
    string socket_address(int | object);
    string dump_socket_status();

int export_uid(object);
string geteuid(function | object default:F_THIS_OBJECT);
string getuid(object default:F_THIS_OBJECT);
int seteuid(string | int);

string strwrap (string, int, int|void);
