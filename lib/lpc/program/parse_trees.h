#pragma once

#define NODES_PER_BLOCK         256

enum node_type {
    NODE_RETURN, NODE_TWO_VALUES, NODE_OPCODE, NODE_OPCODE_1, NODE_OPCODE_2,
    NODE_UNARY_OP, NODE_UNARY_OP_1, NODE_BINARY_OP, NODE_BINARY_OP_1,
    NODE_TERNARY_OP, NODE_TERNARY_OP_1, NODE_CONTROL_JUMP, NODE_LOOP,
    NODE_CALL, NODE_CALL_1, NODE_CALL_2, NODE_LAND_LOR, NODE_FOREACH,
    NODE_LVALUE_EFUN, NODE_SWITCH_RANGES, NODE_SWITCH_STRINGS, 
    NODE_SWITCH_DIRECT, NODE_SWITCH_NUMBERS, NODE_CASE_NUMBER,
    NODE_CASE_STRING, NODE_DEFAULT, NODE_IF, NODE_BRANCH_LINK, NODE_PARAMETER,
    NODE_PARAMETER_LVALUE, NODE_EFUN, NODE_ANON_FUNC, NODE_REAL, NODE_NUMBER,
    NODE_STRING, NODE_FUNCTION_CONSTRUCTOR, NODE_CATCH, NODE_TIME_EXPRESSION
};

enum control_jump_type {
    CJ_BREAK_SWITCH = 2, CJ_BREAK = 0, CJ_CONTINUE = 1
};

union parse_value {
    int number;
    double real;
    struct parse_node_s *expr;
};

typedef struct parse_node_s {
    short kind;
    short line;
    char type;
    union parse_value v, l, r; /* left, right, and value */
} parse_node_t;

typedef struct parse_node_block_s {
    struct parse_node_block_s *next;
    parse_node_t nodes[NODES_PER_BLOCK];
} parse_node_block_t;

#define IS_NODE(vn, nt, op) ((vn)->kind == nt && (vn)->v.number == op)

#define CREATE_TERNARY_OP(vn, op, t, x, y, z) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_TERNARY_OP;\
	INT_CREATE_TERNARY_OP(vn, op, t, x, y, z);\
	} while(0)

#define INT_CREATE_TERNARY_OP(vn, op, t, x, y, z) do {\
	(vn)->l.expr = (x);\
	(vn)->type = t;\
	CREATE_BINARY_OP((vn)->r.expr, op, t, y, z);\
	} while(0)

#define CREATE_BINARY_OP(vn, op, t, x, y) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_BINARY_OP;\
	INT_CREATE_BINARY_OP(vn, op, t, x, y);\
	} while(0)

#define INT_CREATE_BINARY_OP(vn, op, t, x, y) do {\
	INT_CREATE_UNARY_OP(vn, op, t, y);\
	(vn)->l.expr = x;\
	} while(0)

#define CREATE_UNARY_OP(vn, op, t, x) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_UNARY_OP;\
	INT_CREATE_UNARY_OP(vn, op, t, x);\
	} while(0)

#define INT_CREATE_UNARY_OP(vn, op, t, x) do {\
	INT_CREATE_OPCODE(vn, op, t);\
	(vn)->r.expr = x;\
	} while(0)

#define CREATE_OPCODE(vn, op, t) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_OPCODE;\
	INT_CREATE_OPCODE(vn, op, t);\
	} while(0)

#define INT_CREATE_OPCODE(vn, op, t) do {\
	(vn)->v.number = op;\
	(vn)->type = t;\
	} while(0)

#define CREATE_OPCODE_1(vn, op, t, p) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_OPCODE_1;\
	INT_CREATE_OPCODE(vn, op, t);\
	(vn)->l.number = p;\
	} while(0)

#define CREATE_OPCODE_2(vn, op, t, p1, p2) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_OPCODE_2;\
	INT_CREATE_OPCODE(vn, op, t);\
	(vn)->l.number = p1;\
	(vn)->r.number = p2;\
	} while(0)

#define CREATE_UNARY_OP_1(vn, op, t, x, p) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_UNARY_OP_1;\
	INT_CREATE_UNARY_OP(vn, op, t, x);\
	(vn)->l.number = p;\
	} while(0)

#define CREATE_BINARY_OP_1(vn, op, t, x, y, p) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_BINARY_OP_1;\
	INT_CREATE_BINARY_OP(vn, op, t, x, y);\
	(vn)->type = p;\
	} while(0)

#define CREATE_TERNARY_OP_1(vn, op, t, x, y, z, p) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_TERNARY_OP_1;\
	INT_CREATE_TERNARY_OP(vn, op, t, x, y, z);\
	(vn)->r.expr->type = p;\
	} while(0)

#define CREATE_RETURN(vn, val) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_RETURN;\
	(vn)->r.expr = val;\
	} while(0)

#define CREATE_LAND_LOR(vn, op, x, y) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_LAND_LOR;\
	(vn)->v.number = op;\
	(vn)->l.expr = x;\
	(vn)->r.expr = y;\
	(vn)->type = ((x->type == y->type) ? x->type : TYPE_ANY);\
	} while(0)

#define CREATE_CALL(vn, op, t, el) do {\
	(vn) = el;\
	(vn)->kind = NODE_CALL;\
	(vn)->l.number = (vn)->v.number;\
	(vn)->v.number = op;\
	(vn)->type = t;\
	} while(0)

#define CREATE_STATEMENTS(vn, ln, rn) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_TWO_VALUES;\
	(vn)->l.expr = ln;\
	(vn)->r.expr = rn;\
	} while(0)

#define CREATE_TWO_VALUES(vn, t, ln, rn) do {\
	CREATE_STATEMENTS(vn, ln, rn);\
	(vn)->type = t;\
	} while(0)

#define CREATE_CONTROL_JUMP(vn, op) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_CONTROL_JUMP;\
	(vn)->v.number = op;\
	} while(0)

#define CREATE_PARAMETER(vn, t, p) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_PARAMETER;\
	(vn)->type = t;\
	(vn)->v.number = p;\
	} while(0)

#define CREATE_IF(vn, c, s, e) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_IF;\
	(vn)->v.expr = c;\
	(vn)->l.expr = s;\
	(vn)->r.expr = (e);\
	} while(0)

#define CREATE_LOOP(vn, tf, b, i, t) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_LOOP;\
	(vn)->type = tf;\
	(vn)->v.expr = b;\
	(vn)->l.expr = i;\
	(vn)->r.expr = t;\
	} while(0)

#define CREATE_LVALUE_EFUN(vn, t, lvl) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_LVALUE_EFUN;\
	(vn)->r.expr = lvl;\
	(vn)->type = t;\
	} while(0)

#define CREATE_FOREACH(vn, ln, rn) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_FOREACH;\
	(vn)->l.expr = ln;\
	(vn)->r.expr = rn;\
	} while(0)

#define CREATE_ERROR(vn) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_NUMBER;\
	(vn)->type = TYPE_ANY;\
	} while(0)

#define CREATE_REAL(vn, val) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_REAL;\
	(vn)->type = TYPE_REAL;\
	(vn)->v.real = val;\
	} while(0)

#define CREATE_NUMBER(vn, val) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_NUMBER;\
	(vn)->type = (val ? TYPE_NUMBER : TYPE_ANY);\
	(vn)->v.number = val;\
	} while(0)

#define CREATE_STRING(vn, val) do {\
	(vn) = new_node_no_line();\
	(vn)->kind = NODE_STRING;\
	(vn)->type = TYPE_STRING;\
	(vn)->v.number = store_prog_string(val);\
	} while(0)

#define CREATE_EXPR_LIST(vn, pn) do {\
	(vn) = new_node();\
	(vn)->v.number = (pn ? ((parse_node_t *)pn)->kind : 0);\
	(vn)->l.expr = (pn ? ((parse_node_t *)pn)->l.expr : (vn));\
	(vn)->r.expr = pn;\
	} while(0)

#define CREATE_EXPR_NODE(vn, pn, f) do {\
	(vn) = new_node_no_line();\
	(vn)->v.expr = pn;\
	(vn)->l.expr = vn;\
	(vn)->r.expr = 0;\
	(vn)->type = f;\
	} while(0)

#define CREATE_CATCH(vn, pn) do {\
	(vn) = new_node();\
	(vn)->kind = NODE_CATCH;\
	(vn)->type = TYPE_ANY;\
	(vn)->r.expr = pn;\
	} while(0)

#define CREATE_TIME_EXPRESSION(vn, pn) do {\
	(vn) = new_node();\
        (vn)->kind = NODE_TIME_EXPRESSION;\
        (vn)->type = TYPE_ANY;\
        (vn)->r.expr = pn;\
        } while(0)

#define NODE_NO_LINE(x,y) do {\
	(x) = new_node_no_line();\
	(x)->kind = y;\
	} while(0)

/* tree functions */
void free_tree(void);
void release_tree(void);
void lock_expressions(void);
void unlock_expressions(void);
/* node functions */
parse_node_t *new_node(void);
parse_node_t *new_node_no_line(void);
parse_node_t *make_branched_node(short, char, parse_node_t *, parse_node_t *);
/* parser grammar functions */
parse_node_t *binary_int_op(parse_node_t *, parse_node_t *, char, char *);
parse_node_t *make_range_node(int, parse_node_t *, parse_node_t *, parse_node_t *);
parse_node_t *insert_pop_value(parse_node_t *);
parse_node_t *optimize_loop_test(parse_node_t *);
int is_boolean(parse_node_t *);
