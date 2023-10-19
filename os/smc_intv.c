/**********************************************************************/
/*File:           smc_intv.c                                          */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       SMC Interrupt handeling functions                   */
/*                                                                    */
/*(C) Copyright Time Warner Interactive 1995                          */
/*                                                                    */
/*      This file implements the device interrupt service functions.  */
/*      Some support is also found in SMC_RX.                         */
/**********************************************************************/

#include <config.h>
#include <os_proto.h>
#include <smc_regs.h>
#include <smc_data.h>
#include <net_smc.h>
#include <net_link.h>
#include <smc_link.h>
#include <smc_dma.h>
#include <string.h>
#include <phx_proto.h>

static void SMC_TXHndlr(SMC_Data *smc);
static void SMC_Allocate(SMC_Data *smc);
static void SMC_EPHHndlr(SMC_Data *smc);

extern struct act_q smc_act_q_elt;

void SMC_Interrupt_Vector0(int vector)
{
#if WB_BASE
    int old_base = *(VU16 *)WB_BASE;
#endif
 smc_data[0].control.curr_bank = SMC_READ_BANK(smc_data[0].port);

 if (smc_data[0].control.curr_bank != 2)
 {
  SMC_SET_BANK(smc_data[0].port,2);
 }

 smc_data[0].control.curr_pointer = SMC_READ_REG(smc_data[0].port,SMC_PTR);

 smc_data[0].control.last_ISR = SMC_READ_REG(smc_data[0].port,SMC_ISR);
 SMC_WRITE_REG(smc_data[0].port,SMC_ISR,0);

 smc_data[0].control.int_posted++;
 smc_data[0].control.int_sem++;

 prc_q_action(&smc_act_q_elt);
#if WB_BASE
    *(VU16 *)WB_BASE = old_base;
#endif
}  /* SMC_Interrupt_Vector0 */

#if INCLUDE_TS_ARRAY
struct time_stamp ts_array[ N_TS ];
U32 ts_aix;
#endif

void Net_Interrupt_Handler(void)
{
 U16 old_ISR;
 int old_ipl;
#if WB_BASE
    int old_base = *(VU16 *)WB_BASE;
#endif

 smc_data[0].p_stats.ints++;

 old_ISR = smc_data[0].control.last_ISR & 0xff;
 old_ISR &= smc_data[0].control.last_ISR >>8;

 do
 {
#if INCLUDE_TS_ARRAY
  ts_array[ ts_aix %= N_TS ].isr = old_ISR;
  ts_array[ ts_aix ].qs = 0;
  ts_array[ ts_aix++ ].t = (float)prc_get_count()/75000000.0;
#endif
  if (old_ISR & SMC_RXOVRN_INT)
  {
   smc_data[0].e_stats.dropped++;
   SMC_RXError(&smc_data[0]);
  }
  if (old_ISR & SMC_EPH_INT)
  {
   smc_data[0].p_stats.ephints++;
   SMC_EPHHndlr(&smc_data[0]);
  }
  if (old_ISR & (SMC_TX_INT | SMC_TX_EMPTY_INT))
  {
   if ( old_ISR & SMC_TX_INT )  
   {
    smc_data[0].p_stats.txints++;
    SMC_TXHndlr(&smc_data[0]);
   }
   if ( old_ISR & SMC_TX_EMPTY_INT ) 
   {
    SMC_WRITE_REG(smc_data[0].port,SMC_ISR,SMC_TX_EMPTY_INT);
    SMC_Set_Int_Mask(&smc_data[0], SMC_TX_EMPTY_INT , 0);
   }
  }
  if (old_ISR & ( SMC_RCV_INT | SMC_ERCV_INT))
  {
   smc_data[0].p_stats.rxints++;
   SMC_RXHndlr(&smc_data[0]);
  }
  if (old_ISR & SMC_ALLOC_INT)
  {
   smc_data[0].p_stats.alcints++;
   SMC_Allocate(&smc_data[0]);
  }

  SMC_SET_BANK(smc_data[0].port,2);
  old_ISR = SMC_READ_REG(smc_data[0].port,SMC_ISR) & smc_data[0].control.Int_Mask;

 } while (old_ISR);

#if INCLUDE_TS_ARRAY
 ts_array[ ts_aix %= N_TS ].isr = old_ISR;
 ts_array[ ts_aix ].qs = 0;
 ts_array[ ts_aix++ ].t = (float)prc_get_count()/75000000.0;
#endif

 smc_data[0].control.int_sem--;
 smc_data[0].control.int_cleared++;

 old_ipl = prc_set_ipl(INTS_OFF);
 SMC_WRITE_REG(smc_data[0].port,SMC_PTR,smc_data[0].control.curr_pointer);
 SMC_WRITE_REG(smc_data[0].port,SMC_ISR,smc_data[0].control.Int_Mask<<8);
 if (smc_data[0].control.curr_bank != 2)
 {
  SMC_SET_BANK(smc_data[0].port,smc_data[0].control.curr_bank);
 }
 prc_set_ipl(old_ipl);
#if WB_BASE
    *(VU16 *)WB_BASE = old_base;
#endif
} /* Net_Interrupt_Handler */

static void SMC_TXHndlr(SMC_Data *smc)
{
 U16 old_Ptr, old_Pnr, tx_pkt_no; 

 old_Ptr = SMC_READ_REG(smc->port,SMC_PTR);
 old_Pnr = SMC_READ_REG(smc->port,SMC_PNR);

 do
 { 
  tx_pkt_no = SMC_READ_REG(smc->port,SMC_FIFO) & 0xff;

  SMC_WRITE_REG(smc->port,SMC_ISR,SMC_TX_INT);

  if ( (tx_pkt_no & 0x80) == 0 )
  {
   U8 pkt_num = tx_pkt_no & 0x7f;
   int ii;
   for ( ii = 0; ii < N_PKTS; ++ii )
   {
    if ( pn_array[ ii ].pktn == pkt_num )
    {
#if INCLUDE_TS_ARRAY
 ts_array[ ts_aix %= N_TS ].qs = 's';
 ts_array[ ts_aix ].isr = 0;
 ts_array[ ts_aix++ ].t = (float)prc_get_count()/75000000.0;
#endif
     ++pn_array[ ii ].sent;
     break;
    }
   }
  }
  else
  {
   smc->control.drv_tx_errs++;
  }
 } while (SMC_READ_REG(smc->port,SMC_ISR) & SMC_TX_INT);

 SMC_Set_Int_Mask(smc, SMC_TX_EMPTY_INT, 0);
}

void SMC_Allocate(SMC_Data *smc)
{
 U16 arr_pnr = SMC_READ_REG(smc->port,SMC_PNR);

 SMC_WRITE_REG(smc->port,SMC_PNR,arr_pnr >> 8);
 SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_PKTRLS);

 SMC_Set_Int_Mask(smc,SMC_ALLOC_INT,0);

 SMC_WRITE_REG(smc->port,SMC_PNR,arr_pnr);
} /* SMC_Allocate */

static void SMC_EPHHndlr(SMC_Data *smc)
{
 U16 this_tcr, this_ephsr;

 SMC_SET_BANK(smc->port,SMC_BANK(SMC_TCR));
 this_tcr = SMC_READ_REG(smc->port,SMC_TCR);
 this_ephsr = SMC_READ_REG(smc->port,SMC_EPHSR);

 if ( ( this_tcr & SMC_TX_ENABLE ) == 0 )
 {
  if (this_ephsr & SMC_TX_UNRN ) smc->e_stats.txunrn ++;
  if (this_ephsr & SMC_SQET ) smc->e_stats.sqet++;
  if (this_ephsr & SMC_LOST_CAR) smc->e_stats.lostcarr++;
  if (this_ephsr & SMC_LATCOL) smc->e_stats.latecoll++;
  if (this_ephsr & SMC_16COL) smc->e_stats.coll16++;
  SMC_WRITE_REG(smc->port,SMC_TCR,smc->control.TX_control);
 }

 if ( this_ephsr & SMC_CTR_ROL )
 {
  U16 counters = SMC_READ_REG(smc->port,SMC_ECR);
  smc->e_stats.snglcoll += counters & 0x000f;
  smc->e_stats.multcoll += ( counters & 0x00f0 ) >> 4;
  smc->e_stats.tx_defr += ( counters & 0x0f00 ) >> 8;
  smc->e_stats.exc_defr += ( counters & 0xf000 ) >> 12;
 }

 if ( ( this_ephsr & SMC_LINK_OK ) != (smc->control.last_EPHSR & SMC_LINK_OK) )
 {
  if ( this_ephsr & SMC_LINK_OK )
  {
   SMC_Reset_Chip( smc );
   smc->control.TX_control &= ~SMC_TX_EPHLOOP;
   smc->control.TX_control |= SMC_TX_FDUPLX;
   smc->status = SMC_Setup( smc );
  }
  else 
  {
   SMC_Set_Loopback(smc,NET_LOGICAL_LOOPBACK);
  }
  SMC_SET_BANK(smc->port,SMC_BANK(SMC_CTR));
  SMC_WRITE_REG(smc->port,SMC_CTR,smc->control.Mode & ~SMC_CTR_LEENA);
  SMC_WRITE_REG(smc->port,SMC_CTR,smc->control.Mode | SMC_CTR_LEENA);
 }

 smc->control.last_EPHSR = this_ephsr;
 SMC_WRITE_REG(smc->port,SMC_BSR,2);
} /* SMC_EPHHndlr */
