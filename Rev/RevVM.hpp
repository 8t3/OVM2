#pragma once

#include "RevBase.hpp"
#define rev_luai_numadd(a,b)        ((a)+(b))
#define rev_luai_numsub(a,b)        ((a)-(b))
#define rev_luai_nummul(a,b)        ((a)*(b))
#define rev_luai_numdiv(a,b)        ((a)/(b))
#define rev_luai_nummod(a,b)        ((a) - floor((a)/(b))*(b))
#define rev_luai_numpow(a,b)        (pow(a,b))
#define rev_luai_numunm(a)          (-(a))
#define rev_luai_numeq(a,b)         ((a)==(b))
#define rev_luai_numlt(a,b)         ((a)<(b))
#define rev_luai_numle(a,b)         ((a)<=(b))
#define rev_luai_numisnan(a)        (!rev_luai_numeq((a), (a)))
void rev_luaV_execute(r_lua_State rL, lua_State *L, int nexeccalls);

//Enum TMS declare, IDS depend on order do not change
typedef enum {
	TM_INDEX,
	TM_NEWINDEX,
	TM_GC,
	TM_MODE,
	TM_EQ, 
	TM_ADD,
	TM_SUB,
	TM_MUL,
	TM_DIV,
	TM_MOD,
	TM_POW,
	TM_UNM,
	TM_LEN,
	TM_LT,
	TM_LE,
	TM_CONCAT,
	TM_CALL,
	TM_N       
} TMS;
