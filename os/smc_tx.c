/**********************************************************************/
/*File:           smc_tx.c                                            */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       SMC Transmit functions.                             */
/*                                                                    */
/*(C) Copyright Atari Games Corporation 1996                          */
/*                                                                    */
/*      This file implements the device level function of the network */
/*      interface. The device supported, obviously, is the SMC 919c9x */
/**********************************************************************/

#include <smc_regs.h>
#include <smc_data.h>
#include <net_smc.h>
#include <net_link.h>
#include <smc_link.h>
#include <smc_dma.h>
#include <os_proto.h>
#include <config.h>
#include <string.h>
#include <phx_proto.h>

#define NET_ETYPE_RUSH (0x8f55)

static U32 SMC_Upload(
                SMC_Data *smc,
                packet_number pkt_no,   /* If -1, then use urrent PNR pkt no. */
                U16 pkt_size,
                Net_Buffer *pkt
);

U32 SMC_Init_Header( packet_number pk_id, Net_Buffer *hdr , const netadd dest)
{
/***********************************************************************/
/*  This function places the header information into the passed buffer,*/
/*  which should be large enough for the full header.                  */
/*  The data is offset by 4 bytes - these 4 bytes contain the nd.      */
/***********************************************************************/
    int res;
    network_descriptor nd_id;

    nd_id = SMC_ND_FROM_PKT_NUM(pk_id);
    if (hdr->size >= (hdr->len + hdr->offset)) {
        U8 *p_buff;
	hdr->flags |= NB_TXPND | NB_INUSE;
        p_buff = hdr->data;
        memcpy(p_buff,dest,sizeof(netadd));
        p_buff+=sizeof(netadd);
        memcpy(p_buff,smc_data[SMC_IDX_FROM_ND(nd_id)].my_nid,sizeof(netadd));
        p_buff+=sizeof(netadd);
        if((smc_data[SMC_IDX_FROM_ND(nd_id)].descriptors[SMC_ND_IDX_FROM_ND(nd_id)].capabilities & NET_SEND_RAW)) {
            hdr->len = 2 * sizeof(netadd);
        }
        else {
        /* If interface was not opened for RAW SEND, then put some debugging */
        /* stuff in the front of the packet. Also, the ethernet type field!  */
            hdr->len = NET_PKT_HEADER_SIZE;
            *p_buff++ = (U8)NET_ETYPE_RUSH & 0xff;
            *p_buff++ = (U8)(NET_ETYPE_RUSH>>8) & 0xff;
            *p_buff++ = (U8)0;
            *p_buff++ = (U8)0;
        }
        res = 0;
    }
    else {
       res = NETERR_SEND_BAD_HEADER;
    } /* endif */
    return res;
} /* SMC_Init_Header */

static U32 no_buffs;
U32 SMCP_Alloc_Buff(network_descriptor nd_id,
                   U16 pkt_size,
                   packet_number *pkt_num,
                   U32 opts)
{
/*************************************************************/
/*   This function is a protected call of SMC_Alloc_Buff     */
/*                                                           */
/*   In order to ensure that SMC Alloc requests are properly */
/*   matched up on the pending queue, the Semaphore is       */
/*   blocked until the queue addition has been attempted.    */
/*************************************************************/
    int ii;
    U32 res,got_sem=0;
    SMC_Data *smc;
    smc = &smc_data[SMC_IDX_FROM_ND(nd_id)];
    /* Request buffer resource from SMC */
    /* See if the SMC is ready and willing */
    /* Protect us from interruptions, get the semaphore */
	do {
		EnterCritSec();						/*C*/
		if (smc->mode) {							/*C*/
			res = NETERR_SMC_BLOCKED;				/*C*/
#if DMA_STATS
			dma_stats.blockeds++;
#endif /* DMA_STATS */
		}
		else {								/*C*/
		    smc->mode ++;						/*C*/
#if DMA_STATS
		    dma_stats.blocks++;
#endif /* DMA_STATS */
		    res = NETERR_SEND_NORESOURCE;				/*C*/
		    got_sem = 1;						/*C*/

		    for ( ii = 0; ii < N_PKTS; ++ii )
		    {
			if ( pn_array[ ii ].qued == pn_array[ ii ].sent )
			{
#if INCLUDE_TS_ARRAY
 ts_array[ ts_aix %= N_TS ].qs = 'q';
 ts_array[ ts_aix ].isr = 0;
 ts_array[ ts_aix++ ].t = (float)prc_get_count()/75000000.0;
#endif /* INCLUDE_TS_ARRAY */
			 pn_array[ ii ].fram = eer_rtc;
			 ++pn_array[ ii ].qued;
			 *pkt_num = SMC_PKT_DRV(pn_array[ ii ].pktn) |
				    (nd_id & 0xf00003ff) |
				    (0x0fff0000 & (smc->p_stats.txin <<17));
			 res = NETOK_SEND_COMPLETE;
			 break;
			}
		    }
if ( ii == N_PKTS )
	++no_buffs;
		    smc->mode --;						/*C*/
#if DMA_STATS
		    dma_stats.unblocks++;
#endif /* DMA_STATS */
	        }								/*C*/
		ExitCritSec();							/*C*/
		if ( (!(res & NETOK_MASK)) && opts == NETOPT_BLOCKING ) 
			Net_Relinquish(SMC_ND_FROM_PKT_NUM(pkt_num));
		else
			break;
	} while (!(res & NETOK_MASK));
    return res;
} /* SMCP_ALLOC_Buff */

U32 SMC_Alloc_Buff(network_descriptor nd_id,
                   U16 pkt_size,
                   packet_number *pkt_num,
                   U32 opts)
{
/***********************************************************************/
/*    This function will attempt to obtain a buffer from on-chip       */
/*    memory, detirmine the pkt number, and stuff it into the          */
/*    provided space.                                                  */
/*    Depending on Opts, may return after enabling the interrupts,     */
/*    and the packet stuff gets done later.                            */
/*    NOTE:                                                            */
/*    This function assumes the chip is in the correct bank.           */
/***********************************************************************/
    U32 res;
    U16 pkt_blks;       /* Number of 256 byte blocks needed. */
    U16 busy_loop;      /* Small (!) value used to idle a little while */
    U16 alloc_done;     /* TRUE when an alloc completes.  */
    SMC_Data *smc;

    smc = &smc_data[SMC_IDX_FROM_ND(nd_id)];
    busy_loop = SMC_BUSY_LOOP;
    while ((SMC_READ_REG(smc->port,SMC_MMUCR) & SMC_MMU_BUSY) && busy_loop) {
       /* A release is in progress, so hang about */
       if (opts != NETOPT_SEND_IMMEDIATE) {
           /* Relinquish, if we are allowed to... */
           Net_Relinquish(nd_id);
        }
        busy_loop--;
        smc->control.tbusys++;
    } /* endwhile */

    if (busy_loop) {  /* If MMU_busy went away, carry on, otherwise bail out */
        /* issue the allocate command */
        pkt_blks = ( pkt_size / 256 ) ;   /* requesting 0 allocates 1 block!!! */
        SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_ALLOC | pkt_blks);

        /* delay a few ticks, depending on options IMMEDIATE uses busy wait, */
        /* NON_BLOCK & BLOCKING will allow re-scheduling.                    */
        busy_loop = SMC_BUSY_LOOP;
        alloc_done = SMC_READ_REG(smc->port,SMC_ISR) & SMC_ALLOC_INT;
        while ( busy_loop && ! alloc_done ) {
#if 0
            if (opts != NETOPT_SEND_IMMEDIATE) {
               /* Relinquish, if we are allowed to... */
               Net_Relinquish(nd_id_);
               /* so, it is some time later... */
            } /* endif */
#endif
            busy_loop--;
            smc->control.tbusys++;
            alloc_done = SMC_READ_REG(smc->port,SMC_ISR) & SMC_ALLOC_INT;
        } /* enddo */

        /* Did the allocate succeed ? */
        if (alloc_done) {
        /* Yes, detrimine the packet no, return it to caller. */
            *pkt_num = SMC_PKT_DRV((SMC_READ_REG(smc->port,SMC_PNR) & SMC_ARRPNRMSK)>>8);
            *pkt_num |= (nd_id & 0xf00003ff) | (0x0fff0000 & (smc->p_stats.txin <<17));
            res = NETOK_SEND_COMPLETE;
        }
        else {
             res = NETOK_SEND_PENDING;
        }
    }  /* busy_loop */
    else {
        res = NETERR_MMU_NORESOURCE;
    } /* endif */

    return res;
} /* SMC_Alloc_Buff */

U32 SMCP_TX_Enqueue(packet_number pkt_num,
                       U16 size,
                       Net_Buffer *pkt,
                       U32 opts)
{
        int nres=NETOK;
        SMC_Data *smc;
        U16 old_bank;

        smc = &smc_data[SMC_IDX_FROM_ND(pkt_num)];
        /* Protect us from interruptions, get the semaphore */
	do {
		EnterCritSec();							/*C*/
		if (smc->mode) {						/*C*/
			nres = NETERR_SMC_BLOCKED;				/*C*/
#if DMA_STATS
			dma_stats.blockeds++;
#endif /* DMA_STATS */
		}
		else {								/*C*/
			smc->mode ++;						/*C*/
#if DMA_STATS
			dma_stats.blocks++;
#endif /* DMA_STATS */
			old_bank = SMC_READ_BANK(smc->port);			/*C*/
			if (old_bank != SMC_BANK(SMC_MMUCR)) {			/*C*/
				SMC_SET_BANK(smc->port,smc->control.curr_bank = SMC_BANK(SMC_MMUCR));
			} /* endif */						/*C*/
			/* Copy the buffer into the chip memory */		/*C*/
			nres = SMC_TX_Enqueue(smc,pkt_num,size,pkt);		/*C*/
			if (old_bank != SMC_BANK(SMC_MMUCR)) {			/*C*/
				SMC_SET_BANK(smc->port,smc->control.curr_bank = old_bank);
			} /* endif */						/*C*/
			smc->mode --;/* We're done with the chip, let 'r go */  /*C*/
#if DMA_STATS
			dma_stats.unblocks++;
#endif /* DMA_STATS */
		}								/*C*/
		ExitCritSec();							/*C*/
		if ( !(nres & NETOK_MASK) && opts == NETOPT_BLOCKING ) 
			Net_Relinquish(SMC_ND_FROM_PKT_NUM(pkt_num));
		else
			break;
	} while (!(nres & NETOK_MASK));
        return nres;
} /* SMCP_TX_Enqueue */

U32 SMC_TX_Enqueue( SMC_Data *smc,
                       packet_number pkt_num,
                       U16 size,
                       Net_Buffer *pkt)
{
    int res;
#if 0
	char cbuff[80];
#endif
    if (&smc_data[SMC_IDX_FROM_ND(pkt_num)] != smc ) {
#if 0
        net_putstr("Bad smc address or smc packet number in TX_Enqueue call!\n");
#endif
        smc->control.drv_tx_errs++;
        return NETERR_SEND_BADPARAMETER;
    }
    res = SMC_Upload(smc,pkt_num,size,pkt);

    if ( 0 == res ) {
        smc->p_stats.txqd++;
#if !defined(SMC_USE_ETEN) || !SMC_USE_ETEN
        /* If the Early Transmit feature of the chip is used, this has been done in the upload */
	/* Now place the packet onto the TX FIFO */
	SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_TXPUSH);
#endif /* SMC_USE_ETEN */
#if SMC_TX_AUTORLS
        SMC_Set_Int_Mask(smc, SMC_TX_INT | SMC_TX_EMPTY_INT, 1);
#else
        SMC_Set_Int_Mask(smc, SMC_TX_INT , 1);
#endif  /* SMC_TX_AUTORLS */
	pkt->flags |= NB_TXDONE;
	pkt->flags &= ~NB_TXPND;
    } /* endif */
    return NETOK_SEND_COMPLETE;
} /* SMC_TX_Enqueue */

static U32 SMC_Upload(
                SMC_Data *smc,
                packet_number pkt_no,   /* If -1, then use urrent PNR pkt no. */
                U16 pkt_size,
                Net_Buffer *pkt
)
/*****************************************************************************/
/* A function that actually does some work...                                */
/* In this case, move stuff from tNet_Buffer into on-chip memory             */
/*****************************************************************************/
{
    U16 ptr_mask;       /* Pre-set a mask to 'or' with the data pointer    */

    /* We need to set up the PNR before getting the data */
    if ( pkt_no != -1 ) {
        SMC_WRITE_REG(smc->port,SMC_PNR,SMC_DRV_PKT(pkt_no));
    }
#if defined(SMC_USE_ETEN) && SMC_USE_ETEN
#warning Using Early Transmit feature
    ptr_mask = SMC_PTR_AUTO_INC | SMC_PTR_ETEN    ;
#else
    ptr_mask = SMC_PTR_AUTO_INC ;
#endif /* SMC_USE_ETEN */

    /* Set up the SMC's data pointer so we can access the actual packet  */
    /* on the chip. ( Add 2, as there are 4 bytes of internal stuff )    */
    SMC_WRITE_REG(smc->port,SMC_PTR,ptr_mask | 2);

    /* Write the size - mask upper 5 bits, and low bit        */
    /* Add 6, as the SMC Internal control overhead has to be  */
    /* counted, too.                                          */

    ptr_mask = SMC_READ_REG(smc->port,SMC_PTR);  /* Insert a delay */

    SMC_WRITE_REG(smc->port,SMC_DATA,(pkt_size+6)  & 0x07fe);

    /* Conveniently, the SMC will auto-increment its data pointer,      */
    /* all that's left to do is to write the data register.             */
    SMC_Copy_Net_Buffer(pkt,pkt_size,0,smc->port);

    return 0;

} /* SMC_Upload */

void SMC_Copy_Net_Buffer( Net_Buffer *pkt, U16 pkt_size,U32 mode, U32 dest)
{
	U16 b_in_p;		/* Byte_IN_Packet index. This var tracks the pos.  */
				/* in the packet, in lumps of NET_BUFFER_SIZE.     */
	S16 j=0;		/* counter for moving the bytes/words of chip      */
				/* within a buffer lump.                           */
	S16 block;		/* Number of bytes to go into a given buffer - can */
				/* go negative on boundary conditions.             */
	U8   *dp;		/* Pointer into the current buffer being filled.   */
	U16  data=0;
	Net_Buffer *buff;	/* Local buffer pointer which tracks the passed one*/
	U16  *pdest;
    
	buff = pkt;
	pdest = (U16 *) dest;

	/* But still need a little house-keeping to handle odd starts and   */
	/* and ends.                                                        */
        b_in_p = 0;          /* byte in packet starts at zero (duh) */

        /* Now, move data from buffer into SMC */
        /* We always start on a word boundary - so we only have to  */
        /* contend with odd length buffers.                         */
        do {
            dp = & buff->data[buff->offset];       /* pointer into src buff */
            block = buff->len;           /* this much to do in this buff    */
            if ( b_in_p&1 ) {            /* Odd start, do single byte write */
               block --;
               data |= (*dp++)<<8;       /* Use last byte of previous buffer */
		if(mode) 
			{*pdest++ = data; pdest++; }
		else 
		       SMC_WRITE_DATA(dest,SMC_DATA,data);
            }
            for (j=1; j < block ;j+=2 ) {
	        data = (*dp++)&0xff;
                data |= (*dp++)<<8;
		if(mode)
			{*pdest++ = data; pdest++;}
		else
			SMC_WRITE_DATA(dest,SMC_DATA,data);
            } /* endfor j */

            if (block & 1) {               /* is there a trailing byte? */
	        data = (*dp++)&0xff;          /* Set it up in data, for use */
                                              /* next iteration, or at end. */
            }
            b_in_p += buff->len;         /* this many bytes added on chip */
            buff = buff->next;
        } while ( (b_in_p <= pkt_size) && buff ); /* enddo */

        /* fill in the control byte. If it was an odd size packet,   */
        /* then need to write one more byte, otherwise load it into  */
        /* the upper half of the next word.                          */
        if (pkt_size &1) {
		if(mode) 
			{*pdest++ = data | (0x20 << 8); pdest++; }
		else 
			SMC_WRITE_REG(dest,SMC_DATA,data | (0x20 << 8 ));
        }
        else {
		if(mode) 
			{*pdest++ = 0 ; pdest++; }
		else 
			SMC_WRITE_REG(dest,SMC_DATA,0);
        }
} /* SMC_Copy_Net_Buffer */

void SMC_Initiate_Next_Allocate(network_descriptor nd_id)
	/* If there is another TX ready, do an allocate for it */
{
	SMC_DMA_Req *tx_req = 0, *test_req;
	packet_number pkt_no;
	U32 res;

	if( smc_data[SMC_IDX_FROM_ND(nd_id)].control.Int_Mask & SMC_ALLOC_INT ) 
	{
		return; /* An allocate is still pending. Do not intiate another one. */
	}
	else if (smc_dma_tx_queue) 
	{  /* There is another TX request queued, so start an allocate for it */
		tx_req = smc_dma_tx_queue->next;	/* this is the head... */
		res = SMCP_Alloc_Buff(tx_req->pkt_num,tx_req->pkt_size,&pkt_no, NETOPT_SEND_IMMEDIATE);
		if(res == NETOK_SEND_COMPLETE) { /* The allocate happened */
			tx_req->pkt_num = pkt_no;	/* The actual allocated pkt no. */
			tx_req->req_type = SMC_DMA_TX;  /* It's ready for big time. */
			if(tx_req == (test_req =DMA_Dequeue(&smc_dma_tx_queue))) { /* Yup, we're in sync */
				DMA_Enqueue(&smc_dma_queue,tx_req);
				smc_data[SMC_IDX_FROM_ND(nd_id)].p_stats.txpnd--;	
			}
			else {  /* oh, dear - a catastrophe. the head of the queue is not what it should be */
				/* Free the pkt we just got, then free the requests. */
				SMC_Chip_Pkt_Rls(pkt_no);
				DMA_Enqueue(&smc_dma_free,tx_req);
				DMA_Enqueue(&smc_dma_free,test_req);
			}
		}
		else 
		{ /* No resource, or chip blocked, or pending. Need to try again, later */
		}
	} /* end if  */
} /* SMC_Initiate_Next_Allocate */

void SMC_Chip_Pkt_Rls(packet_number pno)
/**********************************************************************
	Simple little thing that releases the packet passed
	on the SMC chip.
	But it does take note of the condition of the MMU,
	and will busy wait if needed.
**********************************************************************/
{
        SMC_Data *smc= &smc_data[SMC_IDX_FROM_ND(pno)];
	U32 busy_loop = SMC_BUSY_LOOP;

	while ((SMC_READ_REG(smc->port,SMC_MMUCR) & SMC_MMU_BUSY) && busy_loop) {
		/* A release is in progress, so hang about */
		busy_loop--;
		smc->control.tbusys++;
	} /* endwhile */
	if(busy_loop) {
		/* Set up the Packet Number Register so SMC knows which packet we mean */
		SMC_WRITE_REG(smc->port,SMC_PNR,SMC_DRV_PKT(pno));
		/* Now release the packet. */
		SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_PKTRLS);
	}
} /* SMC_Chip_Pkt_Rls */
