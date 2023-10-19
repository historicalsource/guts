/*
 * $Id: fexcp.c,v 1.17 1997/10/06 17:21:49 shepperd Exp $
 *
 *		Copyright 1995 Time Warner Interactive, Inc.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 * This function is a stab at doing some very simple floating point exception
 * handling for the R3K processor (3081). At this time, it only provides for
 * placing a (single precision) 0.0 in the destination register on operations
 * that would have otherwise underflowed. All other floating exceptions result
 * in what amounts to a prc_panic or a callback to the stub to report the
 * exception to gdb.
 */

#include <config.h>
#include <os_proto.h>
#include <intvecs.h>
#include <string.h>
#include <phx_proto.h>

#if HOST_BOARD_CLASS && ((HOST_BOARD_CLASS&HOST_BOARD) == PHOENIX) 
# define HB_PHOENIX 1
#endif

#if (HOST_BOARD == HCR4K) || HB_PHOENIX
extern U32 prc_get_ipl(void);
#endif

#if 0
#define TEST_IT		1		/* set this to simply test the code */
#define SQUAWK		1		/* set this to txt_str the results */
#define DO_IT_TOO	1		/* set this to test it and to do it */
#define LOOP_ON_ERROR	1		/* loop so we can see what's happening */
#endif

#define EMULATE_JUMP	1		/* handle FP instructions in delay slots */

#if !TEST_IT
# if defined(SQUAWK)
#  undef SQUAWK
# endif
#endif

#if SQUAWK
# include <nsprintf.h>
#endif

/*
**     	FLOATING POINT EXCEPTION BITS in Control/Status Register 	
*/

#define	ExINEXACT 	0x1000
#define	ExUNDERFLOW 	0x2000
#define ExOVERFLOW	0x4000
#define ExZERODIV	0x8000 
#define ExINVALID	0x10000
#define ExUNIMPL	0x20000
#define AllExceptions   0x3F000		/* all Exceptions in fp csr   */

/*
**     	FLOATING POINT TRAP ENABLE BITS in Control/Status Register 	
*/

#define	TrINEXACT 	0x080
#define	TrUNDERFLOW 	0x100
#define TrOVERFLOW	0x200 
#define TrZERODIV	0x400 
#define TrINVALID	0x800
#define EnableAll       0xF80	/* all Traps in fp csr              */

#define FRM			1	/* round towards 0 */
#if (PROCESSOR&PROCESSOR_CLASS) == MIPS4000
# define FCSR_FS		(1<<24)
#else
# define FCSR_FS		(0)
#endif

typedef struct {		/* floating point instruction layout */
#if BYTE0_OFFS
	unsigned opcode:6;
	unsigned subop:5;
	unsigned ft:5;
	unsigned fs:5;
	unsigned fd:5;
	unsigned func:6;
#else
	unsigned func:6;
	unsigned fd:5;
	unsigned fs:5;
	unsigned ft:5;
	unsigned subop:5;
	unsigned opcode:6;
#endif
} Instruction;

typedef struct {		/* floating point data layout */
#if BYTE0_OFFS
    unsigned sign:1;
    unsigned exp:8;
    unsigned mant:23;
#else
    unsigned mant:23;
    unsigned exp:8;
    unsigned sign:1;
#endif
} Float;

/* get_fpcsr - get the current FPU's control and status register
 * At entry:
 *	no requirements
 * At exit:
 *	returns current contents of the FPU's csr
 */

extern U32 prc_get_fpcsr(void);

/* set_fpcsr - set the FPU's control and status register 
 * At entry:
 *	new = new value to set in csr
 * At exit:
 *	returns previous contents of the FPU's csr
 */
extern U32 prc_set_fpcsr(U32 new);

/* enable_cp1 - sets the bit in the SR to enable the FPU
 * At entry:
 *	no requirements
 * At exit:
 *	the CP1 bit is set in the SR
 *	returns 0
 */
extern U32 prc_enable_cp1(void);

static const char * const msgs[] = {
    "FP Inexact",
    "FP Underflow",
    "FP Overflow",
    "FP Divide by zero",
    "FP Invalid operation",
    "FP Unimplemented operation",
    "FP Undefined exception"
};

/* floating_exceptions is a array of totals of each of the exception types encountered.
 * The totals appear in the order as described in the above msgs table
 */

int floating_exceptions[7];

extern void wait_forever(void);

static int get_which_fp(U32 excepts) {
    int ii, jj;
    for (jj=0x20, ii=5; jj; jj >>= 1, --ii) {
	if (excepts&jj) return ii;
    }
    return 6;
}

#if !NO_PM_DATA
extern struct pm_general pm_data;
#endif

/* die - is a sort of duplicate of the panic function found in os_shims.
 * At entry:
 *	regs - points to the array of registers saved at exception
 *	msg - points the text of the reason for the exception
 * At exit:
 *	does not exit. It calls wait_forever() which either waits for
 *	watchdog to hit, or if the stub is loaded, jumps immediately
 *	to begin (effectively doing a reset).
 */

#if !LOOP_ON_ERROR
static void die(U32 *regs, const char *msg) {
# if !NO_PM_DATA
    struct pm_general *pmdta;
#  if HB_PHOENIX
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASE;
#  else
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)(0x9fc00010);
#  endif

    pmdta = &pm_data;

#  if REGISTER_SIZE == 4
    memcpy((char *)(pmdta->pm_regs+1), (char *)(regs+1), 31*4);
#  else
    {
       int jj,kk;
       for (jj=1, kk=AT_REGNUM_W; jj<32; ++jj, kk += 2) {
  	  pmdta->pm_regs[jj] = regs[kk];
       }
    }
#  endif
    pmdta->pm_regs[0] = 0;		/* r0 is always 0 */
    pmdta->pm_cause = regs[CAUSE_REGNUM_W];	/* remember the cause */
    pmdta->pm_stack = (U32*)regs[SP_REGNUM_W];
    pmdta->pm_stkupper = (U32 *)romv->ROMV_STACK;
    pmdta->pm_stklower = (U32*)prc_extend_bss(0);
    pmdta->pm_stkrelative = pmdta->pm_stack;
    pmdta->pm_badvaddr = regs[BADVADDR_REGNUM_W];
    pmdta->pm_pc = regs[PC_REGNUM_W];
    pmdta->pm_sr = regs[PS_REGNUM_W];
    pmdta->pm_msg = msg;
# endif
# if HOST_BOARD == LCR3K
    HEALTH = 0;		/* turn off the GREEN LED */
# endif
    wait_forever();
}
#endif

#if !TEST_IT || DO_IT_TOO

/* validate_float - verifies that the float argument is legimate
 * At entry:
 *	arg - points to what must be a floating point value
 * At exit:
 *	returns 1 if the argument is legimate (in range)
 *	returns 0 if the argument is NaN or other non-legal FP value
 */
static int validate_float(Float *arg) {
    if (arg->exp == 0) {
       if (arg->mant == 0) return 1;
       return 0;
    }
    if (arg->exp == 0xFF) return 0;
    return 1;
}

#if (PROCESSOR&PROCESSOR_CLASS) == MIPS4000
# define RINDX_MULT	(2)
#else
# define RINDX_MULT	(1)
#endif

/* ops3_s - process a 3 operand instruction
 * At entry:
 *	regs - pointer to registers saved at exception
 *	fd - destination register number
 *	fs - source register number
 *	ft - source register number
 * At exit:
 *	sets the register pointed to by fd to 0.0 and
 *	returns 0 if both ft and fs are valid floating numbers.
 *	returns 1 if either ft or fs is not a valid floating number.
 */
static int ops3_s(U32 *regs, int fd, int fs, int ft) {
    int sts;
    sts = validate_float((Float *)(regs+FP0_REGNUM_W+ft*RINDX_MULT)) |
          (validate_float((Float *)(regs+FP0_REGNUM_W+fs*RINDX_MULT))<<1);
    if (sts == 3) {
	regs[FP0_REGNUM_W+fd*RINDX_MULT] = 0;
	return 0;
    }
    return 1;
}

/* ops2_s - process a 2 operand instruction
 * At entry:
 *	regs - pointer to registers saved at exception
 *	fd - destination register number
 *	fs - source register number
 *	ft - not used (place holder)
 * At exit:
 *	sets the register pointed to by fd to 0.0 and
 *	returns 0 if fs is a valid floating number.
 *	returns 1 if fs is not a valid floating number.
 */
static int ops2_s(U32 *regs, int fd, int fs, int ft) {
    if (validate_float((Float *)(regs+FP0_REGNUM_W+fs*RINDX_MULT))) {
	regs[FP0_REGNUM_W+fd*RINDX_MULT] = 0;
	return 0;
    }
    return 1;
}
#endif

#if DO_IT_TOO
# define INIT_INS(name, routine, fmt, func) {name, routine, fmt, func}
#else
# if TEST_IT
#  define INIT_INS(name, routine, fmt, func) {name, fmt, func}
# else
#  define INIT_INS(name, routine, fmt, func) {routine, fmt, func}
# endif
#endif

static const struct {
#if TEST_IT
    const char *name;
#endif
#if !TEST_IT || DO_IT_TOO
    int (*routine)(U32 *, int, int, int);
#endif
    int subop;
    int func;
} instrs[] = {
    INIT_INS("add.s",	ops3_s,	  16, 0 ),
    INIT_INS("sub.s",	ops3_s,	  16, 1 ),
    INIT_INS("mul.s",	ops3_s,	  16, 2 ),
    INIT_INS("div.s",	ops3_s,	  16, 3 ),
    INIT_INS("abs.s",	ops2_s,	  16, 5 ),
    INIT_INS("mov.s",	0,	  16, 6 ),
    INIT_INS("cvt.s.d",	ops2_s,   17, 32 ),
    INIT_INS("cvt.d.s",	0,        16, 33 ),
    INIT_INS("cvt.s.w",	ops2_s,   20, 32 ),
    INIT_INS("cvt.d.w",	0,        20, 33 ),
    INIT_INS("cvt.w.s", ops2_s,   16, 36 ),
    INIT_INS("cvt.w.d", 0,        17, 36 ),
    INIT_INS("c.sf.s",	0,        16, 56 ),
    INIT_INS("c.ngle.s",0,        16, 57 ),
    INIT_INS("c.seq.s",	0,        16, 58 ),
    INIT_INS("c.ngl.s",	0,        16, 59 ),
    INIT_INS("c.lt.s",	0,        16, 60 ),
    INIT_INS("c.nge.s",	0,        16, 61 ),
    INIT_INS("c.le.s",	0,        16, 62 ),
    INIT_INS("c.ngt.s",	0,        16, 63 ),
    INIT_INS("c.sf.d",	0,        17, 56 ),
    INIT_INS("c.ngle.d",0,        17, 57 ),
    INIT_INS("c.seq.d",	0,        17, 58 ),
    INIT_INS("c.ngl.d",	0,        17, 59 ),
    INIT_INS("c.lt.d",	0,        17, 60 ),
    INIT_INS("c.nge.d",	0,        17, 61 ),
    INIT_INS("c.le.d",	0,        17, 62 ),
    INIT_INS("c.ngt.d",	0,        17, 63 ),
    INIT_INS("add.d",	0,	  17, 0 ),
    INIT_INS("sub.d",	0,	  17, 1 ),
    INIT_INS("mul.d",	0,	  17, 2 ),
    INIT_INS("div.d",	0,	  17, 3 ),
    INIT_INS("abs.d",	0,	  17, 5 ),
    INIT_INS("mov.d",	0,	  17, 6 ),
    INIT_INS("mfc1",	0,	   0, 0 ),
    INIT_INS("mtc1",	0,	   4, 0 ),
    INIT_INS(0, 0, 0, 0)
};

#if EMULATE_JUMP
static U32 emulate_jump(Instruction *ins, U32 *regs
# if SQUAWK
, char *fred, int *row
# endif
) {
    U32 ret, *ip;
    ret = 0;		/* assume nfg */
    ip = (U32*)ins;
# if SQUAWK
    if (ins->opcode == 2) {
	nsprintf(fred, AN_VIS_COL, "J %08lX", ((*ip&0x03FFFFFF)<<2) + ((U32)(ip+1)&0xF0000000));
    } else if (ins->opcode == 3) {
	nsprintf(fred, AN_VIS_COL, "JAL %08lX", ((*ip&0x03FFFFFF)<<2) + ((U32)(ip+1)&0xF0000000));
    } else if (ins->opcode == 0 && ins->func == 8) {
	nsprintf(fred, AN_VIS_COL, "JR r%d (%08lX, [%d*%d])", ins->subop,
    		regs[ZERO_REGNUM_W+ins->subop*RINDX_MULT], ins->subop, RINDX_MULT);
    } else if (ins->opcode == 0 && ins->func == 9) {
	nsprintf(fred, AN_VIS_COL, "JALR r%d,r%d (%08lX)",
    		ins->fs, ins->subop, regs[ZERO_REGNUM_W+ins->subop*RINDX_MULT]);
    } else {
	nsprintf(fred, AN_VIS_COL, "Unknown Jmp instruction: %08lX (opcode=%d)", *ip, ins->opcode);
    }
    txt_str(-1, *row, fred, WHT_PAL);
#  if HB_PHOENIX
    prc_delay(0);
#  endif
    *row += 1;
# endif
    if (ins->opcode == 2 ||		/* jump */
	ins->opcode == 3) {		/* jal */
	ret = ((U32)ip&0xF0000000) | ((*ip&0x03FFFFFF) << 2);	/* return address */
	if (ins->opcode == 3) {		/* jal */
	    regs[RA_REGNUM_W] = (U32)(ip+2);	/* patch the RA register */
	}
    } else if ((ins->opcode == 0 &&	/* special */
	(ins->func == 9 || 		/* jalr */
	 ins->func == 8))) {		/* jr */
	int rs, rd;
	rs = ins->subop;
	rd = ins->fs;
	if (ins->func == 9 && rd) regs[ZERO_REGNUM_W+rd*RINDX_MULT] = (U32)(ip+2);
	ret = regs[ZERO_REGNUM_W+(rs*RINDX_MULT)];
    }
# if (PROCESSOR&PROCESSOR_CLASS) == MIPS4000
    if (ret) regs[RA_REGNUM_U] = (regs[RA_REGNUM_W]&0x80000000) ? 0xFFFFFFFF : 0;
# endif
# if SQUAWK
    nsprintf(fred, AN_VIS_COL, "Return address = %08lX", ret);
    txt_str(-1, *row, fred, WHT_PAL);
#  if HB_PHOENIX
    prc_delay(0);
#  endif
    *row += 1;
# endif
    return ret;
}
#endif

/* fexcp - process a floating point exception
 * At entry:
 *	regs - pointer to registers saved at exception
 * At exit:
 *	if the failed instruction can be emulated and valid results
 *	returned to the destination register, the destination register
 *	will be updated in the exception frame, the saved PC will be
 *	advanced by 4 (to prevent the failed instruction from being
 *	restarted), and control returned to the caller of this function.
 *	If the instruction cannot be emulated or valid results cannot
 *	determined, the function will not return. It will either panic
 *	and cause a system reset or control will be given to the stub
 *	so the exception can be reported to gdb.
 */
static U32 *fexcp(U32 *regs) {
    int ii, nfg, which_fp;
    Instruction *ins;
#if HOST_BOARD == LCR3K
    struct ROM_VECTOR_STR * volatile romv = (struct ROM_VECTOR_STR *)0x9FC00010;
#else
# if !HB_PHOENIX
    struct ROM_VECTOR_STR * volatile romv = (struct ROM_VECTOR_STR *)(RRBUS_BASEnc+0x10);
    VU32 junk;
# else
    struct ROM_VECTOR_STR * volatile romv = (struct ROM_VECTOR_STR *)(DRAM_BASE);
# endif
#endif
    U32 fpcsr, next_ins;
#if SQUAWK
    char fred[AN_VIS_COL+1];
# if !HB_PHOENIX
    int row = 6;
# else
    int row = 17;
# endif
#endif

#if (PROCESSOR&PROCESSOR_CLASS) == MIPS3000
    prc_enable_cp1();		/* re-enable fp */
#endif

    fpcsr = prc_get_fpcsr();	/* get the CSR */
    regs[FCRCS_REGNUM_W] = fpcsr&~AllExceptions; /* clear the cause bits when we can continue */
    prc_set_fpcsr(regs[FCRCS_REGNUM_W]);	/* clear the cause bits so we can continue */
    fpcsr >>= 7;
    fpcsr = ((fpcsr|0x20) & (fpcsr>>5)) & 0x3F;	/* qualify the cause with the enable bits */
    which_fp = get_which_fp(fpcsr);	/* figure out which exception it is */
    ++floating_exceptions[which_fp];	/* tally it */
    romv->ROMV_STUB_REASON = (U32)(msgs[which_fp]); /* record the reason for the stub's benefit */
#if (HOST_BOARD == HCR4K)
    junk = romv->ROMV_STUB_REASON;
#endif
    next_ins = regs[PC_REGNUM_W]+4;	/* assume we're just to skip this instruction */
#if SQUAWK
    nsprintf(fred, sizeof(fred), "Got a floating exception at %08lX", regs[PC_REGNUM_W]);
    txt_str(-1, row++, fred, RED_PAL);
    nsprintf(fred, sizeof(fred), "CAUSE = %08lX, FPCSR = %08lX", regs[CAUSE_REGNUM_W], prc_get_fpcsr());
    txt_str(-1, row++, fred, RED_PAL);
#  if HB_PHOENIX
    prc_delay(0);
#  endif
#endif
    ins = (Instruction *)(regs[PC_REGNUM_W]);	/* get pointer to failed instruction */
    nfg = 0;				/* assume instruction is ok */
    if ((regs[CAUSE_REGNUM_W]&CAUSE_BD) != 0) {
#if EMULATE_JUMP
# if SQUAWK
	next_ins = emulate_jump(ins, regs, fred, &row);
# else
	next_ins = emulate_jump(ins, regs);
# endif
	if (!next_ins) {
	    nfg = 2;			/* in branch delay slot, can't deal with it */
	} else {
	    ins = (Instruction *)(regs[PC_REGNUM_W]+4);	/* point to the real instruction */
	}
#else
        nfg = 2;
#endif
    }
    if (!nfg) {
	if (ins->opcode != 17) {	/* if opcode is not a CP1 instruction */
	    nfg = 1;			/* not a valid FP instruction, punt */
	} else if (which_fp != 1 && which_fp != 5) {
	    nfg = 3;			/* not a FP operation that can be skipped, punt */
	}
    }
#if SQUAWK
    nsprintf(fred, sizeof(fred), "which_fp=%d, nfg=%d, ins=%08lX", which_fp, nfg, ins);
    txt_str(-1, row++, fred, RED_PAL);
#  if HB_PHOENIX
    prc_delay(0);
#  endif
#endif
#if TEST_IT
# define WHAT instrs[ii].name
#else
# define WHAT instrs[ii].routine
#endif
    if (!nfg) {			/* if it's potentially a valid instruction */
	for (ii=0; WHAT; ++ii) {
	    if (instrs[ii].subop == ins->subop && instrs[ii].func == ins->func) {
#if TEST_IT
# if SQUAWK
		nsprintf(fred, sizeof(fred), "%s f%d, f%d, f%d",
			instrs[ii].name, ins->fd, ins->fs, ins->ft);
		txt_str(-1, row++, fred, WHT_PAL);
#  if HB_PHOENIX
		prc_delay(0);
#  endif
# endif
#endif
#if !TEST_IT || DO_IT_TOO
		{
		    int (*func)(U32 *, int, int, int);
		    func = instrs[ii].routine;	/* get pointer to emulator function */
		    if (func) {			/* if there is one, execute it */
			nfg = func(regs, ins->fd, ins->fs, ins->ft) ? 3 : 0;
		    } else {
			nfg = 1;		/* else not a valid FP instruction */
		    }
		}
#endif
		break;
	    }
	}
	if (WHAT == 0) nfg = 1;			/* not a valid FP instruction */
    }
    if (nfg) {				/* if not a valid instruction */
#if SQUAWK
	if (nfg == 3) nsprintf(fred, sizeof(fred), "Floating point exception: %s", msgs[which_fp]);
	else if (nfg == 2) nsprintf(fred, sizeof(fred), "Floating point exception in branch delay slot.");
	else if (nfg == 1) nsprintf(fred, sizeof(fred), "Not an FP instruction: %08lX", *(U32*)regs[PC_REGNUM_W]);
	else strcpy(fred, "Unknown FP error");
	txt_str(-1, row++, fred, RED_PAL);
#  if HB_PHOENIX
	prc_delay(0);
#  endif
#endif
#if !LOOP_ON_ERROR
        {
	    void (*f)(U32 *regs);
# if HB_PHOENIX				/* you will want to make _all_ flavors use this eventually */
	    struct IIO_VECTOR_STR *s;
	    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
	    if (!s) die(regs, msgs[which_fp]);
	    f = (void (*)(U32*))s->STUBVEC_FAKE_EH;	/* get pointer to stub's handler */
# else
	    f = (void (*)(U32*))romv->ROMV_STUB_EH;	/* get pointer to stub's handler */
# endif
	    if (f) {			/* if there is one */
		prc_set_fpcsr(0);		/* turn off FP interrupts */
		f(regs);			/* jump to stub's exception handler */
	    } else {
		die(regs, msgs[which_fp]);	/* else there isn't one, so panic */
	    }
        }
#endif
    } else {
#if SQUAWK
# if (HOST_BOARD == HCR4K) || HB_PHOENIX
	nsprintf(fred, sizeof(fred), "Saved SR=%08lX, current SR=%08lX",
		regs[PS_REGNUM_W], prc_get_ipl());
	txt_str(-1, row++, fred, RED_PAL);
# endif
	nsprintf(fred, sizeof(fred), "Advancing PC to next_ins=%08lX", next_ins);
	txt_str(-1, row++, fred, RED_PAL);
	txt_str(-1, row++, "Exception counts:", WHT_PAL);
	nsprintf(fred, sizeof(fred), "I=%d, U=%d, O=%d, DBZ=%d, INV=%d, UNIMP=%d",
		floating_exceptions[0], floating_exceptions[1], floating_exceptions[2], 
		floating_exceptions[3], floating_exceptions[4], floating_exceptions[5]);
	txt_str(-1, row++, fred, WHT_PAL);
#  if HB_PHOENIX
	prc_delay(0);
#  endif
#endif
    }
#if !LOOP_ON_ERROR
# if (HOST_BOARD == HCR4K) || HB_PHOENIX
    if ((regs[PS_REGNUM_W]&SR_ERL) != 0) {
	regs[ERRPC_REGNUM_W] = next_ins;
    } else {
	regs[EPC_REGNUM_W] = next_ins;
    }
# endif
    regs[PC_REGNUM_W] = next_ins;	/* it's been handled, so skip the failing instrucion */
#endif
    return regs;			/* return from exception */
}

static void zero_fpregs(void) {		/* this is just to make debugging easier */
    __asm__(
    "mtc1 $0, $f0;"
    "mtc1 $0, $f1;"
    "mtc1 $0, $f2;"
    "mtc1 $0, $f3;"
    "mtc1 $0, $f4;"
    "mtc1 $0, $f5;"
    "mtc1 $0, $f6;"
    "mtc1 $0, $f7;"
    "mtc1 $0, $f8;"
    "mtc1 $0, $f9;"
    "mtc1 $0, $f10;"
    "mtc1 $0, $f11;"
    "mtc1 $0, $f12;"
    "mtc1 $0, $f13;"
    "mtc1 $0, $f14;"
    "mtc1 $0, $f15;"
    "mtc1 $0, $f16;"
    "mtc1 $0, $f17;"
    "mtc1 $0, $f18;"
    "mtc1 $0, $f19;"
    "mtc1 $0, $f20;"
    "mtc1 $0, $f21;"
    "mtc1 $0, $f22;"
    "mtc1 $0, $f23;"
    "mtc1 $0, $f24;"
    "mtc1 $0, $f25;"
    "mtc1 $0, $f26;"
    "mtc1 $0, $f27;"
    "mtc1 $0, $f28;"
    "mtc1 $0, $f29;"
    "mtc1 $0, $f30;"
    "mtc1 $0, $f31;"
    "ctc1 $0, $31;"
    );
    return;
}

void init_fpu() {
    zero_fpregs();
#if HOST_BOARD == LCR3K
    prc_set_vec((FLOATEXEC_LVL-XBUS0_LVL)+XBUS0_INTVEC, (void (*)(void))fexcp);
#else
    prc_set_vec(FLOAT_INTVEC, (void (*)(void))fexcp);
#endif
    prc_set_fpcsr(FCSR_FS|TrUNDERFLOW|TrOVERFLOW|TrZERODIV|TrINVALID|FRM);	/* enable most traps */
}
