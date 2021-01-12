/*
** $Id: lopcodes.h $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h

#include "llimits.h"


/*===========================================================================
  We assume that instructions are unsigned 32-bit integers.
  All instructions have an opcode in the first 7 bits.
  Instructions can have the following formats:

        3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
        1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
iABC          C(8)     |      B(8)     |k|     A(8)      |   Op(7)     |
iABx                Bx(17)               |     A(8)      |   Op(7)     |
iAsBx              sBx (signed)(17)      |     A(8)      |   Op(7)     |
iAx                           Ax(25)                     |   Op(7)     |
isJ                           sJ(25)                     |   Op(7)     |

  A signed argument is represented in excess K: the represented value is
  the written unsigned value minus K, where K is half the maximum for the
  corresponding unsigned argument.
===========================================================================*/


enum OpMode {iABC, iABx, iAsBx, iAx, isJ};  /* basic instruction formats */

/*
** R[x] - register
** K[x] - constant (in constant table)
** RK(x) == if k(i) then K[x] else R[x]
*/


/*
** grep "ORDER OP" if you change these enums
*/

enum OpCode {
/*----------------------------------------------------------------------
  name		args	description
------------------------------------------------------------------------*/
OP_MOVE,/*	A B	R[A] := R[B]					*/
OP_LOADI,/*	A sBx	R[A] := sBx					*/
OP_LOADF,/*	A sBx	R[A] := (lua_Number)sBx				*/
OP_LOADK,/*	A Bx	R[A] := K[Bx]					*/
OP_LOADKX,/*	A	R[A] := K[extra arg]				*/
OP_LOADFALSE,/*	A	R[A] := false					*/
OP_LFALSESKIP,/*A	R[A] := false; pc++				*/
OP_LOADTRUE,/*	A	R[A] := true					*/
OP_LOADNIL,/*	A B	R[A], R[A+1], ..., R[A+B] := nil		*/
OP_GETUPVAL,/*	A B	R[A] := UpValue[B]				*/
OP_SETUPVAL,/*	A B	UpValue[B] := R[A]				*/

OP_GETTABUP,/*	A B C	R[A] := UpValue[B][K[C]:string]			*/
OP_GETTABLE,/*	A B C	R[A] := R[B][R[C]]				*/
OP_GETI,/*	A B C	R[A] := R[B][C]					*/
OP_GETFIELD,/*	A B C	R[A] := R[B][K[C]:string]			*/

OP_SETTABUP,/*	A B C	UpValue[A][K[B]:string] := RK(C)		*/
OP_SETTABLE,/*	A B C	R[A][R[B]] := RK(C)				*/
OP_SETI,/*	A B C	R[A][B] := RK(C)				*/
OP_SETFIELD,/*	A B C	R[A][K[B]:string] := RK(C)			*/

OP_NEWTABLE,/*	A B C k	R[A] := {}					*/

OP_SELF,/*	A B C	R[A+1] := R[B]; R[A] := R[B][RK(C):string]	*/

OP_ADDI,/*	A B sC	R[A] := R[B] + sC				*/

OP_ADDK,/*	A B C	R[A] := R[B] + K[C]				*/
OP_SUBK,/*	A B C	R[A] := R[B] - K[C]				*/
OP_MULK,/*	A B C	R[A] := R[B] * K[C]				*/
OP_MODK,/*	A B C	R[A] := R[B] % K[C]				*/
OP_POWK,/*	A B C	R[A] := R[B] ^ K[C]				*/
OP_DIVK,/*	A B C	R[A] := R[B] / K[C]				*/
OP_IDIVK,/*	A B C	R[A] := R[B] // K[C]				*/

OP_BANDK,/*	A B C	R[A] := R[B] & K[C]:integer			*/
OP_BORK,/*	A B C	R[A] := R[B] | K[C]:integer			*/
OP_BXORK,/*	A B C	R[A] := R[B] ~ K[C]:integer			*/

OP_SHRI,/*	A B sC	R[A] := R[B] >> sC				*/
OP_SHLI,/*	A B sC	R[A] := sC << R[B]				*/

OP_ADD,/*	A B C	R[A] := R[B] + R[C]				*/
OP_SUB,/*	A B C	R[A] := R[B] - R[C]				*/
OP_MUL,/*	A B C	R[A] := R[B] * R[C]				*/
OP_MOD,/*	A B C	R[A] := R[B] % R[C]				*/
OP_POW,/*	A B C	R[A] := R[B] ^ R[C]				*/
OP_DIV,/*	A B C	R[A] := R[B] / R[C]				*/
OP_IDIV,/*	A B C	R[A] := R[B] // R[C]				*/

OP_BAND,/*	A B C	R[A] := R[B] & R[C]				*/
OP_BOR,/*	A B C	R[A] := R[B] | R[C]				*/
OP_BXOR,/*	A B C	R[A] := R[B] ~ R[C]				*/
OP_SHL,/*	A B C	R[A] := R[B] << R[C]				*/
OP_SHR,/*	A B C	R[A] := R[B] >> R[C]				*/

OP_MMBIN,/*	A B C	call C metamethod over R[A] and R[B]		*/
OP_MMBINI,/*	A sB C k	call C metamethod over R[A] and sB	*/
OP_MMBINK,/*	A B C k		call C metamethod over R[A] and K[B]	*/

OP_UNM,/*	A B	R[A] := -R[B]					*/
OP_BNOT,/*	A B	R[A] := ~R[B]					*/
OP_NOT,/*	A B	R[A] := not R[B]				*/
OP_LEN,/*	A B	R[A] := #R[B] (length operator)			*/

OP_CONCAT,/*	A B	R[A] := R[A].. ... ..R[A + B - 1]		*/

OP_CLOSE,/*	A	close all upvalues >= R[A]			*/
OP_TBC,/*	A	mark variable A "to be closed"			*/
OP_JMP,/*	sJ	pc += sJ					*/
OP_EQ,/*	A B k	if ((R[A] == R[B]) ~= k) then pc++		*/
OP_LT,/*	A B k	if ((R[A] <  R[B]) ~= k) then pc++		*/
OP_LE,/*	A B k	if ((R[A] <= R[B]) ~= k) then pc++		*/

OP_EQK,/*	A B k	if ((R[A] == K[B]) ~= k) then pc++		*/
OP_EQI,/*	A sB k	if ((R[A] == sB) ~= k) then pc++		*/
OP_LTI,/*	A sB k	if ((R[A] < sB) ~= k) then pc++			*/
OP_LEI,/*	A sB k	if ((R[A] <= sB) ~= k) then pc++		*/
OP_GTI,/*	A sB k	if ((R[A] > sB) ~= k) then pc++			*/
OP_GEI,/*	A sB k	if ((R[A] >= sB) ~= k) then pc++		*/

OP_TEST,/*	A k	if (not R[A] == k) then pc++			*/
OP_TESTSET,/*	A B k	if (not R[B] == k) then pc++ else R[A] := R[B]	*/

OP_CALL,/*	A B C	R[A], ... ,R[A+C-2] := R[A](R[A+1], ... ,R[A+B-1]) */
OP_TAILCALL,/*	A B C k	return R[A](R[A+1], ... ,R[A+B-1])		*/

OP_RETURN,/*	A B C k	return R[A], ... ,R[A+B-2]	(see note)	*/
OP_RETURN0,/*		return						*/
OP_RETURN1,/*	A	return R[A]					*/

OP_FORLOOP,/*	A Bx	update counters; if loop continues then pc-=Bx; */
OP_FORPREP,/*	A Bx	<check values and prepare counters>;
                        if not to run then pc+=Bx+1;			*/

OP_TFORPREP,/*	A Bx	create upvalue for R[A + 3]; pc+=Bx		*/
OP_TFORCALL,/*	A C	R[A+4], ... ,R[A+3+C] := R[A](R[A+1], R[A+2]);	*/
OP_TFORLOOP,/*	A Bx	if R[A+2] ~= nil then { R[A]=R[A+2]; pc -= Bx }	*/

OP_SETLIST,/*	A B C k	R[A][C+i] := R[A+i], 1 <= i <= B		*/

OP_CLOSURE,/*	A Bx	R[A] := closure(KPROTO[Bx])			*/

OP_VARARG,/*	A C	R[A], R[A+1], ..., R[A+C-2] = vararg		*/

OP_VARARGPREP,/*A	(adjust vararg parameters)			*/

OP_EXTRAARG/*	Ax	extra (larger) argument for previous opcode	*/
};


constexpr int NUM_OPCODES = (int)(OP_EXTRAARG) + 1;


/*===========================================================================
  Notes:
  (*) In OP_CALL, if (B == 0) then B = top - A. If (C == 0), then
  'top' is set to last_result+1, so next open instruction (OP_CALL,
  OP_RETURN*, OP_SETLIST) may use 'top'.

  (*) In OP_VARARG, if (C == 0) then use actual number of varargs and
  set top (like in OP_CALL with C == 0).

  (*) In OP_RETURN, if (B == 0) then return up to 'top'.

  (*) In OP_LOADKX and OP_NEWTABLE, the next instruction is always
  OP_EXTRAARG.

  (*) In OP_SETLIST, if (B == 0) then real B = 'top'; if k, then
  real C = EXTRAARG _ C (the bits of EXTRAARG concatenated with the
  bits of C).

  (*) In OP_NEWTABLE, B is log2 of the hash size (which is always a
  power of 2) plus 1, or zero for size zero. If not k, the array size
  is C. Otherwise, the array size is EXTRAARG _ C.

  (*) For comparisons, k specifies what condition the test should accept
  (true or false).

  (*) In OP_MMBINI/OP_MMBINK, k means the arguments were flipped
   (the constant is the first operand).

  (*) All 'skips' (pc++) assume that next instruction is a jump.

  (*) In instructions OP_RETURN/OP_TAILCALL, 'k' specifies that the
  function builds upvalues, which may need to be closed. C > 0 means
  the function is vararg, so that its 'func' must be corrected before
  returning; in this case, (C - 1) is its number of fixed parameters.

  (*) In comparisons with an immediate operand, C signals whether the
  original operand was a float. (It must be corrected in case of
  metamethods.)

===========================================================================*/

/*
** masks for instruction properties. The format is:
** bits 0-2: op mode
** bit 3: instruction set register A
** bit 4: operator is a test (next instruction must be a jump)
** bit 5: instruction uses 'L->top' set by previous instruction (when B == 0)
** bit 6: instruction sets 'L->top' for next instruction (when C == 0)
** bit 7: instruction is an MM instruction (call a metamethod)
*/

LUAI_DDEC(const lu_byte luaP_opmodes[NUM_OPCODES];)

inline OpMode getOpMode(int m) { return cast(OpMode, luaP_opmodes[m] & 7); }
inline bool testAMode(int m)   { return luaP_opmodes[m] & (1 << 3); }
inline bool testTMode(int m)   { return luaP_opmodes[m] & (1 << 4); }
inline bool testITMode(int m)  { return luaP_opmodes[m] & (1 << 5); }
inline bool testOTMode(int m)  { return luaP_opmodes[m] & (1 << 6); }
inline bool testMMMode(int m)  { return luaP_opmodes[m] & (1 << 7); }


/*
** size and position of opcode arguments.
*/
constexpr int SIZE_C  = 8;
constexpr int SIZE_B  = 8;
constexpr int SIZE_Bx = SIZE_C + SIZE_B + 1;
constexpr int SIZE_A  = 8;
constexpr int SIZE_Ax = SIZE_Bx + SIZE_A;
constexpr int SIZE_sJ = SIZE_Bx + SIZE_A;

constexpr int SIZE_OP = 7;
constexpr int POS_OP  = 0;

constexpr int POS_A = POS_OP + SIZE_OP;
constexpr int POS_k = POS_A + SIZE_A;
constexpr int POS_B = POS_k + 1;
constexpr int POS_C = POS_B + SIZE_B;

constexpr int POS_Bx = POS_k;
constexpr int POS_Ax = POS_A;
constexpr int POS_sJ = POS_A;


/*
** limits for opcode arguments.
** we use (signed) 'int' to manipulate most arguments,
** so they must fit in ints.
*/

/* Check whether type 'int' has at least 'b' bits ('b' < 32) */
constexpr inline bool L_INTHASBITS(int b) {
  return (UINT_MAX >> (b - 1)) >= 1;
}

constexpr int MAXARG_Bx = L_INTHASBITS(SIZE_Bx) ? ((1 << SIZE_Bx) - 1) : MAX_INT;
constexpr int MAXARG_Ax = L_INTHASBITS(SIZE_Ax) ? ((1 << SIZE_Ax) - 1) : MAX_INT;
constexpr int MAXARG_sJ = L_INTHASBITS(SIZE_sJ) ? ((1 << SIZE_sJ) - 1) : MAX_INT;

constexpr int MAXARG_A  = (1 << SIZE_A) - 1;
constexpr int MAXARG_B  = (1 << SIZE_B) - 1;
constexpr int MAXARG_C  = (1 << SIZE_C) - 1;

constexpr int OFFSET_sC  = MAXARG_C >> 1;
constexpr int OFFSET_sBx = MAXARG_Bx >> 1;         /* 'sBx' is signed */
constexpr int OFFSET_sJ  = MAXARG_sJ >> 1;

inline int int2sC(int i) { return i + OFFSET_sC; }
inline int sC2int(int i) { return i - OFFSET_sC; }


/* creates a mask with 'n' 1 bits at position 'p' */
constexpr Instruction MASK1(int n, int p) {
  return (~((~(Instruction)0) << n)) << p;
}

/* creates a mask with 'n' 0 bits at position 'p' */
constexpr Instruction MASK0(int n, int p) {
  return ~MASK1(n, p);
}

/*
** the following macros help to manipulate instructions
*/

inline OpCode GET_OPCODE(Instruction i) {
  return cast(OpCode, (i >> POS_OP) & MASK1(SIZE_OP, 0));
}

inline void SET_OPCODE(Instruction &i, OpCode o) {
  i = (i & MASK0(SIZE_OP, POS_OP)) | ((cast(Instruction, o) << POS_OP) & MASK1(SIZE_OP, POS_OP));
}

inline bool checkopm(Instruction i, OpMode m) {
  return getOpMode(GET_OPCODE(i)) == m;
}

inline int getarg(Instruction i, int pos, int size) {
  return cast_int((i >> pos) & MASK1(size,0));
}

inline void setarg(Instruction &i, int v, int pos, int size) {
  i = (i & MASK0(size, pos)) | ((cast(Instruction, v) << pos) & MASK1(size, pos));
}

inline int GETARG_A(Instruction i) { return getarg(i, POS_A, SIZE_A); }
inline void SETARG_A(Instruction &i, int v) { setarg(i, v, POS_A, SIZE_A); }

inline int GETARG_B(Instruction i) { return check_exp(checkopm(i, iABC), getarg(i, POS_B, SIZE_B)); }
inline int GETARG_sB(Instruction i) { return sC2int(GETARG_B(i)); }
inline void SETARG_B(Instruction &i, int v) { setarg(i, v, POS_B, SIZE_B); }

inline int GETARG_C(Instruction i) { return check_exp(checkopm(i, iABC), getarg(i, POS_C, SIZE_C)); }
inline int GETARG_sC(Instruction i) { return sC2int(GETARG_C(i)); }
inline void SETARG_C(Instruction &i,int v) { setarg(i, v, POS_C, SIZE_C); }

inline int TESTARG_k(Instruction i) { return check_exp(checkopm(i, iABC), cast_int(i & (1u << POS_k))); }
inline int GETARG_k(Instruction i) { return check_exp(checkopm(i, iABC), getarg(i, POS_k, 1)); }
inline void SETARG_k(Instruction &i, int v) { setarg(i, v, POS_k, 1); }

inline int GETARG_Bx(Instruction i) { return check_exp(checkopm(i, iABx), getarg(i, POS_Bx, SIZE_Bx)); }
inline void SETARG_Bx(Instruction &i, int v) { return setarg(i, v, POS_Bx, SIZE_Bx); }

inline int GETARG_Ax(Instruction i) { return check_exp(checkopm(i, iAx), getarg(i, POS_Ax, SIZE_Ax)); }
inline void SETARG_Ax(Instruction &i, int v) { setarg(i, v, POS_Ax, SIZE_Ax); }

inline int GETARG_sBx(Instruction i) {
  return check_exp(checkopm(i, iAsBx), getarg(i, POS_Bx, SIZE_Bx) - OFFSET_sBx);
}

inline void SETARG_sBx(Instruction &i, int b) { SETARG_Bx(i, cast_uint(b + OFFSET_sBx)); }

inline int GETARG_sJ(Instruction i) {
  return check_exp(checkopm(i, isJ), getarg(i, POS_sJ, SIZE_sJ) - OFFSET_sJ);
}

inline void SETARG_sJ(Instruction &i, int j) { setarg(i, cast_uint((j)+OFFSET_sJ), POS_sJ, SIZE_sJ); }

inline Instruction CREATE_ABCk(int o, int a, int b, int c, int k) {
  return ((cast(Instruction, o) << POS_OP)
	  | (cast(Instruction, a) << POS_A)
	  | (cast(Instruction, b) << POS_B)
	  | (cast(Instruction, c) << POS_C)
	  | (cast(Instruction, k) << POS_k));
}

inline Instruction CREATE_ABx(int o, int a, int bc) {
  return ((cast(Instruction, o) << POS_OP)
	  | (cast(Instruction, a) << POS_A)
	  | (cast(Instruction, bc) << POS_Bx));
}

inline Instruction CREATE_Ax(int o, int a) {
  return ((cast(Instruction, o) << POS_OP)
	  | (cast(Instruction, a) << POS_Ax));
}

inline Instruction CREATE_sJ(int o, int j, int k) {
  return ((cast(Instruction, o) << POS_OP)
	  | (cast(Instruction, j) << POS_sJ)
	  | (cast(Instruction, k) << POS_k));
}


/* "out top" (set top for next instruction) */
inline bool isOT(Instruction i) {
  return (testOTMode(GET_OPCODE(i)) && GETARG_C(i) == 0) ||
    GET_OPCODE(i) == OP_TAILCALL;
}

/* "in top" (uses top from previous instruction) */
inline bool isIT(Instruction i) {
  return testITMode(GET_OPCODE(i)) && GETARG_B(i) == 0;
}



#if !defined(MAXINDEXRK)  /* (for debugging only) */
constexpr int MAXINDEXRK = MAXARG_B;
#endif

/*
** invalid register that fits in 8 bits
*/
constexpr int NO_REG = MAXARG_A;

/* number of list items to accumulate before a SETLIST instruction */
constexpr int LFIELDS_PER_FLUSH = 50;

#endif
