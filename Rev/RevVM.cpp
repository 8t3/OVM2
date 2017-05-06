
#include "RevVM.hpp"
#include "RevLAPI.hpp"
#include "RevLINT.hpp"
#include "../Lua/lobject.h"
#include "../Lua/lfunc.h"
#include "../Lua/lopcodes.h"
#include "../Lua/lgc.h"
#include <math.h>  

#pragma region RevVM Defines 
#define rev_setobj(rL, a, b) *(a) = *(b)
#define rev_setobjs2s setobj
#define rev_setobj2s setobj
#define rev_setbvalue(obj, x) { r_TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=R_LUA_TBOOLEAN; }
#define rev_setnilvalue(obj) ((obj)->tt=R_LUA_TNIL)
#define rev_setnvalue(obj,x) { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=R_LUA_TNUMBER; }
#define rev_sethvalue { TValue *i_o=(obj); i_o->value.gc=cast(GCObject *, (x)); i_o->tt=R_LUA_TTABLE; }
#define rev_vmtry(x) { __try{x;} __except(rev_luaV_vmcatch(GetExceptionINformation()){})}
#define runtime_check(L, c)	{ if (!(c)) break; }
#define RA(i)	(base+GETARG_A(i))
/* to be used after possible stack reallocation */
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))
#define KBx(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k+GETARG_Bx(i))
#define dojump(L,pc,i)	{(pc) += (i); luai_threadyield(L);}
#define Protect(x)      { rL->savedpc = pc; {x;}; base = rL->base; }
#define rev_luaC_barrier(rL,p,v) { if (valiswhite(v) && isblack(obj2gco(p)))  \
        luaC_barrierf(rL,obj2gco(p),gcvalue(v)); }
#pragma endregion

rblx::luaV_gettable r_luaV_gettable = (rblx::luaV_gettable)(rev_Offset(r_luaV_gettable_addy));
rblx::luaV_settable r_luaV_settable = (rblx::luaV_settable)(rev_Offset(r_luaV_settable_addy));
rblx::lua_setfield r_lua_setfield = (rblx::lua_setfield)(rev_Offset(r_lua_setfield_addy));

typedef TValue r_TValue;

DWORD rev_luaV_vmcatch(PEXCEPTION_POINTERS ExInfo)
{
	printf("[!] An error occured! Exception information:\n-> EIP: %x\n-> Error code: %d\n", ExInfo->ContextRecord->Eip, GetLastError());
	return EXCEPTION_EXECUTE_HANDLER;
}

r_Proto* rev_luaV_specproto(r_lua_State* rL, Proto* Specification)
{
	r_Proto* rp = rev_luaF_newproto((DWORD)rL);
	rp->is_vararg = Specification->is_vararg;
	rp->maxstacksize = Specification->maxstacksize;
	rp->numparams = Specification->numparams;
	return rp;
}

const TValue* rev_luaV_tonumber(const TValue *obj, TValue *n) {
	
	lua_Number number;
	if (obj->tt == R_LUA_TNUMBER) 
		return obj;

	setnvalue(n, number);
	return n;
	
	
}


void rev_Arith(r_lua_State* rL, TValue*  A, TValue* B, TValue* C, TMS t_op) {
	TValue tempb, tempc;
	const TValue *b, *c;
	if ((b = rev_luaV_tonumber(B, &tempb)) != NULL &&
		(c = rev_luaV_tonumber(C, &tempc)) != NULL) {
		LUA_NUMBER nb = check_exp(ttisnumber(b), (b)->value.n);
		LUA_NUMBER nc = check_exp(ttisnumber(c), (c)->value.n);
		switch (t_op) {
		case TM_ADD: 
			rev_setnvalue(A, rev_luai_numadd(nb, nc));
			break;
		case TM_SUB: 
			rev_setnvalue(A, rev_luai_numsub(nb, nc));
			break;
		case TM_MUL: 
			rev_setnvalue(A, rev_luai_nummul(nb, nc));
			break;
		case TM_DIV: 
			rev_setnvalue(A, rev_luai_numdiv(nb, nc));
			break;
		case TM_MOD: 
			rev_setnvalue(A, rev_luai_nummod(nb, nc));
			break;
		case TM_POW: 
			rev_setnvalue(A, rev_luai_numpow(nb, nc));
			break;
		case TM_UNM: 
			rev_setnvalue(A, rev_luai_numunm(nb));
			break;
		default: 
			((void)0); 
			break;
		}
	}
}

void rev_luaV_execute(r_lua_State* rL, lua_State * L, int nexeccalls)
{
	LClosure *cl;
	StkId base;
	TValue *k;
	const Instruction *pc;
reentry:
	pc = rL->savedpc;
	cl = (LClosure*)rL->ci->func->value.gc;
	base = rL->base;
	k = cl->p->k;
	while(1) {
		const Instruction i = *pc++;
		StkId ra;
		ra = RA(i);
		switch (GET_OPCODE(i)) {
			case OP_MOVE: {
				rev_setobjs2s(rL, ra, RB(i));
				continue;
			}
			case OP_LOADK: {
				rev_setobj2s(rL, ra, KBx(i));
				continue;
			}
			case OP_LOADBOOL: {
				rev_setbvalue(ra, GETARG_B(i));
				if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
				continue;
			}
			case OP_LOADNIL: {
				TValue *rb = RB(i);
				do {
					rev_setnilvalue(rb--);
				} while (rb >= ra);
				continue;
			}
			case OP_GETUPVAL: {
				int b = GETARG_B(i);
				rev_setobj2s(rL, ra, cl->upvals[b]->v);
				continue;
			}
			case OP_SETUPVAL: {
				UpVal *uval = cl->upvals[GETARG_B(i)];
				rev_setobj(rL, uval->v, ra);
				continue;
			}
			case OP_JMP: {
				dojump(rL, pc, GETARG_sBx(i));
				continue;
			}
			case OP_GETTABLE: {
				//we have to use rblxs func for these, dont think we can ref our own
				Protect(r_luaV_gettable((DWORD)rL, (DWORD)RB(i), (DWORD)RKC(i), (DWORD)ra));
				continue;
			}
			case OP_SETTABLE: {
				Protect(r_luaV_settable((DWORD)rL, (DWORD)RB(i), (DWORD)RKC(i), (DWORD)ra));
				continue;
			}

			case OP_ADD: {
				TValue *rb = RKB(i); 
				TValue *rc = RKC(i); 
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_numadd(nb, nc));
				}
				else 
					Protect(rev_Arith(rL, ra, rb, rc, TM_ADD));
				
				continue;
			}
			case OP_SUB: {
				TValue *rb = RKB(i);
				TValue *rc = RKC(i);
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_numsub(nb, nc));
				}
				else
					Protect(rev_Arith(rL, ra, rb, rc, TM_SUB));

				continue;
			}
			case OP_MUL: {
				TValue *rb = RKB(i);
				TValue *rc = RKC(i);
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_nummul(nb, nc));
				}
				else
					Protect(rev_Arith(rL, ra, rb, rc, TM_MUL));

				continue;
			}
			case OP_DIV: {
				TValue *rb = RKB(i);
				TValue *rc = RKC(i);
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_numdiv(nb, nc));
				}
				else
					Protect(rev_Arith(rL, ra, rb, rc, TM_DIV));

				continue;
			}
			case OP_POW: {
				TValue *rb = RKB(i);
				TValue *rc = RKC(i);
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_numpow(nb, nc));
				}
				else
					Protect(rev_Arith(rL, ra, rb, rc, TM_POW));

				continue;
			}
			case OP_UNM: {
				TValue *rb = RKB(i);
				TValue *rc = RKC(i);
				if (R_LUA_TNUMBER == (DWORD)rb && R_LUA_TNUMBER == (DWORD)rc) {
					LUA_NUMBER nb = check_exp(ttisnumber(rb), (rb)->value.n);
					LUA_NUMBER nc = check_exp(ttisnumber(rc), (rc)->value.n);
					rev_setnvalue(ra, rev_luai_numunm(nb, nc));
				}
				else
					Protect(rev_Arith(rL, ra, rb, rc, TM_UNM));

				continue;
			}
		}
	}
}
