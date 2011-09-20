/**
 */
#ifndef _BYTECODE_OPS_H_
#define _BYTECODE_OPS_H_

enum eBC_Ops
{
	BC_OP_NOP,
	
	BC_OP_JUMP,
	BC_OP_JUMPIF,
	BC_OP_JUMPIFNOT,
	
	BC_OP_RETURN,	// = 4
	BC_OP_CALLFUNCTION,
	BC_OP_CALLMETHOD,
	BC_OP_CREATEOBJ,	
	
	BC_OP_LOADVAR,	// = 8
	BC_OP_SAVEVAR,

	BC_OP_LOADINT,	// = 10
	BC_OP_LOADREAL,
	BC_OP_LOADSTR,

	BC_OP_DUPSTACK,	// = 13
	BC_OP_CAST,	//
	
	BC_OP_ELEMENT,	// = 15
	BC_OP_INDEX,

	BC_OP_ENTERCONTEXT,	// = 17
	BC_OP_LEAVECONTEXT,
	BC_OP_DEFINEVAR,

	// Operations
	BC_OP_LOGICNOT,	// 20
	BC_OP_LOGICAND,
	BC_OP_LOGICOR,
	BC_OP_LOGICXOR,

	BC_OP_BITNOT,	// 24
	BC_OP_BITAND,
	BC_OP_BITOR,
	BC_OP_BITXOR,

	BC_OP_BITSHIFTLEFT,	// 28
	BC_OP_BITSHIFTRIGHT,
	BC_OP_BITROTATELEFT,

	BC_OP_NEG,	// 31
	BC_OP_ADD,
	BC_OP_SUBTRACT,
	BC_OP_MULTIPLY,
	BC_OP_DIVIDE,
	BC_OP_MODULO,

	BC_OP_EQUALS,	// 37
	BC_OP_NOTEQUALS,
	BC_OP_LESSTHAN,
	BC_OP_LESSTHANOREQUAL,
	BC_OP_GREATERTHAN,
	BC_OP_GREATERTHANOREQUAL
};

#endif