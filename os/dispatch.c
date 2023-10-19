
/*		dispatch.c
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 * ++++++ WARNING WARNING WARNING WARNING WARNING +++++
 * This file is machine generated. Any changes you put in here will be lost
 * the next time phx_vecs.mac is touched. You need to make your changes to
 * phx_vecs.mac instead of this file.
 */
#include <config.h>			/* begining of dispatch.c */
#include <os_proto.h>
#include <intvecs.h>

#ifndef BLABF
# define BLABF(x)
#endif

#ifndef BLAB
# define BLAB(x)
#endif

#define UART_NOTES IO_MAIN_GLOBAL_INT
#define SND_NOTES IO_MAIN_GLOBAL_INT
extern void sst_prc_delay(void);
void (*set_ipl_vec)();	/*  prc_set_ipl soft vector  */
static void int0_irq(void);	/*  Hardware interrupt 0  */
static void int1_irq(void);	/*  Hardware interrupt 1  */
static void int3_irq(void);	/*  Hardware interrupt 3  */
static void int4_irq(void);	/*  Hardware interrupt 4  */
static void int5_irq(void);	/*  Hardware interrupt 5  */
void (*timervec)();	/*  Soft Timer interrupt  */
void (*framevec)();	/*  Frame complete  */
void (*expintp)();	/*  Expansion Slot interrupt  */
void (*nssintp)();	/*  NSS/Hi-Link interrupt */
void (*dbgintp)();	/*  Debug switch interrupt  */
void (*vsyintp)();	/*  VSYNC interrupt  */
void (*dm0intp)();	/*  Galileo DMA0Comp  */
void (*tm3intp)();	/*  Galileo T3EXP timer interrupt  */
extern void cputimerint(void);
void (*uartintp)();	/*  I/O ASIC UART interrupts  */
void (*sndintp)();	/*  I/O ASIC Sound interrupts  */
static void (*hwvinst(const struct irq_tab *, void (*)()))();
static void (*pvinst(const struct irq_tab *, void (*)()))();
static void (*gvinst(const struct irq_tab *, void (*)()))();
static void (*ioainst(const struct irq_tab *, void (*)()))();
static const struct irq_tab irq_inits[] = {
 { (void (**)())&gameint, 0, 0 },	/*  post-wierdness Vblank  */
 { (void (**)())&ms4vec, 0, 0 },	/*  4 Millisecond timer  */
 { (void (**)())&prc_delay_vec, 0, 0 },	/*  prc_delay soft vector  */
 { (void (**)())&set_ipl_vec, 0, 0 },	/*  prc_set_ipl soft vector  */
 { (void (**)())&savintp, 0, 0 },	/*  context save soft vector  */
 { (void (**)())&restintp, 0, 0 },	/*  context restore soft vector  */
 { 0, hwvinst, VN_IRQ0 },	/*  Software interrupt 0  */
 { 0, hwvinst, VN_IRQ1 },	/*  Software interrupt 0  */
 { 0, hwvinst, VN_IRQ2 },	/*  Hardware interrupt 0  */
 { 0, hwvinst, VN_IRQ3 },	/*  Hardware interrupt 1  */
 { 0, hwvinst, VN_IRQ5 },	/*  Hardware interrupt 3  */
 { 0, hwvinst, VN_IRQ6 },	/*  Hardware interrupt 4  */
 { 0, hwvinst, VN_IRQ7 },	/*  Hardware interrupt 5  */
 { 0, hwvinst, VN_TLBM },	/*  TLB modification  */
 { 0, hwvinst, VN_TLBL },	/*  TLB miss on I or D fetch  */
 { 0, hwvinst, VN_TLBS },	/*  TLB miss on store  */
 { 0, hwvinst, VN_ADDRL },	/*  Address error on I or D fetch  */
 { 0, hwvinst, VN_ADDRS },	/*  Address error on store  */
 { 0, hwvinst, VN_BUSERRI },	/*  Bus error on I-fetch  */
 { 0, hwvinst, VN_BUSERRD },	/*  Bus error on D-fetch  */
 { 0, hwvinst, VN_SYSCALL },	/*  SYSCALL instruction  */
 { 0, hwvinst, VN_BREAK },	/*  Breakpoint instruction  */
 { 0, hwvinst, VN_RESERV },	/*  Reserved instruction  */
 { 0, hwvinst, VN_COPROC },	/*  Coprocessor unusable  */
 { 0, hwvinst, VN_OVERFL },	/*  Integer Arithmetic Overflow  */
 { 0, hwvinst, VN_TRAPV },	/*  TRAP exception  */
 { 0, hwvinst, VN_FPE },	/*  FLoating point exception  */
 { (void (**)())&timervec, 0, 0 },	/*  Soft Timer interrupt  */
 { (void (**)())&tpllvec, 0, 0 },	/* Adjust 1MS timer */
 { (void (**)())&framevec, 0, 0 },	/*  Frame complete  */
 { 0, hwvinst, VN_IRQ4 },	/*  IDE interrupt  */
 { (void (**)())&expintp, pvinst, EXP_NOTES },	/*  Expansion Slot interrupt  */
 { (void (**)())&nssintp, pvinst, NSS_NOTES },	/*  NSS/Hi-Link interrupt */
 { (void (**)())&dbgintp, pvinst, DBG_NOTES },	/*  Debug switch interrupt  */
 { (void (**)())&vsyintp, pvinst, VSY_NOTES },	/*  VSYNC interrupt  */
 { (void (**)())&dm0intp, gvinst, DM0_NOTES },	/*  Galileo DMA0Comp  */
 { (void (**)())&tm3intp, gvinst, TM3_NOTES },	/*  Galileo T3EXP timer interrupt  */
 { (void (**)())&uartintp, ioainst, UART_NOTES },	/*  I/O ASIC UART interrupts  */
 { (void (**)())&sndintp, ioainst, SND_NOTES },	/*  I/O ASIC Sound interrupts  */
 { 0, 0, 0} };

extern void (*ramv_tbl[])();

static void (*hwvinst(const struct irq_tab *it, void (*new)()))() {
    int indx;
    void (*old_rtn)();

    indx = it->notes;
    if (indx >= VN_MAX) return 0;
    old_rtn = ramv_tbl[indx];
    ramv_tbl[indx] = new;
    return old_rtn;
}

static void (*pvinst(const struct irq_tab *it, void (*new)()))() {

    if (it->notes) {
	BLABF(("\ndispatch: Adding 0x%08lX to main INTCTL (0x%08lX)...", it->notes, *(VU32*)INTCTL_IE));
	*((VU32 *)(INTCTL_IE)) |= it->notes;
    }
    return 0;
}

static void (*gvinst(const struct irq_tab *it, void (*new)()))() {

    if (it->notes) {
	BLABF(("\ndispatch: Adding 0x%08lX to Galileo INTCTL (0x%08lX)...", it->notes, \
			*(VU32*)GALILEO_CPU_I_ENA));
	*((VU32 *)(GALILEO_CPU_I_ENA)) |= it->notes;
    }
    return 0;
}

static void (*ioainst(const struct irq_tab *it, void (*new)()))() {
extern void prc_wait_n_usecs( int );
    if (it->notes)
    {
     BLABF(("\ndispatch: Adding 0x%04X to IOASIC INTCTL: (0x%04X)...", (int)it->notes, \
		*(VU32*)IO_MAIN_CTL&0xFFFF));
     while ( ( *((VU32 *)IO_MAIN_CTL) & it->notes ) != it->notes )
     {
#ifdef LED_OUT
      *(VU32 *)LED_OUT = ~( 1 << B_LED_RED );
#endif
      do
      {
       *((VU32 *)IO_MAIN_CTL) |= it->notes;
       prc_wait_n_usecs( 1000 );
      } while ( ( *((VU32 *)IO_MAIN_CTL) & it->notes ) != it->notes );
      prc_wait_n_usecs( 1000 );
#ifdef LED_OUT
      *(VU32 *)LED_OUT |= ( 1 << B_LED_RED );
#endif
     }
    }
    return 0;
}

void (*prc_set_vec (unsigned int vecnum, void (*routine)() ))()
{
    void (*old_rtn)();
    void (**softvec)();
    const struct irq_tab *tbp;
    int old_ipl;

    if ( vecnum >= N_INTVECS ) return 0;
    tbp = irq_inits+vecnum;
    softvec = tbp->softvec;
    old_rtn = 0;
    old_ipl = prc_set_ipl(INTS_OFF);
    if ( tbp->installer ) old_rtn = tbp->installer(tbp, routine);
    if ( softvec ) {
	if (!old_rtn) old_rtn = *softvec;
	*softvec = routine;
    }
    prc_set_ipl(old_ipl);
    return old_rtn;
}

static void int0_irq(void) {
    if (dm0intp) dm0intp();
    if (tm3intp) tm3intp();
    return;
}

static void int1_irq(void) {
    if (uartintp) uartintp();
    if (sndintp) sndintp();
    return;
}

static void int3_irq(void) {
    if (vsyintp) vsyintp();
    return;
}

static void int4_irq(void) {
    if (nssintp) nssintp();
    return;
}

static void int5_irq(void) {
    if (expintp) expintp();
    if (dbgintp) dbgintp();
    return;
}

void prc_init_vecs(void) {
    prc_set_vec(PRC_DELAY_INTVEC, sst_prc_delay);
    prc_set_vec(INT0_INTVEC, int0_irq);
    prc_set_vec(INT1_INTVEC, int1_irq);
    prc_set_vec(INT3_INTVEC, int3_irq);
    prc_set_vec(INT4_INTVEC, int4_irq);
    prc_set_vec(INT5_INTVEC, int5_irq);
    prc_set_vec(TM3_INTVEC, cputimerint);

/* Map the Phoenix interrupts */

    BLABF(("\ndispatch: Mapping the A interrupts: 0x%08lX...", INTCTL_MAPA_INIT));
    *((VU32 *)(INTCTL_MAPA)) = INTCTL_MAPA_INIT;

    {
	int jj;
	extern int prc_get_cause(void), prc_get_ipl(void);
	jj = prc_get_cause();
	if (jj&0xFF00) BLABF(("\ndispatch: CPU cause reg: %08lX, CPU SR reg: %08lX", jj, prc_get_ipl()));
	if (jj&0x0400) {
	    if (!dm0intp) BLAB("\ndispatch: Warning, IRQ 0 pending \" Galileo DMA0Comp \"");
	    if (!tm3intp) BLAB("\ndispatch: Warning, IRQ 0 pending \" Galileo T3EXP timer interrupt \"");
	}
	if (jj&0x0800) {
	    if (!uartintp) BLAB("\ndispatch: Warning, IRQ 1 pending \" I/O ASIC UART interrupts \"");
	    if (!sndintp) BLAB("\ndispatch: Warning, IRQ 1 pending \" I/O ASIC Sound interrupts \"");
	}
	if (jj&0x1000) {
	    BLAB("\ndispatch: Warning, IRQ 2 pending with no vector assigned");
	}
	if (jj&0x2000) {
	    if (!vsyintp) BLAB("\ndispatch: Warning, IRQ 3 pending \" VSYNC interrupt \"");
	}
	if (jj&0x4000) {
	    if (!nssintp) BLAB("\ndispatch: Warning, IRQ 4 pending \" NSS/Hi-Link interrupt\"");
	}
	if (jj&0x8000) {
	    if (!expintp) BLAB("\ndispatch: Warning, IRQ 5 pending \" Expansion Slot interrupt \"");
	    if (!dbgintp) BLAB("\ndispatch: Warning, IRQ 5 pending \" Debug switch interrupt \"");
	}
    }
    return;
}
