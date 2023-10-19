#ifndef SMC_REGS_H
/*
	Header file defining the various SMC Registers and their Banks.
	
	The macros SMC_BANK and SMC_REG extract the bank number and 
	register numbers, respectivly.
*/                                

#define SMC_REGS_H

#define SMC_BANK(Bnk_Reg) ((Bnk_Reg & 0x0300)>>8)
#define SMC_REG(Bnk_Reg)  (Bnk_Reg & 0x000f)

/*******************************************************************/
/*             Tx/Rx Control                                       */
/*******************************************************************/
/* Tranmit Control Regiter */
#define SMC_TCR 	(0x0000)
#define SMC_TCRMSK      (0x3d87)
/* Bit offsets in the TCR */
#define SMC_TX_EPHLOOP  (1<<13)
#define SMC_TX_STPSQET  (1<<12)
#define SMC_TX_FDUPLX   (1<<11)
#define SMC_TX_MONCSN   (1<<10)
#define SMC_TX_NOCRC    (1<<8)
#define SMC_TX_PADEN    (1<<7)
#define SMC_TX_FORCOL   (1<<2)
#define SMC_TX_LOOP     (1<<1)
#define SMC_TX_ENABLE	(1<<0)

#define SMC_EPHSR	(0x0002)
/* bit offsets in the EPHSR */
#define SMC_TX_UNRN     (1<<15)
#define SMC_LINK_OK     (1<<14)
#define SMC_RX_OVRN     (1<<13)
#define SMC_CTR_ROL     (1<<12)
#define SMC_EXC_DEF     (1<<11)
#define SMC_LOST_CAR    (1<<10)
#define SMC_LATCOL      (1<<9)
#define SMC_TX_DEFR     (1<<7)
#define SMC_LTX_BRD     (1<<6)
#define SMC_SQET        (1<<5)
#define SMC_16COL       (1<<4)
#define SMC_LTX_MULT    (1<<3)
#define SMC_MULCOL      (1<<2)
#define SMC_SNGLCOL     (1<<1)
#define SMC_TX_SUC      (1)

/* Receive Control Register */
#define SMC_RCR 	(0x0004)
#define SMC_RCRMSK      (0x4fff)
/* Bit offsets in the Receive Conrol Register */
#define SMC_RX_SOFT_RST  (1<<15)
#define SMC_RX_FLT_CAR   (1<<14)
#define SMC_RX_STRIP_CRC (1<<9)
#define SMC_RX_ENABLE    (1<<8)
#define SMC_RX_ALMUL     (1<<2)
#define SMC_RX_PRMS      (1<<1)
#define SMC_RX_ABORT     (1<<0)

/* Counter Register for Tx MAC Stats */
#define SMC_ECR 	(0x0006)

/* Memory Information and Memory Configuration Register */
#define SMC_MIR 	(0x0008)
#define SMC_MCR 	(0x000A)
#define SMC_MCRMSK      (0x001f)
#define SMC_BNK0REGS (6)
/*
	EEPROM / Chip Control
	Device Address and EEPROM Access.
	Device Control Register.	
*/
#define SMC_CR		(0x0100)
#define SMC_CONFIGMSK   (0x1746)
/* Bits in the chip configuration register */
#define SMC_CR_NOWAIT	(1<<12)
#define SMC_CR_FULLSTEP (1<<10)
#define SMC_CR_SETSQLCH (1<<9)
#define SMC_CR_AUISEL   (1<<8)
#define SMC_CR_16BIT	(1<<7)
#define SMC_CR_DISLINK	(1<<6)
#define SMC_CR_INTSEL1	(1<<2)
#define SMC_CR_INTSEL0	(1<<1)

#define SMC_BAR 	(0x0102)
#define SMC_IAR0b	(0x0104)
#define SMC_IAR1b	(0x0105)
#define SMC_IA10MSK     (0xffff)
#define SMC_IAR2b	(0x0106)
#define SMC_IAR3b	(0x0107)
#define SMC_IA32MSK     (0xffff)
#define SMC_IAR4b	(0x0108)
#define SMC_IAR5b	(0x0109)
#define SMC_IA54MSK     (0xffff)
#define SMC_GPR		(0x010A)
#define SMC_GENERALMSK  (0xffff)

#define SMC_CTR		(0x010C)
#define SMC_CONTROLMSK  (0x48e0)
/* Bit offsets in the Control Register */
#define SMC_CTR_RCVBAD	(1<<14)
#define SMC_CTR_PWRDN	(1<<13)
#define SMC_CTR_AUTRLS	(1<<11)
#define SMC_CTR_LEENA	(1<<7)
#define SMC_CTR_CRENA	(1<<6)
#define SMC_CTR_TEENA	(1<<5)
#define SMC_CTR_EESEL	(1<<2)
#define SMC_CTR_RELOAD	(1<<1)
#define SMC_CTR_STORE	(1<<0)

#define SMC_BNK1REGS (7)

/*******************************************************************/
/*      Run time Registers.                                        */
/*      These are all that is needed for normal operation.         */
/*      So bank-switching should not occur in normal ops.          */
/*******************************************************************/
/* MMU Command Register */
#define SMC_MMUCR       (0x0200)
#define SMC_MMU_NOP     (0<<5)
#define SMC_MMU_ALLOC   (1<<5)
#define SMC_MMU_RESET   (2<<5)
#define SMC_MMU_RXPOP   (3<<5)
#define SMC_MMU_RXRLS   (4<<5)
#define SMC_MMU_PKTRLS  (5<<5)
#define SMC_MMU_TXPUSH  (6<<5)
#define SMC_MMU_TXRESET (7<<5)
#define SMC_MMU_BUSY    (1)


#define SMC_PNR 	(0x0202)
#define SMC_PNRb	(0x0202)
#define SMC_APRb	(0x0203)
#define SMC_ARRPNRMSK   (0x1f00)
/* FIFO register, both Rx and TX */
#define SMC_FIFO	(0x0204)
#define SMC_FIFO_RXMSK  (0x7f00)
#define SMC_FIFO_RXEMPY (0x8000)
#define SMC_FIFO_TXMSK  (0x007f)
#define SMC_FIFO_TXEMPY (0x0080)
/* The data pointer register. There are constants that apply to this one */
#define SMC_PTR		(0x0206)
#define SMC_POINTERMSK  (0xe7fe)
#define SMC_PTR_RCV     (1<<15)
#define SMC_PTR_AUTO_INC (1<<14)
#define SMC_PTR_READ    (1<<13)
#define SMC_PTR_ETEN    (1<<12)

#define SMC_DATA	(0x0208)
#define SMC_DATA1MSK    (0xffff)
#define SMC_DATA_LOWb   (0x0209)
#define SMC_DATA2	(0x020A)
#define SMC_DATA2MSK    (0xffff)
/* Interrupt stuffs - note some are BYTE registers !    */
#define SMC_ISR         (0x020C)
#define SMC_ISRMSK      (0x3f00)
#define SMC_ISTb	(0x020C)
#define SMC_ACKb	(0x020C)
#define SMC_MSKb	(0x020D)

/* Various interuupt flags that can be set/cleared in the SMC_MSKb */
#define SMC_ERCV_INT            (1<<6)
#define SMC_EPH_INT             (1<<5)
#define SMC_RXOVRN_INT          (1<<4)
#define SMC_ALLOC_INT           (1<<3)
#define SMC_TX_EMPTY_INT        (1<<2)
#define SMC_TX_INT              (1<<1)
#define SMC_RCV_INT             (1<<0)

#define SMC_ALL_INTS    (SMC_ERCV_INT | SMC_EPH_INT |SMC_RXOVRN_INT | \
                         SMC_ALLOC_INT |SMC_TX_EMPTY_INT | SMC_TX_INT | \
                         SMC_RCV_INT)

/* Bit offsets to important stuff in the receive status word. */
#define SMC_ALGN_ERR        (1<<15)
#define SMC_BROADCAST       (1<<14)
#define SMC_BADCRC          (1<<13)
#define SMC_ODDFRM          (1<<12)
#define SMC_TOO_LONG        (1<<11)
#define SMC_TOO_SHORT       (1<<10)
#define SMC_MULTICAST       (1)

#define SMC_RCV_ERROR       (SMC_ALGN_ERR | SMC_BADCRC | SMC_TOO_LONG | SMC_TOO_SHORT)

#define SMC_BNK2REGS (7)

 
/******************************************************************/
/*      The multicast table.                                      */
/*      Each byte is treated as a bitmask selecting one group of  */
/*      multicasts to be accepted.So there are 64 total groups.   */
/******************************************************************/

#define SMC_MT0b	(0x0300)
#define SMC_MT10	(0x0300)
#define SMC_MT1b	(0x0301)
#define SMC_MT10MSK     (0xffff)
#define SMC_MT2b	(0x0302)
#define SMC_MT32	(0x0302)
#define SMC_MT3b	(0x0303)
#define SMC_MT32MSK     (0xffff)
#define SMC_MT4b	(0x0304)
#define SMC_MT54	(0x0304)
#define SMC_MT5b	(0x0305)
#define SMC_MT54MSK     (0xffff)
#define SMC_MT6b	(0x0306)
#define SMC_MT76	(0x0306)
#define SMC_MT7b	(0x0307)
#define SMC_MT76MSK     (0xffff)

/*  This bank has more if it is a 91C94, but it seems that at least the */
/*  revision register is an undocmented feature in the 91C92 as well... */

/*  Management Interface.   Stuff for the transceiver control, Not      */
/*  documented enough to use...                                         */
#define SMC_MGMT        (0x0308)

/* Revision Register */
#define SMC_REVS        (0x030A)
/* Offset masks into revs    */
#define SMC_CHIP_ID     (0xF <<4)
#define SMC_CHIP_RV     (0xF <<0)
/* Constants in Revs            */
#define SMC_VERS_92     (3<<4)
#define SMC_VERS_94     (4<<4)
#define SMC_VERS_95     (5<<4)
#define SMC_VERS_100    (7<<4)

/* Early Receive Register */
#define SMC_ERCV        (0x030C)
#define SMC_RCV_DISCRD  (1<<7)

#define SMC_BNK3REGS (7)


/* Bank Select Register. 
	The last 2 bytes of every bank are bank select registers.
	So there are actually 4, as detailed below. Therefore
	whatever bank is selected, the BSR can be read and written.
	This then needs special handling in the code.
*/	

#define	SMC_BSR0	(0x000E)
#define	SMC_BSR1	(0x010E)
#define	SMC_BSR2	(0x020E)
#define	SMC_BSR3	(0x030E)
#define	SMC_BSR         SMC_BSR2

#define SMC_BSR_MSK     (0x03)

#define SMC_MT10MSK     (0xffff)
#define SMC_MT32MSK     (0xffff)
#define SMC_MT54MSK     (0xffff)
#define SMC_MT76MSK     (0xffff)

#endif
