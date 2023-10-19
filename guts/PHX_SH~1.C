/*
 * $Id: phx_shims.c,v 1.83 1997/10/15 06:07:37 shepperd Exp $
 *
 *		Copyright 1996 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#include <phx_proto.h>
#include <icelesspkt.h>
#include <intvecs.h>
#include <wms_proto.h>
#include <st_proto.h>

#define DO_STACK_WATERMARK	1

#if ANNOUNCE_BOOT_ACTIONS
# include <ioa_uart.h>
#endif

#define R4K_HOST 1

#if !defined(TIME_EXCEPTIONS) /* See also phx_root.mac */
# define TIME_EXCEPTIONS 0
#endif

#if !defined(SHOW_TIME)
# define SHOW_TIME	0
#endif

#if !defined(TIMER_HISTORY)
# define TIMER_HISTORY	0
#endif

#if !defined(NU_IE)
# define NU_IE		SR_IE
#endif

# define NUM_ELTS(x) (sizeof(x)/sizeof(x[0]))

extern void begin();

#define offsetof(s, m)    (int)(&(((s *)0)->m))

#include <eer_defs.h>
#include <os_proto.h>

extern struct _reent *mainline_reent;
extern struct _reent *interrupt_reent;
extern struct _reent *ast_reent;
extern struct _reent *action_reent;
extern struct _reent *_impure_ptr;

extern struct pconfigb guts_pbase;
const struct pconfigb *pbase;
extern void SelfTest(int cold);
extern void eer_user_purge();

#if !BOOT_FROM_DISK && !EPROM_ST
# define POWERUP (0xC0EDBABE)
extern unsigned long powerUp;
#endif

unsigned short bitshd;

int prc_mod_latch(int new) {
   int old_sr;

   if (new == 0) return bitshd;		/* just report what it currently is */
   old_sr = prc_set_ipl(INTS_OFF);	/* disable interrupts */
   if (new < 0) {		
      bitshd &= new;		/* negative, we AND out the cooresponding bits */
   } else {
      bitshd |= new;		/* positive, we OR in the cooresponding bits */
   }
   prc_set_ipl(old_sr);		/* re-enable interrupts */
   return new;
}

static U32 old_timer_rate;

U32 prc_timer_rate(U32 new) {
#if HOST_BOARD != CHAMELEON
    VU32 *timr, *ctl;
#endif
    U32 old, oldps;
    old = old_timer_rate;

    if (new) {
	oldps = prc_set_ipl(INTS_OFF);

#if HOST_BOARD != CHAMELEON
	timr = (VU32*)GALILEO_TIMER3;
	ctl = (VU32*)GALILEO_TIMER_CTL;
	*ctl &= ~(3<<6);	/* disable the timer/counter */
	*timr = new;
	*ctl |= (3<<6);		/* enable the timer as a timer */
	*(VU32*)GALILEO_INT_CAUSE = ~TM3_NOTES; /* ack any pending timer interrupts */
#else
	prc_set_compare(prc_get_count() + new); /* acks the interrupt */
#endif
	old_timer_rate = new;
	prc_set_ipl(oldps);
    }
    return old;
}

void __main() {
    return;
}


#if ANNOUNCE_BOOT_ACTIONS

int shims_putc_disable;

void (*shims_putc_vec)(char c);

void shims_putc(char c) {

    if (shims_putc_vec) {
	shims_putc_vec(c);		/* do his action */
	return;
    }

    if (!shims_putc_disable) {		/* Only print if it's not disabled */
	prc_putc(c);
    }

}

int shims_puts(const char *str) {
    int c, lastc=0;
    const char *msg = str;

    if (shims_putc_disable) return 0;
    msg = str;
    while ((c=*str++) != 0) {
	if (c == '\n' && lastc != '\r') shims_putc('\r');
	shims_putc(c);
	lastc = c;
    }
    return str-msg;
}

#ifndef BLAB
# define BLAB(x) shims_puts(x)
#endif
#ifndef BLABF
# define BLABF(x) shims_printf x
#endif

#else

#ifndef BLAB
# define BLAB(x)
#endif

#ifndef BLABF
# define BLABF(x)
#endif

#endif			/* ANNOUNCE_BOOT_ACTIONS */

#if MAX_AST_QUEUES 
volatile int _guts_astlvl;
#endif

#if HOST_BOARD == CHAMELEON
static void cputimerint(void);
#endif

#if INCLUDE_SYSCALL
static void syscall(U32 *);
#endif

const char boot_version[] = 
"\r\nEPROM Boot code. Version: " __DATE__ " " __TIME__ "\r\n"
"Copyright 1996,1997 Atari Games, Corp.\r\n";

void BootUp() {
   extern struct pconfigp PBASE;
   unsigned long cold_boot;
   void (*func)(void);
   struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;

#if HOST_BOARD == PHOENIX
   *(VU32*)IO_RESET = -1;      /* l=unleash the I/O ASIC */
   *(VU32*)PCI_RESET = -1;     /* unleash the PCI */
#endif
#if (HOST_BOARD == PHOENIX_AD) || (HOST_BOARD == SEATTLE) || (HOST_BOARD == VEGAS)
    /* Un-reset everything */
   *(VU32*)RESET_CTL = (1<<B_RESET_EXP)|(1<<B_RESET_IOASIC)|(1<<B_RESET_IDE)|
    			(1<<B_RESET_3DFX)|(1<<B_RESET_NSS)|(1<<B_RESET_WIDGET);
#endif
#if HOST_BOARD == FLAGSTAFF
    /* Un-reset everything */
   *(VU32*)RESET_CTL = (1<<B_RESET_EXP)|(1<<B_RESET_IOASIC)|(1<<B_RESET_IDE)|
    			(1<<B_RESET_3DFX)|(1<<B_RESET_ENET)|(1<<B_RESET_WIDGET);
#endif
#if (HOST_BOARD == CHAMELEON)
    /* Un-reset everything */
   RESET_CTL_T = RESET_PCI_SLOT_0|RESET_PCI_SLOT_1|RESET_PCI_SLOT_2|
    		 RESET_IDE|RESET_IOASIC|RESET_AUX_UART|RESET_SMC|
    		 RESET_NSS_CONN|RESET_GUN_CONF;
#endif

#if MAX_AST_QUEUES 
    _guts_astlvl = -1;
#endif

   func=(void (*)(void))romv->ROMV_STUB_INIT;
        
   if (func) {		/* if the stub is loaded, go init it... */
       if (!romv->ROMV_STUB_FLAG) func(); /* ...if not already init'ed */
# if HOST_BOARD == CHAMELEON
#  if defined(DBG_NOTES)
       INTCTL_IE_T |= DBG_NOTES;
#  endif
# else
#  if ICELESS_MANY
       *(VU32*)INTCTL_IE |= ICELESS_MANY;
#  else
#   if ICELESS_LVL
       *(VU32*)INTCTL_IE |= 1<<(ICELESS_LVL-1);
#   endif
#  endif
# endif
   }

# if HOST_BOARD != CHAMELEON
    UnLock();				/* free up the I/O ASIC */
# endif
    prc_wait_n_usecs(100000);		/* wait for the I/O ASIC to come to life */

#if ANNOUNCE_BOOT_ACTIONS
# ifdef IO_DIPSW_T
    if ((IO_DIPSW_T&(IO_DIPSW0|IO_DIPSW1)) != 0) shims_putc_disable = 1;
# else
#  ifdef IO_DIPSW
    if ((*(VU32*)IO_DIPSW&(IO_DIPSW0|IO_DIPSW1)) != 0) shims_putc_disable = 1;
#  endif
# endif

#if !SA_DIAGS
    BLAB(boot_version);
#endif

# if DRAM_BANKS
    BLABF(("Shims: DRAM configuration: %d bank%sof %d megabyte%c\r\n",
    		DRAM_BANKS, DRAM_BANKS > 1 ? "s " : " ",
    		DRAM_BANK_SIZE/0x00100000, DRAM_BANK_SIZE > 0x00100000 ? 's' : ' '));
# endif
# if HOST_BOARD == PHOENIX
    BLAB("Shims: 1 bank of 8 and 1 bank of 32 megabytes\r\n");
# endif
#endif

# if ICELESS_NMI
    if (romv->ROMV_STUB_VECS) {		/* if the iceless has an NMI vector ...*/
	struct IIO_VECTOR_STR *s;

	s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
	if (s->STUBVEC_NMI) {
	    VU32 *ptr = (VU32*)INTCTL_NMI;
	    *ptr = ICELESS_NMI;		/* select the NMI bit */
	}
    }
# endif	

# if HOST_BOARD != CHAMELEON
   BLAB("Shims: Setting Galileo timer...");
   prc_timer_rate(50000);	/* set rate to 1Ms */
#else
   BLABF(("Shims: Setting CPU timer to %d...", CPU_SPEED/2000));
   prc_set_vec(INT5_INTVEC, cputimerint);
   old_timer_rate = CPU_SPEED/2000; 
#endif

   pbase = &guts_pbase;		/* for now, we just use our dummy pconfigb */
   if ( PBASE.p_validate == PB_VALID && PBASE.p_configb ) pbase = PBASE.p_configb;

# if INCLUDE_FEXCP
   {
       extern void init_fpu(void);
       BLAB("\nShims: Installing floating point exception handler...");
       init_fpu();
   }
# endif

# if INCLUDE_SYSCALL
   prc_set_vec(SYSCALL_INTVEC, (void (*)(void))syscall);
# endif

#if !BOOT_FROM_DISK && !EPROM_ST
   cold_boot = powerUp ^ POWERUP;
   powerUp = POWERUP;
#else
   cold_boot = 1;
#endif

   BLAB("\nShims: Finish setting up \"C\" environment...");

   __main();			/* do other startup stuff */

   BLAB("\nShims: Starting selftest code.\r\n");
   SelfTest(cold_boot);		/* now goto selftest */

}

# if !BOOT_FROM_DISK && !EPROM_ST
extern struct pm_general pm_data;
# endif
extern U32 INIT_SP;

extern void flush_cache(void);
void stub_start(void) {
    flush_cache();		/* make sure any variables are written to memory */
    begin();			/* doesn't return from here */
}

void prc_reboot() {		/* reboot */
   int cnt;
   extern void eer_action(void *);

   eer_user_purge();		/* clear any open user recs w/o writing them */
   eer_action(0);		/* purge any pending eeprom writes */
   
   prc_set_ipl(INTS_OFF);	/* disable interrupts */
#if !BOOT_FROM_DISK && !EPROM_ST
   pm_data.pm_cntr = -42;	/* this means something to Mike */
   pm_data.pm_msg = 0;
#endif
   for (cnt=0; cnt < 40; ++cnt) {
#if !BOOT_FROM_DISK && defined(WDOG) && !NO_WDOG && !EPROM_ST
       WDOG = 0;		/* give WDOG a kick */
#endif
       vid_waitvb(0);	/* count frames */ 
   }
   flush_cache();		/* make sure our variables are written to memory */
   prc_reset_hardware();
#if !BOOT_FROM_DISK && defined(WDOG) && !NO_WDOG && !EPROM_ST
   WDOG = 0;			/* give WDOG one more kick to give some additional time */
#endif
   begin();			/* never comes back from here */
}

/*		exit_to_game()
 *	Hack to allow Williams-style momentary-contact TEST button
 *	even when it takes a _long_ time to get through a boot.
 *	The actual hackery involves a variable in the text segment
 *	(S.O.L. if we ever protect memory), which is extern'd here
 *	because it's not real general-case.
 *
 *	The function itself is defined to take a selected menu pointer
 *	and return an int so that it can be called directly from the
 *	game's selftest menu.
 */
extern int go_to_self_test;
int exit_to_game( const struct menu_d *smp ) {
    (void) smp;
#if (BOOT_FROM_DISK == 0) && (EPROM_ST == 0)
    go_to_self_test = 0;
#endif
    prc_reboot();
    return 0;
}

void prc_cache_init(int cache_on) {	/* cache is always on on this processor */
   return;
}

void (*ms4vec)(void);

extern int adj_usclock();

static U32 *his_zclock;

U32 *prc_timer_ptr(U32 *ptr) {
    U32 *old;
    old = his_zclock;
    his_zclock = ptr;
    return old;
}

#if TIMER_HISTORY
static U32 timer_history[TIMER_HISTORY];
static int timer_history_nxt;
#endif

#if SHOW_TIME
typedef struct dow {
    int days;
    int hours;
    int mins;
    int secs;
    int subsec;
} Dow;

static Dow act;
static struct tq dow_q;

#include <nsprintf.h>

static void upd_dow(Dow *dow, int row) {
    ++dow->subsec;
    *(VU32*)XBUSMON_BASE = dow->subsec&1;
    if (dow->subsec >= 920) {
	U32 oldpos;
	dow->subsec = 0;
	++dow->secs;
	if (dow->secs > 59) {
	    dow->secs = 0;
	    ++dow->mins;
	    if (dow->mins > 59) {
		dow->mins = 0;
		++dow->hours;
		if (dow->hours > 23) {
		    dow->hours = 0;
		    ++dow->days;
		}
	    }
	}
	oldpos = txt_setpos(0);
	txt_decnum((AN_VIS_COL-12)/2, row, dow->days, 3, RJ_BF, WHT_PALB);
	txt_cstr(" ", WHT_PALB);
	txt_cdecnum(dow->hours, 2, RJ_ZF, WHT_PALB);
	txt_cstr(":", WHT_PALB);
	txt_cdecnum(dow->mins, 2, RJ_ZF, WHT_PALB);
	txt_cstr(":", WHT_PALB);
	txt_cdecnum(dow->secs, 2, RJ_ZF, WHT_PALB);
	txt_setpos(oldpos);
    }
    return;
}

static void timer_act(void *arg) {
    dow_q.delta = 1000;
    tq_ins(&dow_q);
    upd_dow(&act, AN_VIS_ROW-1);
    return;
}
#endif

static U32 timer_ints;		/* number of cputimer interrupts */

U32 prc_timer_ints(void) {
    return timer_ints;
}

#define COUNTS_PER_USEC (CPU_SPEED/2000000)

void prc_timer_jobs(int how) {
    U32 lt;
    static U32 last_time;

    ++timer_ints;
#if TIMER_HISTORY
     start = prc_get_count();
     timer_history[timer_history_nxt] = start - timer_history[timer_history_nxt];
     ++timer_history_nxt;
     if (timer_history_nxt >= n_elts(timer_history)) timer_history_nxt = 0;
     timer_history[timer_history_nxt] = start;
#endif
#if SHOW_TIME
     if (!dow_q.func) {
	dow_q.func = timer_act;
	dow_q.delta = 1000;
	tq_ins(&dow_q);
     }
#endif
    lt = 1000;				/* assume 1000 usecs since last time */
    if (last_time && CPU_SPEED) {	/* as long as there was a last time */
	lt = (prc_get_count() - last_time)/COUNTS_PER_USEC; /* compute usecs since last time */
    }
    last_time = prc_get_count();	/* remember this */
    tq_maint(lt);			/* update and launch any timers */

    if (his_zclock) *his_zclock += 1;
    if (tpllvec) tpllvec();
    if (timervec) timervec();		/* call game's interrupt */
}

#if HOST_BOARD == CHAMELEON
static
#endif
void cputimerint() {
#if HOST_BOARD != CHAMELEON
   VU32 *cause  = (VU32*)GALILEO_INT_CAUSE;
# if TIMER_HISTORY
   U32 start;
# endif

   if (!(*cause&TM3_NOTES)) return;	/* not us, just exit */

   *cause = ~TM3_NOTES;			/* ack the interrupt */
#else
   prc_timer_rate(prc_timer_rate(0));	/* ack the timer and set for next time */
#endif

   prc_timer_jobs(1);			/* from h/w interrupt */
}

/* prc_add_q takes a pointer to an "action" structure and puts it at the
 * end of the specified queue, provided it is not already on a queue.
 * Otherwise, it returns a pointer to the "queue head" for the queue
 * it is already on (including the specified one).
 */

struct act_q *prc_add_q(struct act_q *head, struct act_q *new) {
   int old_sr;
   struct act_q *ans = 0;	/* assume success */
   if (!new) return 0;		/* cover bonehead move */
   if (!head) return 0;		/* cover bonehead move */
   old_sr = prc_set_ipl(INTS_OFF); /* has to work w/o interrupts */
   if (new->que == 0) {		/* not currently on any queue */
      ans = head;		/* point to head of action queue */
      while (ans) {		/* while the pointer is not empty */
	 if (ans->next == new ) {
	    new->que = head;	/* fix what got us here */
	    ans = head;		/* already in this queue */
	    break;
	 }
	 if (ans->next == 0) {	/* found the last member */
	    new->next = 0;
	    new->que = head;	/* say what queue we're on */
	    ans->next = new;	/* we're the new last member */
	    ans = 0;		/* return 0 to say it worked */
	    break;
	 }
	 ans = ans->next;
      }
   } else {
      if (new->que != head) {
	 ans= new->que;
      }
   }
   prc_set_ipl(old_sr);
   return (struct act_q *)ans;
}

void prc_swint0() {		/* signal software interrupt 0 */
    __asm__("mfc0 $2, $13; li $3, 0x100; or $2, $3; mtc0 $2, $13");
}
    
void prc_swint1() {		/* signal software interrupt 1 */
    __asm__("mfc0 $2, $13; li $3, 0x200; or $2, $3; mtc0 $2, $13");
}

struct act_q irq_action;
struct act_q *(*prc_q_action_vec)(struct act_q *new);
void (*process_action_q_vec)(int, int);

struct act_q *prc_q_action(struct act_q *new) {
    if (prc_q_action_vec) return prc_q_action_vec(new);
    return prc_add_q(&irq_action, new);
}

void process_action_q(int old_ps, int new_ps) {
      struct act_q * volatile act;
      void (*rtn)(void *);
      static void (*savrtn)(void *);

      _impure_ptr = action_reent;
      while ((act=irq_action.next) != 0) {
         void *param;
	 irq_action.next = act->next;	/* pluck this entry from the queue */
	 act->next = 0;			/* make sure we don't get in too big of a loop */
	 act->que = 0;			/* timer action seems to want this cleared too */
	 savrtn = rtn = (void (*)(void *))act->action;	/* remember the function */
         param = act->param;		/* remember the parameter */
	 if (rtn) {
	     prc_set_ipl(new_ps);
	     if (rtn != savrtn) {
		prc_panic("Act_q_rtn clobbered");
	     }
	     rtn(param);		/* call his function if there is one */
	     prc_set_ipl(old_ps);	/* restore the SR to pre-action schedule */
	 }
      } 				/* continue until the queue is empty */
      return;
}

#if MAX_AST_QUEUES 
    
int prc_get_astlvl(void) {
    return _guts_astlvl;
}

struct act_q ast_action[MAX_AST_QUEUES];
int (*prc_q_ast_vec)(int level, struct act_q *new);
void (*process_ast_q_vec)(int, int);

int prc_q_ast(int level, struct act_q *new) {
    if (prc_q_ast_vec) return prc_q_ast_vec(level, new);
    if (level < 0) return 1;
    if (level >= MAX_AST_QUEUES) return 2;
    if (prc_add_q(ast_action+level, new)) return 3;
    if (!prc_get_actlvl() && _guts_astlvl < 0) prc_swint1();
    return 0;
}

void process_ast_q(int old_ps, int new_ps) {
    struct act_q * volatile act;
    void (*rtn)(void *);
    static void (*savrtn)(void *);
    int ii, didit;

    _impure_ptr = ast_reent;
    do {				/* everytime a function runs, we start over */
	didit = 0;			/* assume we did nothing */
	for (ii=0; ii < MAX_AST_QUEUES; ++ii) {
	    while ((act=ast_action[ii].next) != 0) {
		void *param;
		ast_action[ii].next = act->next; /* pluck this entry from the queue */
		act->next = 0;			/* make sure we don't get in too big of a loop */
		act->que = 0;			/* timer action seems to want this cleared too */
		savrtn = rtn = (void (*)(void *))act->action;	/* remember the function */
		param = act->param;		/* remember the parameter */
		if (rtn) {
		    int prev_lvl = _guts_astlvl;
		    _guts_astlvl = ii;		/* new AST level */
		    prc_set_ipl(new_ps);
		    if (rtn != savrtn) {
			prc_panic("Ast_q_rtn clobbered");
		    }
		    rtn(param);		/* call his function if there is one */
		    prc_set_ipl(old_ps);	/* restore the SR to pre-action schedule */
		    _guts_astlvl = prev_lvl;
		    didit = 1;
		    break;
		}
	    } 				/* continue until the queue is empty */
	    if (didit) break;
	}
    } while (didit);
    return;
}
#endif			/* MAX_AST_QUEUES > 0 */

extern struct ROM_VECTOR_STR romv_tbl;

#if GAL_NOTES

#define G_MRE	(1<<12)		/* master read error */
#define G_SWE	(1<<13)		/* slave write error */
#define G_MWE	(1<<14)		/* master write error */
#define G_SRE	(1<<15)		/* slave read error */
#define G_ADE	(1<<16)		/* address error */
#define G_MEE	(1<<17)		/* memory error */
#define G_MAB	(1<<18)		/* master abort */
#define G_TAB	(1<<19)		/* target abort */
#define G_RTY	(1<<20)		/* retry counter expired */

void galileoint(U32 *regs) {
    U32 which;
    struct IIO_VECTOR_STR *s;
    void (*f)(U32 *regs);
    struct ROM_VECTOR_STR *romv;

    which = *(VU32*)GALILEO_INT_CAUSE & GALILEO_INTCTL_MASK & 
    		(G_MRE|G_SWE|G_MWE|G_SRE|G_ADE|G_MEE|G_MAB|G_TAB|G_RTY);
    if (!which) return;		/* not our error */

    romv = (struct ROM_VECTOR_STR *)(DRAM_BASE);
    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
    if (s) {
	f = (void (*)(U32*))s->STUBVEC_FAKE_EH;	/* get pointer to stub's handler */
	if (f) {
	    romv->ROMV_STUB_REASON = (U32)"Galileo reported bus error";
	    f(regs);		/* jump to stub */
	    *(VU32*)GALILEO_INT_CAUSE = ~(G_MRE|G_SWE|G_MWE|G_SRE|G_ADE|G_MEE|G_MAB|G_TAB|G_RTY);
	    return;
	}
    }	    
    while (1) {
	__asm__("BREAK");
    }
    return;
}
#endif

#if TIME_EXCEPTIONS
extern U32 exception_time[2];
U32 prc_get_exception_time( int clear )
{
 U32 tmp = exception_time[0];
 if ( clear ) exception_time[0] = 0;
 return tmp;
}
#endif

int _ice_pkts;

volatile int _guts_inest;

int prc_get_actlvl(void) {
    return _guts_inest;
}

#if (ICELESS_AST && ICELESS_LVL) || (HOST_BOARD == CHAMELEON)
static struct IIO_VECTOR_STR *check_for_iceless_int(U32 *regs) {
   struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
   struct IIO_VECTOR_STR *stub_vecs;
# ifndef TWI_THREAD
#  define TWI_THREAD 0
# endif
   int disab=1;

# if HOST_BOARD == CHAMELEON
#  ifdef B_PIC_DBG
#   define IS_ICELESS_INTERRUPT() (INTCTL_CAUSE_T&(1<<B_PIC_DBG))
#  else
#   error Need to define DBG_ASN in config.mac
#   define IS_ICELESS_INTERRUPT() (0)
#  endif
# else
#  define IS_ICELESS_INTERRUPT() (prc_get_cause() & 0x8000)
# endif
   stub_vecs = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
   if ( stub_vecs ) {
      if ( IS_ICELESS_INTERRUPT() ) {
	 int (*pfunc)(int);

	 pfunc = (int (*)(int))stub_vecs->STUBVEC_GOT_ATTEN;	/* point to attention handler */
	 if (pfunc) {				/* if there is one */
	    int ans;
	    ans = pfunc(TWI_THREAD);		/* poll for a packet */
	    if (ans) {				/* got a GDB packet */
	       U32 (*efunc)(U32 *);		/* ptr to stub's exception handler */
	       efunc = (U32 (*)(U32*))stub_vecs->STUBVEC_FAKE_EH;
	       efunc(regs);			/* go to stub's exception handler */
	    } else {
	       ++_ice_pkts;			/* something to do to break on */
	    } 
	    disab = 0;				/* don't disable interrupts */
	 }
      } else {
	 disab = 0;				/* don't disable interrupts */
      }
   }
# if HOST_BOARD == CHAMELEON
   if (disab) {
      INTCTL_IE_T &= ~(1<<B_PIC_DBG);	/* disable future iceless interrupts */
   } else {
      void (*pfunc)(int);
      pfunc = (void (*)(int))stub_vecs->STUBVEC_PKTSCAN; /* and we can scan */
      if (pfunc) pfunc(1);		/* scan and call completion routines if any */
   }
# endif
   return stub_vecs;
}
#endif

VU32 pc_at_interrupt;

#if HOST_BOARD == CHAMELEON
# define IH_TYPE void
#else
# define IH_TYPE unsigned long *regs
#endif
    
volatile int exception_count;
volatile int interrupt_count;

void interrupt_handler(IH_TYPE) {
   int which;
   int new, ii, start, did;
   void (*func)(void);
   U32 *ramv;
   struct IIO_VECTOR_STR *stub_vecs;
#if !(ICELESS_AST && ICELESS_LVL)
   struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
   stub_vecs = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
#endif

   ramv = (U32*)romv_tbl.ROMV_RAMVBR;

   ++interrupt_count;

#if (ICELESS_AST && ICELESS_LVL)
   stub_vecs = check_for_iceless_int(regs);
#endif

#if GAL_NOTES
   if (prc_get_cause()&regs[PS_REGNUM_W]&(0x400<<GALILEO_LVL)) {
	galileoint(regs);
   }
#endif

   ++_guts_inest;

   start = 0;
   do {
       did = 0;			/* and assume we didn't do anything */
#if HOST_BOARD == CHAMELEON
       which = prc_get_cause() & SR_IMASK;
#else
       which = prc_get_cause() & regs[PS_REGNUM_W] &SR_IMASK;
#endif
       for (ii=start, new=0x100<<ii; ii < 8; ++ii, new += new) {
	  if ((new&which) != 0) {
	     if (ii < 2) {
		prc_set_cause(prc_get_cause()&~new);
#if HOST_BOARD != CHAMELEON
		regs[CAUSE_REGNUM_W] &= ~new;	/* ACK the s/w interrupt */
#endif
	     }
	     func = (void (*)(void))ramv[VN_IRQ0 + ii];
	     if (func == 0) {
#if ICELESS_AST && ICELESS_LVL
		if (stub_vecs && ii == 7) continue; /* leave ICELESS interrupts alone */
#endif
		if (ii > 1) {		/* legal to have swint's with no vector */
#if HOST_BOARD != CHAMELEON
		   regs[PS_REGNUM_W] &= ~new; /* don't allow this interrupt anymore */
#endif
		   prc_set_ipl(prc_get_ipl()&~new); /* remember the local value too */
		}
	     } else {
#if HOST_BOARD == CHAMELEON
    		if (!stub_vecs) did = 1;
#else
		if (!stub_vecs || ii != 7) did = 1;
#endif
		func();		/* go to user's interrupt routine */
	     }
	  }
       }
       start = 2;		/* next time we skip the s/w interrupt check */
#if (ICELESS_AST && ICELESS_LVL)
       if (stub_vecs) check_for_iceless_int(regs); /* check again for iceless ints */
#endif
   } while (did);
#if ICELESS_AST && ICELESS_LVL
   if (stub_vecs) {		/* if there was an ICELESS interrupt */
      void (*pfunc)(int);
      pfunc = (void (*)(int))stub_vecs->STUBVEC_PKTSCAN; /* and we can scan */
      if (pfunc) pfunc(1);	/* scan and call completion routines if any */
   }
#endif
   if (_guts_inest == 1) {
      int old_ps, new_ps;
#if HOST_BOARD != CHAMELEON
      pc_at_interrupt = regs[PC_REGNUM_W];
#endif
      old_ps = prc_get_ipl();	/* interrupts are disabled at this point */
      new_ps = old_ps | NU_IE;	/* allow interrupts for action routine execution */
      if (process_action_q_vec) {
	  process_action_q_vec(old_ps, new_ps);
      } else {
	  process_action_q(old_ps, new_ps);
      }
   }
   --_guts_inest;
#if MAX_AST_QUEUES 
   if (!_guts_inest && _guts_astlvl < 0) { /* If no actions and no AST level */
      int old_ps, new_ps;
      old_ps = prc_get_ipl();	/* interrupts are disabled at this point */
      new_ps = old_ps | NU_IE;	/* allow interrupts for ast routine execution */
      if (process_ast_q_vec) {
	  process_ast_q_vec(old_ps, new_ps);
      } else {
	  process_ast_q(old_ps, new_ps);
      }
   }
#endif
#if !NO_BLINKING_LED && defined(LED_OUT)
   if (_guts_inest) {
	*(VU32*)LED_OUT &= ~(1<<B_LED_YEL);
   } else {
	*(VU32*)LED_OUT |= 1<<B_LED_YEL;
   }
#endif
   return;
}

extern void wait_forever();

static const char * const pm_cause_msgs[] = {
      "Interrupt",
      "Write to read-only memory",
      "Read from non-existant memory",
      "Write to non-existant memory",
      "Address alignment error on read",
      "Address alignment error on write",
      "Bus error on I fetch",
      "Bus error on D fetch",
      "SYCALL",
      "Breakpoint",			/* breakpoint */
      "Reserved Instruction",
      "CoProcessor unusable",
      "Arithmetic Overflow",
      "Trap exception",
      "Reserved exception",
      "Floating point exception",
      "Undefined exception"
};

#if 0
/*************************************************************************
 * gcc has assigned the following numbers on the BREAK instruction to report
 * various conditions at runtime. We have also assigned one for "unhandled
 * interrupt" for use on the RISQ, HCR4K and MB4600 boards. Some of these
 * breakpoints are not likely to be used by the 4K compiler.
 *
 * If you want to add more, stick them just before "Multiply overflow".
 *************************************************************************/

static const char * const pm_break_msgs[] = {
      "Debug breakpoint",			/* BREAK 0 */
      "Kernel breakpoint",			/* BREAK 1 */
      "Abort",					/* BREAK 2 */
      "Taken BD emulation",			/* BREAK 3 */
      "Not taken BD emulation",			/* BREAK 4 */
      "Single step breakpoint",			/* BREAK 5 */
      "Overflow check",				/* BREAK 6 */
      "Divide by zero",				/* BREAK 7 */
      "Range check",				/* BREAK 8 */
      "Unhandled interrupt",			/* BREAK 9 */
      "Multiply overflow",			/* BREAK 1023 (this MUST be second to last) */
      "Breakpoint"				/* any other break (this MUST be last) */
};
#endif

#define NUM_ELEMS(array) (sizeof(array)/sizeof(array[0]))
#define MAX_CAUSE_MSG (NUM_ELEMS(pm_cause_msgs) - 1)
#define MAX_BREAK_MSG (NUM_ELEMS(pm_break_msgs) - 2)
#define MULOVF_MSG    (NUM_ELEMS(pm_break_msgs) - 2)
#define BREAK_MSG     (NUM_ELEMS(pm_break_msgs) - 1)

const char *get_pm_msg(int cause) {
   int ii;
   const char *ans;

   ii = (cause&0x7C)/4;
   if (ii > MAX_CAUSE_MSG) ii = MAX_CAUSE_MSG;
#if 0
   if (ii == 9) {		/* if breakpoint */
      U32 jj, lpc;
      lpc = regs[PC_REGNUM_W]|0x20000000;
      if ( ((lpc&3) == 0) && 
	  ( (lpc >= DRAM_BASEnc && lpc < (DRAM_BASEnc+0x08000000)) ||
            (lpc >= EXPAN2_BASE && lpc < (EXPAN2_BASE+0x00800000)) ) ) {
	  jj = *(VU32*)lpc;	/* get the failing instruction */
	  jj >>= 16;		/* get the code bits */
	  jj &= 0x3FF;		/* isolate them */
	  if (jj == 1023) {
	     jj = MULOVF_MSG;
	  } else if (jj >= MAX_BREAK_MSG) {
	     jj = BREAK_MSG;
	  }
      } else {
         jj = BREAK_MSG;
      }
      ans = pm_break_msgs[jj];
   } else {
      ans = pm_cause_msgs[ii];
   }
#else
   ans = pm_cause_msgs[ii];
#endif
   return ans;
}

static void update_pm(U32 *regs) {
#if !BOOT_FROM_DISK && !EPROM_ST
# if REGISTER_SIZE == 4
    memcpy((char *)(pm_data.pm_regs+1), (char *)(regs+1), 31*4);
# else
    {
       int jj, kk;
       for (jj=1, kk=AT_REGNUM_W; jj<32; ++jj, kk += 2) {
  	  pm_data.pm_regs[jj] = regs[kk];
       }
    }
# endif
    pm_data.pm_regs[0] = 0;		/* r0 is always 0 */
    pm_data.pm_cause = regs[CAUSE_REGNUM_W];	/* remember the cause */
    pm_data.pm_stack = (U32*)regs[SP_REGNUM_W];
    pm_data.pm_stkupper = &INIT_SP;
    pm_data.pm_stklower = (U32*)prc_extend_bss(0);
    pm_data.pm_stkrelative = pm_data.pm_stack;
    pm_data.pm_badvaddr = regs[BADVADDR_REGNUM_W];
    pm_data.pm_pc = regs[PC_REGNUM_W];
    pm_data.pm_sr = regs[PS_REGNUM_W];
    ++pm_data.pm_cntr;
    pm_data.pm_msg = get_pm_msg(regs[CAUSE_REGNUM_W]);
#endif			/* !BOOT_FROM_DISK && !EPROM_ST */
}

#if DO_STACK_WATERMARK
U32 stack_watermark;
#endif

#if INCLUDE_SYSCALL
# ifndef MAX_SYSCALLS
#  define MAX_SYSCALLS	16
# endif

static int (*syscall_vectors[MAX_SYSCALLS])(void *a, ...);

int (*prc_set_syscall(int which, int (*func)(void *a, ...)))(void *a, ...) {
    int (*f)(void *,...);
    
    if (which < 0 || which >= MAX_SYSCALLS) return 0;
    f = syscall_vectors[which];
    syscall_vectors[which] = func;
    return f;
}

static void syscall(U32 *regs) {
    void *a, *b, *c;
    int which;
    int (*f)(void *,...)=0;

    which = regs[A0_REGNUM_W];
    if (which >= MAX_SYSCALLS || !(f=syscall_vectors[which]) ) {
	update_pm(regs);
	wait_forever();
    }
    a = (void *)regs[A0_REGNUM_W+1*REGISTER_SIZE/4];
    b = (void *)regs[A0_REGNUM_W+2*REGISTER_SIZE/4];
    c = (void *)regs[A0_REGNUM_W+3*REGISTER_SIZE/4];
    which = f(a, b, c);
    regs[V0_REGNUM_W] = which;	/* return result to caller */
    regs[EPC_REGNUM_W] += 4;	/* advance PC over SYSCALL instruction */
    return;
}
#endif

unsigned long *exception_handler(unsigned long *regs) {
   int ii;
   U32 *ramv;
   struct _reent *save_impure;

   save_impure = _impure_ptr;
   _impure_ptr = interrupt_reent;

#if DO_STACK_WATERMARK
   if ((U32)&ramv  > ((U32)&INIT_SP - 1024*1024)) { /* if not Nucleus stack */
	if ( (U32)&ramv < ((U32)&INIT_SP-stack_watermark) ) {
	    stack_watermark = (U32)&INIT_SP - (U32)&ramv;
	}
   }
#endif

   ramv = (U32*)romv_tbl.ROMV_RAMVBR;

   ++exception_count;
   if ((ii=(prc_get_cause()>>2)&0x1F) != 0) {
      void (*func)(U32 *);
      func = (void (*)(U32*))ramv[ii+VN_TLBM-1];
      if (func) {
	  func(regs);
      } else {	 
	  if (ii == 6 || ii == 7) {	/* if a bus error */
	     struct IIO_VECTOR_STR *s;
	     s = (struct IIO_VECTOR_STR *)romv_tbl.ROMV_STUB_VECS;
	     if (s) {
		 func = (void (*)(U32*))s->STUBVEC_FAKE_EH;	/* get pointer to stub's handler */
		 if (func) {
		    func(regs);	/* if there is one, go to the stub */
		    _impure_ptr = save_impure;
		    return regs; /* if he continues, he must have patched it */
		 }
	     }
	  }	     
          update_pm(regs);
	  wait_forever();
      }
   } else {
#if HOST_BOARD != CHAMELEON
      interrupt_handler(regs);
#else
      check_for_iceless_int(regs);
      interrupt_handler();
#endif
   }

   _impure_ptr = save_impure;
   return regs;
}

#if (HOST_BOARD == PHOENIX_AD) || (HOST_BOARD == FLAGSTAFF) || (HOST_BOARD == SEATTLE) || (HOST_BOARD == VEGAS)
# define EE_TYPE VU32 *
#else
# define EE_TYPE VU8 *
#endif

void eer_write_byte( EE_TYPE where, EE_TYPE unlock, int what) {
#if !BOOT_FROM_DISK && !NO_EER_WRITE && !EPROM_ST
   int old_sr;

   old_sr = prc_set_ipl(INTS_OFF);
   *unlock = 0;
   *where = what;
# ifdef FLUSH_WB
   FLUSH_WB();
# endif
   prc_set_ipl(old_sr);
#endif
   return;
}

int pktQueRecv(PktIOStruct *pkt) {
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
    struct IIO_VECTOR_STR *s;
    int (*func)(PktIOStruct *);
    int sts=0;

    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
    if (s) {
	func = (int (*)(PktIOStruct *))s->STUBVEC_PKTQRECV;
	if (func) {
	    sts = func(pkt);
	}
    }
    return sts;
}

int pktQueSend(PktIOStruct *pkt, int flag) {
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
    struct IIO_VECTOR_STR *s;
    int (*func)(PktIOStruct *);
    int sts=0;

    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
    if (s) {
	func = (int (*)(PktIOStruct *))s->STUBVEC_PKTQSEND;
	if (func) {
	    sts = func(pkt);
	}
    }
    return sts;
}

void *pktInit(void) {
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
    struct IIO_VECTOR_STR *s;
    void *(*func)(void);

    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
    
    if (s) {
	func = (void *(*)(void))s->STUBVEC_PKTINIT;
	if (func) {
	    return func();
	}
    }
    return 0;
}

PktIOStruct *pktPoll(int board, int flag, int channel, int thread) {
    struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;
    struct IIO_VECTOR_STR *s;
    PktIOStruct *(*func)(int, int, int, int);

    s = (struct IIO_VECTOR_STR *)romv->ROMV_STUB_VECS;
    if (s) {
	func = (PktIOStruct *(*)(int, int, int, int))s->STUBVEC_PKTPOLL;
	if (func) {
	    return func(board, flag, channel, thread);
	}
    }
    return 0;
}

#if !defined(EXTERN_GETENV)
char *getenv(const char *arg) {
    return 0;
}
#endif
