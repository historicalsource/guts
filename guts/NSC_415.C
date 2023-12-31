/* 	$Id: nsc_415.c,v 1.2 1996/05/07 20:24:28 shepperd Exp $	 */

#if 0 && !defined(lint)
static const char vcid[] = "$Id: nsc_415.c,v 1.2 1996/05/07 20:24:28 shepperd Exp $";
#endif /* lint */
/****************************************************************************
    Copyright (C) 1995 National Semiconductor Corp.  All Rights Reserved
*****************************************************************************
*
*   File:               NSC_415.C
*
*   Purpose:            Functions for handling the NSC PC87415 IDE
*                       controller chip.
*
*   Update History:      1/19/95 GDV Created get/set registers
*                        1/24/95 MF  Created calc registers
*                        4/23/96 DMS Change names of types
*
****************************************************************************/

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>

#include <nsc_defines.h>		/* Common C Definitions */
#include <nsc_pcicfig.h>		/* PCI Configuration Routines */
#include <nsc_415.h>			/* NSC PC87415 IDE Definitions */
#include <nsc_drvdisp.h>		/* IDE Driver Display Definitions */
#include <nsc_gt64010.h>		/* there is only one thing wanted out of here */

#ifndef NUM_HDRIVES
# define NUM_HDRIVES	(1)        	/* default to number of IDE devices supported */
#endif

#define PHYS(x) ((U32)(x)&0x1FFFFFFF)

/* timing limits for the PC87415: */
#define DBA_CLKMIN              2       /* Data Byte Active */
#define DBA_CLKMAX              17
#define DBR_CLKMIN              1       /* Data Byte Recover */
#define DBR_CLKMAX              16

#define CBA_CLKMIN              3       /* Command Byte Active */
#define CBA_CLKMAX              17
#define CBR_CLKMIN              3       /* Command Byte Recover */
#define CBR_CLKMAX              18

PUBLIC void NSC415_GetCurrentRegs   /* Gets current '415 IDE register values */
(
    DEVHANDLE       devhand,            /* handle for device access */
    NSC415_Regs *   regptr,             /* ptr to returned PC87415 reg values */
    BOOL            getdma              /* TRUE to get bus master DMA registers */
)
{
    UINT            i;                  /* loop counter */
    BYTE *          dbptr;              /* pointer to bytes in structure */
    WORD            baseadr;            /* base address for bus master regs */
    DWORD	    tmp;

    dbptr = (BYTE *)&(regptr->nsc_ConfigRegs);

    for (i = 0; i < sizeof(N415_CfigRegs); ++i, ++dbptr)
        *dbptr = PCI_ReadConfigByte(devhand, (BYTE)i);

    if (getdma)
    {
      /* BAR4 always reads back with bit 0 set */
        baseadr = ((WORD)regptr->nsc_ConfigRegs.n415_BaseAddr4) & 0x0FFFE;

        dbptr = (BYTE *)&(regptr->nsc_BusMastRegs);

        for (i = 0; i < sizeof(N415_MastRegs); ++i, ++dbptr, ++baseadr) {
	  tmp =  *dbptr = (*(int *)((baseadr + PCI_IO_BASE) & 0xfffffffc));
	  *dbptr = (BYTE)(tmp >> (8 * ((baseadr + PCI_IO_BASE) & 0x03)));
	}
    }
}

PUBLIC void NSC415_SetRegValues     /* Sets actual '415 IDE controller regs */
(
    DEVHANDLE       devhand,            /* handle for device access */
    NSC415_Regs *   regptr              /* ptr to values to set PC87415 regs */
)
{
    N415_CfigRegs * figptr;             /* pointer to configuration registers */

    figptr  = &(regptr->nsc_ConfigRegs);

    PCI_WriteConfigWord(devhand, (BYTE)((DWORD)&(figptr->n415_Command  ) - (DWORD)figptr), figptr->n415_Command);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_ProgIface) - (DWORD)figptr), figptr->n415_ProgIface);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_Latency  ) - (DWORD)figptr), figptr->n415_Latency);

#if 1
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[0]) - (DWORD)figptr), figptr->n415_Control[0]);
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[1]) - (DWORD)figptr), figptr->n415_Control[1]);
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[2]) - (DWORD)figptr), figptr->n415_Control[2]);
#else
    PCI_WriteConfigDword( devhand, (BYTE)((DWORD)&(figptr->n415_Control[0]) - (DWORD)figptr), *(DWORD*)figptr->n415_Control);
#endif
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr0)  - (DWORD)figptr), figptr->n415_BaseAddr0);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr1)  - (DWORD)figptr), figptr->n415_BaseAddr1);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr2)  - (DWORD)figptr), figptr->n415_BaseAddr2);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr3)  - (DWORD)figptr), figptr->n415_BaseAddr3);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr4)  - (DWORD)figptr), figptr->n415_BaseAddr4);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D1_Dread ) - (DWORD)figptr), figptr->n415_C1D1_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D1_Dwrite) - (DWORD)figptr), figptr->n415_C1D1_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D2_Dread ) - (DWORD)figptr), figptr->n415_C1D2_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D2_Dwrite) - (DWORD)figptr), figptr->n415_C1D2_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D1_Dread ) - (DWORD)figptr), figptr->n415_C2D1_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D1_Dwrite) - (DWORD)figptr), figptr->n415_C2D1_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D2_Dread ) - (DWORD)figptr), figptr->n415_C2D2_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D2_Dwrite) - (DWORD)figptr), figptr->n415_C2D2_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_CmdCtrl_RdWrt) - (DWORD)figptr), figptr->n415_CmdCtrl_RdWrt);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_SectorSize   ) - (DWORD)figptr), figptr->n415_SectorSize);
}

PUBLIC void NSC415_InitRegValues    /* Initializes values for '415 IDE regs */
(
    BOOL *          dodrive,            /* TRUE for each drive to initialize */
    NSC415_Regs *   regptr              /* ptr to returned PC87415 reg values */
)
{
    N415_CfigRegs * nptr;               /* pointer to configuration registers */
    N415_MastRegs * bptr;               /* pointer to bus master registers */

    nptr = &(regptr->nsc_ConfigRegs);

    nptr->n415_Command    = 0x0145;     /* enable bus master and errors */
    nptr->n415_SectorSize = 0xEE;       /* sector size = 512 bytes */

    nptr->n415_ProgIface   =  0x8A;     /* enable master IDE and legacy mode */
    nptr->n415_Control[0]  =  0x70;	/* No write to Vendor ID regs, mask INTA ... */
    					/* map both ints to INTA, turn on PWR, turn off IDE resets */
    nptr->n415_Control[1] &= ~0x0B;     /* disable data phase watchdog, unmask both interrupts */
    nptr->n415_Control[1] |=  0x03;     /* mask both interrupts */

#if 0
# define M0_TIMING	(0xEF)
# define CC_TIMING	(0xF0)
#else
# define M0_TIMING	(0x1B)
# define CC_TIMING	(0xB7)
#endif

    if (dodrive[0] || dodrive[1])       /* configure the first channel */
    {
        nptr->n415_ProgIface  &= ~0x01; /* use legacy mode, not BAR 0,1 */
        nptr->n415_Control[1] &= ~0x48; /* map IDE to BAR 0,1, disable watchdog */
        nptr->n415_Control[1] |=  0x11; /* mask int, buffer BAR 0,1 accesses */
        nptr->n415_Control[2] |=  0x01; /* enable buffers for first channel */
	nptr->n415_BaseAddr0 = PHYS(PCI_IO_BASE) + 0x400;	/* set the base registers */
	nptr->n415_BaseAddr1 = PHYS(PCI_IO_BASE) + 0x408;

        if (dodrive[0]) {
	    nptr->n415_Control[2] &= ~0x10; /* use IORDY for first drive */
	    nptr->n415_C1D1_Dread  = M0_TIMING;  /* use mode 0 timings */
	    nptr->n415_C1D1_Dwrite = M0_TIMING;
	}

        if (dodrive[1]) {
	    nptr->n415_Control[2] &= ~0x20; /* use IORDY for second drive */
	    nptr->n415_C1D2_Dread  = M0_TIMING;
	    nptr->n415_C1D2_Dwrite = M0_TIMING;
	}
    }

    if (dodrive[2] || dodrive[3])       /* configure the second channel */
    {
        nptr->n415_ProgIface  &= ~0x04;	/* use legacy mode, not BAR 2,3 */
        nptr->n415_Control[1] &= ~0x8C; /* map IDE to BAR 2,3, disable watchdog */
        nptr->n415_Control[1] |=  0x22; /* mask int, buffer BAR 2,3 accesses */
        nptr->n415_Control[2] |=  0x02; /* enable buffers for second channel */
	nptr->n415_BaseAddr2 = PHYS(PCI_IO_BASE) + 0x410;	/* set the base registers */
	nptr->n415_BaseAddr3 = PHYS(PCI_IO_BASE) + 0x418;
	nptr->n415_Control[1] &= ~0x0B;     /* disable data phase watchdog, unmask both interrupts */

        if (dodrive[2]) {
	    nptr->n415_Control[2] &= ~0x40; /* use IORDY for first drive */
	    nptr->n415_C2D1_Dread  = M0_TIMING;
	    nptr->n415_C2D1_Dwrite = M0_TIMING;
	}

        if (dodrive[3]) {
	    nptr->n415_Control[2] &= ~0x80; /* use IORDY for second drive */
	    nptr->n415_C2D2_Dread  = M0_TIMING;
	    nptr->n415_C2D2_Dwrite = M0_TIMING;
	}
    }

    nptr->n415_CmdCtrl_RdWrt = CC_TIMING;

    nptr->n415_BaseAddr4 = PHYS(PCI_IO_BASE) + NSC415_DEFVAL_BAR4;

    bptr = &(regptr->nsc_BusMastRegs);

    bptr->n415_Mast1_Cmd  = 0x00;       /* stop any DMA transfers */
    bptr->n415_Mast1_Stat = 0x06;       /* reset error/interrupts */

    bptr->n415_Mast2_Cmd  = 0x00;       /* stop any DMA transfers */
    bptr->n415_Mast2_Stat = 0x06;       /* reset error/interrupts */
}

#if 0
static int dump_regs(NSC415_Regs *dregs, int row) {
    N415_CfigRegs *regs;

    regs = &dregs->nsc_ConfigRegs;

    txt_str(1, row++, "Vendor ", WHT_PAL);
    txt_chexnum(regs->n415_VendorID, 4, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_DeviceID, 4, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "CMD ", WHT_PAL);
    txt_chexnum(regs->n415_Command, 4, RJ_ZF, WHT_PAL);
    txt_cstr("  Status ", WHT_PAL);
    txt_chexnum(regs->n415_Status, 4, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "Revison ", WHT_PAL);
    txt_chexnum(regs->n415_RevisionID, 2, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_ProgIface, 2, RJ_ZF, WHT_PAL);
    txt_cstr(" SubClass ", WHT_PAL);
    txt_chexnum(regs->n415_SubClass, 2, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "ClassCode ", WHT_PAL);
    txt_chexnum(regs->n415_ClassCode, 2, RJ_ZF, WHT_PAL);
    txt_cstr(" Latency ", WHT_PAL);
    txt_chexnum(regs->n415_Latency, 2, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_HeadType, 2, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "BAddr0 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr0, 8, RJ_ZF, WHT_PAL);
    txt_cstr("     BAddr1 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr1, 8, RJ_ZF, WHT_PAL);
    txt_str(1, row++, "BAddr2 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr2, 8, RJ_ZF, WHT_PAL);
    txt_cstr("     BAddr3 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr3, 8, RJ_ZF, WHT_PAL);
    txt_str(1, row++, "BAddr4 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr4, 8, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "CmdCtl " , WHT_PAL);
    txt_chexnum(regs->n415_CmdCtrl_RdWrt, 2, RJ_ZF, WHT_PAL);
    txt_cstr("     SectorSize " , WHT_PAL);
    txt_chexnum(regs->n415_SectorSize, 2, RJ_ZF, WHT_PAL);

    return row;
}

typedef volatile struct ide_regs {
    unsigned char data;
    unsigned char err_precomp;
    unsigned char sect_cnt;
    unsigned char sect_numb;
    unsigned char low_cyl;
    unsigned char hi_cyl;
    unsigned char drv_hd;
    unsigned char csr;
    unsigned char filler[0x1FE];
    unsigned char di_fd;
    unsigned char alt_sts;
} IdeRegs;
#endif

void setup_harddrive(void) {
    NSC415_Regs   disk_controller_regs;
    BOOL          drives[3];

/* grab registers */
    NSC415_GetCurrentRegs(PC87415_DEVICE_NUMBER, \
                        &disk_controller_regs, (BOOL)FALSE);

/* initialize the NS87415 */
    drives[0] = (NUM_HDRIVES > 0);
    drives[1] = (NUM_HDRIVES > 1);
    drives[2] = (NUM_HDRIVES > 2);
    drives[3] = (NUM_HDRIVES > 3);

    NSC415_InitRegValues(&drives[0], &disk_controller_regs);

/* store everything back */
    NSC415_SetRegValues(PC87415_DEVICE_NUMBER, &disk_controller_regs);

    return;
}

void nsc_drvconfig(int reg, U32 value) {
    PCI_WriteConfigDword(PC87415_DEVICE_NUMBER, reg, value);
    return;
}

/***************************************************************************/
