#ifndef smc_link_defs
#define smc_link_defs
/**********************************************************************/
/*File:           smc_link.h                                          */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       Function Prototypes for the SMC driver	      */
/*                                                                    */
/*(C) Copyright Time Warner Interactive 1995                          */
/*                                                                    */
/**********************************************************************/

#include <smc_data.h>

/* smc_main.c */
U32 SMC_Initialize(Net_Desc *nd_mem);
Net_Desc *SMC_Open( char *device );
U32 SMC_Close(network_descriptor old_nd);
void SMC_Reset();
U32 SMC_Is_Open(network_descriptor query_nd);
void SMC_Add_Sigs(Net_Desc *nd);
void SMC_Remove_Sigs( SMC_Data *smc, Net_Desc *nd);
void SMC_Interrupt_Vector0(int vector);
U32 SMC_Set_Int_Mask(SMC_Data *smc, int int_flag, int int_state);
U32 SMC_Set_MC_Mask(SMC_Data *smc, int channel, int state);
U32 SMC_Setup(SMC_Data *smc);
int SMC_Reset_Chip(SMC_Data *smc);
U32 SMC_Set_Loopback(SMC_Data *smc, U32 loop_mode);
U32 SMC_Init_Network_ID(SMC_Data *smc);


/* smc_dbug.c */
void SMC_Debug_C_stats(SMC_Data *smc,SMC_Data *diff);
void SMC_Debug_P_stats(SMC_Data *smc,SMC_Data *diff);
void SMC_Debug_E_stats(SMC_Data *smc,SMC_Data *diff);
void SMC_Debug_S_stats(SMC_Data *smc,SMC_Data *diff);
void SMC_Debug_ND_stats(Net_Desc *nd,Net_Desc *diff);

/* smc_rx.c */
void SMC_RXHndlr(SMC_Data *smc);
void SMC_RXError(SMC_Data *smc);
U32  SMC_Load_Net_Buffer( Net_Buffer* buff, U16 start, U16 end, U32 mode, U32 source );

/* smc_tx.c */
U32 SMC_Init_Header( packet_number pk_id, Net_Buffer *hdr , const netadd dest);
U32 SMC_Alloc_Buff(network_descriptor nd_id, U16 pkt_size, packet_number *pkt_num, U32 opts);
U32 SMCP_Alloc_Buff(network_descriptor nd_id,U16 pkt_size,packet_number *pkt_num, U32 opts);
U32 SMCP_TX_Enqueue(packet_number pkt_num,U16 size,Net_Buffer *pkt,U32 opts);
U32 SMC_TX_Enqueue( SMC_Data *smc,packet_number pkt_num,U16 size,Net_Buffer *pkt);
void SMC_Copy_Net_Buffer( Net_Buffer *pkt, U16 pkt_size,U32 mode, U32 dest);
void SMC_Chip_Pkt_Rls(packet_number pno);
void SMC_Initiate_Next_Allocate(network_descriptor);

/* smc_intv.c */
void SMC_Interrupt_Vector0(int vector);
void SMC_Interrupt_Handler(void);

/* smc_diag.c */
U32 SMC_Verify_Bank(SMC_Data *smc, int bankNo,int verbose);

/* smc_dma.c */
void SMC_DMA_Initialize(U8 *dma_pool, U32 pool_size, void (*handler)(),void *func);
void Galileo_Interrupt_Vector();
void Galileo_Interrupt_Handler();
U32 SMC_Start_DMA(network_descriptor nd_id);

#endif
