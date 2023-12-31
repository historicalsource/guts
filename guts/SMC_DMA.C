/**********************************************************************/
/*File:           smc_dma.c                                          */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Atari Games Corporation.          */
/*Contents:       SMC Device functions.                               */
/*                                                                    */
/*(C) Copyright Atari Games Corporation 1996                          */
/*                                                                    */
/*      This file implements the DMA routines and interrupt handling. */
/*                                                                    */
/**********************************************************************/

#include <smc_regs.h>
#include <smc_data.h>
#include <net_smc.h>
#include <smc_dma.h>
#include <net_link.h>
#include <smc_link.h>
#include <string.h>
#include <os_proto.h>
#include <intvecs.h>

SMC_DMA_Req  *smc_dma_free;		/* pointer to a queue of free request blks  */
SMC_DMA_Req  *smc_dma_queue;		/* pointer to queue of pending requests     */
SMC_DMA_Req  *smc_dma_tx_queue;		/* pointer to current outstanding ALLOC request queue */
SMC_DMA_Req  *smc_dma_req;		/* pointer to current outstanding request (NOT on any queue)*/
	/* To get the first element, use smc_dma_queue->next  !	    */

DMA_stats dma_stats;

struct act_q galileo_dma_act_q_elt;
extern struct act_q smc_act_q_elt;

static U32 Galileo_DMA_Verify(SMC_DMA_Req *this_req);
static U32 SMC_RX_DMA_Done(SMC_DMA_Req *completed_rx_req);
static void SMC_TX_DMA_Done(SMC_DMA_Req *completed_tx_req);
static void Galileo_DMA_Invocation( Galileo_DMA *dma_req);
U32 Galileo_Initiate_Next_SMC_DMA(SMC_DMA_Req *this);

void SMC_DMA_Initialize(U8 *dma_pool, U32 pool_size,
                 void (*handler)(),void *func)
/******************************************************************************
	This function sets up the buffers and vectors for managing DMA
	transfers and interrupts.
******************************************************************************/
{
	U8 *step=dma_pool;
	U32 res;

	smc_dma_queue = 0;
	smc_dma_tx_queue = 0;
	smc_dma_free = 0;
#if DMA_STATS
	memset(&dma_stats,0,sizeof(dma_stats));
#endif /* DMA_STATS */

	while (step<=(dma_pool+pool_size-sizeof(SMC_DMA_Req))) {
		((SMC_DMA_Req *)step)->used = 0;
		((SMC_DMA_Req *)step)->head = 0;
		if(NETOK != (res = DMA_Enqueue(&smc_dma_free,((SMC_DMA_Req *)step))))
			break;
		step += sizeof(SMC_DMA_Req);
	}

	/* The prc_q_action element  could be bent to NU_Activate_HISR with the param set to the Handler HISR */
	galileo_dma_act_q_elt.que = 0;
	galileo_dma_act_q_elt.next = 0;
	if(handler) {
		galileo_dma_act_q_elt.action = handler;
		galileo_dma_act_q_elt.param = func;
	}
	else {  /* Defualt handler is to call the Handler direct */
		galileo_dma_act_q_elt.action = Galileo_Interrupt_Handler;
		galileo_dma_act_q_elt.param = 0;
	}
	prc_set_vec(DM0_INTVEC, Galileo_Interrupt_Vector);

} /* SMC_DMA_Initialize */

void Galileo_Interrupt_Vector()
{
/******************************************************************************
	This is the lowest level vector, which gets called soon after
	the Galileo chip interrupts the processor.
	Basically, turn of the interrupt source, if it is the
	Galileo, and cause a action routine to run.
******************************************************************************/
	VU32 *cause = (VU32 *)GALILEO_INT_CAUSE;
	void* res;
	if(!(*cause&DM0_NOTES) ) return;
	/* It is a DMA Channel 0 interrupt, so deal with it */

	/* First, turn it off */
	*cause = ~DM0_NOTES;
	
	/* now, what do we do with it? */
	/* Queue an action routine. This is done to allow driver or app to bend 
	   vector and force correct context for driver/app
	*/
	res = prc_q_action(&galileo_dma_act_q_elt) ;
	dma_stats.ints++;

} /* Galileo_Interrupt_Vector */

void Galileo_Interrupt_Handler()
{
/******************************************************************************
	For now, the only interrupt comes from Galileo DMA Channel 0.
	And the only reason for an interrupt is that the DMA has finished.
	So verify that it has.
******************************************************************************/
	U32 res = 0;
        if( (*(Galileo_DMA_Control *)GALILEO_DMA0_CTL).DMAActSt) {
		/* The channel seems to be busy.. what to do ? */
		dma_stats.busy++;
		return;
	}

	dma_stats.actr++;
	if (smc_dma_req) {	/* There is a corresponding request */
		SMC_DMA_Req 	*this = smc_dma_req;
		dma_stats.reqs++;
		/* Verify that the request matches the DMA registers */
		if ( 1 | (Galileo_DMA_Verify(this)))
		{
    			U16 old_bank ;

			dma_stats.vfys++;
			old_bank=SMC_READ_BANK(smc_data[0].port);            
			if (old_bank != SMC_BANK(SMC_PNR))
			{ 
				/* Are we in correct bank? */              
				SMC_SET_BANK(smc_data[0].port,SMC_BANK(SMC_PNR));
#if DMA_STATS
				dma_stats.bad_banks++;
#endif /* DMA_STATS */
				
			} /* endif */                                           

			/* What type was just done? */
			if (this->req_type == SMC_DMA_RX) {
				/* Handle completing the Rx transaction */
				res = SMC_RX_DMA_Done(this);
			}
			else if (this->req_type == SMC_DMA_TX) {
				/* A TX completed moving onto the chip */
				SMC_TX_DMA_Done(this);
			}
			/* We've done with the current request, so free it */
			if (res)
			{	/* Request failed, do again */
#if DMA_STATS
				dma_stats.re_issues++;
#endif /* DMA_STATS */
			}
#if 0   /* As a test, DO NOT re-issue failed DMA moves... */
			else
#endif
			{  /* all ok, free up request */
				DMA_Enqueue(&smc_dma_free,this);
				this = DMA_Dequeue(&smc_dma_queue);
			}

			if (Galileo_Initiate_Next_SMC_DMA(this)) 
			{ /* Next request has been dispatched */
			  /* or the SMC was not available...  */
				dma_stats.nxtr++;
			}
			else 
			{ 
				EnterCritSec();						/*C*/
				smc_dma_req = 0;	/* No more requests */
				smc_data[0].mode = 0;	/* So allow access to the SMC */
#if 0 /* FOM */
    				if (smc_data[0].control.int_sem)
				{
					dma_stats.smcq++;
				    prc_q_action(&smc_act_q_elt) ;
				}
#endif /* FOM */
#if DMA_STATS
				dma_stats.unblocks++;
#endif /* DMA_STATS */
				ExitCritSec();						/*C*/
			}
		}
		else  /* DMA state is not matched by the current request...*/
		{     /* So re-issue the Galileo DMA request. */
#if DMA_STATS
			smc_dma_req->invoked++;
			dma_stats.re_issues++;
#endif
			Galileo_DMA_Invocation((Galileo_DMA *) &smc_dma_req->dma_req);
		}
	} 
} /* Galileo_Interrupt_Handler */

U32 Galileo_DMA_Verify(SMC_DMA_Req *this_req)
{
/******************************************************************************
Sanity check that verifies that the just completed DMA matches the one that was requested.
******************************************************************************/
	Galileo_DMA *dma_req = &this_req->dma_req;
	U32 r1,r2;
	VU32 res;
	if (( this_req-> req_type == SMC_DMA_TX) && 
	    ( (*(VU32*)GALILEO_DMA0_CTL) == dma_req->Control.dma_word))
	{ /* We both agree we're doing a Tx type ... */
	  /* Now see if the addresses match */
		r1 = PHYS((U32) &this_req->dma_buffer + (this_req->pkt_size*2));
		r2 = *(VU32 *)GALILEO_DMA0_SRC + *(VU32 *)GALILEO_DMA0_BC;
		res = r1 == r2 ;
	}
	else if (( this_req-> req_type == SMC_DMA_RX) && 
	    ( (*(VU32 *)GALILEO_DMA0_CTL) == dma_req->Control.dma_word))
	{ /* We both agree we're doing a Rx type ... */
		r1 = PHYS((U32) &this_req->dma_buffer + (this_req->pkt_size*2));
		r2 = *(VU32 *)GALILEO_DMA0_DST + *(VU32 *)GALILEO_DMA0_BC;
		res = r1 == r2 ;
	}
	else res =  0; /* Directions didn't match... */
	return res;
}

U32 Galileo_Initiate_Next_SMC_DMA(SMC_DMA_Req *this)
{
/******************************************************************************
	This function will locate the next DMA request, if there is one.
	The this is set to this request, and the request is
	removed from the queue (obviously!).
	The Galileo DMA Request record and the SMC chip are set up. 
	The Galileo is then invoked with the prepared request record.
******************************************************************************/
	Galileo_DMA *this_req;
	U32 res = NETOK;
	SMC_Data *smc;

	if (this) 
	{
		smc = &smc_data[SMC_IDX_FROM_ND(this->pkt_num)];
		this_req = &this->dma_req;
		/*
		Prepare SMC to handle movement, and
		initite next transfer. 
		*/

#if 0	/* For now, this protection NEEDS to be done outside this function */
		EnterCritSec();							/*C*/
		if ( smc->mode | smc->control.int_sem )					/*C*/
		{   /* SMC either blocked, or in interrupt service code */	/*C*/
			res = NETERR_SMC_BLOCKED;				/*C*/
		}								/*C*/
		else {								/*C*/
			smc->mode ++;						/*C*/
			old_bank = SMC_READ_BANK(smc->port);			/*C*/
			if (old_bank != SMC_BANK(SMC_MMUCR)) {			/*C*/
				SMC_SET_BANK(smc->port,smc->control.curr_bank = SMC_BANK(SMC_MMUCR));
			} /* endif */						/*C*/
			res = NETOK;						/*C*/
		} /* endif */							/*C*/
		ExitCritSec();							/*C*/
#endif
		if ( res == NETOK )
		{
			
			this_req->ByteCt = 2 * this->pkt_size; 
				 /* By two, as we move 4 bytes per xfer, but only 2 are data... */
			this_req->NextRecPtr = 0;
			this_req->Control.dma_word = 0;              /* Set all the bits  to 0. */
			this_req->Control.dma_bits.IntMode = DMA_INT_BY_BLOCK;       /* an interrupt when done, please */
			this_req->Control.dma_bits.ChainMod = DMA_NOCHAIN;
			this_req->Control.dma_bits.TransMod = DMA_BY_REQ;
			this_req->Control.dma_bits.DatTransLim = DMA_4BYTES;		/* Need to see what effect this has ... */
			if (this->req_type == SMC_DMA_TX) {
				U16 ptr_mask = 0;
				this_req->SrcAdd = PHYS(this->dma_buffer);
				this_req->Control.dma_bits.SrcDir = DMA_INC;
				this_req->DestAdd = PHYS(smc->port + (SMC_REG(SMC_DATA)<< 1));
				this_req->Control.dma_bits.DestDir = DMA_HOLD;

				/* Now set up the SMC to upload the data */
				/* Set up the Packet Number Register so it knows which packet to use */
				SMC_WRITE_REG(smc->port, SMC_PNR,SMC_DRV_PKT(this->pkt_num));

#				if defined(SMC_USE_ETEN) && SMC_USE_ETEN
#					warning Using Early Transmit feature
					ptr_mask = SMC_PTR_AUTO_INC | SMC_PTR_ETEN    ;
#				else
					ptr_mask = SMC_PTR_AUTO_INC ;
#				endif

				/* The pointer register sould start at 2 for TX buffers, 
				   as the first 2 bytes are for SMC use.			*/
				SMC_WRITE_REG(smc->port, SMC_PTR, ptr_mask | 2);
				/* first, write the size - mask upper 5 bits, and low bit */
				SMC_WRITE_REG(smc->port, SMC_DATA,(this->pkt_size +6) & 0x07fe);
			}
			else if (this->req_type == SMC_DMA_RX) {
				U16 ptr_mask = 0;
				this_req->DestAdd = PHYS(this->dma_buffer);
				this_req->Control.dma_bits.DestDir = DMA_INC;
				this_req->SrcAdd = PHYS(smc->port + (SMC_REG(SMC_DATA)<< 1));
				this_req->Control.dma_bits.SrcDir = DMA_HOLD;

				/* Now set up the SMC to unload the data */
				/* Set up the Packet Number Register so it knows which packet to use */
				/* NOTE that we're specifying the packet number, rather than relying
				   on the RX FIFO packet number. Because things are now so asynchrounous,
				   it is possible for the top of the RX FIFO to NOT be the packet we are
				   trying to DMA.
				*/
				SMC_WRITE_REG(smc->port, SMC_PNR,SMC_DRV_PKT(this->pkt_num));

				ptr_mask = SMC_PTR_AUTO_INC | SMC_PTR_READ ;

				/* The pointer register is set up with the desired offset.
				   Add 4, as the SMC has 4 bytes of overhead at the start. */
				SMC_WRITE_REG(smc->port, SMC_PTR,ptr_mask | (4 + this->pkt_offset));
			}
			else /* Bogus request, kill it, see about a next one */
			{ /* loop around again, until valid request shows up, or queue dries up */
				DMA_Enqueue(&smc_dma_free,this);
				this = 0;
				this_req = 0; 
#if DMA_STATS
				dma_stats.bad_req++;
#endif
			}


			/* Now is the time to actually program the Galileo to do its thing */
			Galileo_DMA_Invocation(this_req);
			/* Now that the request has been Invoked, it properly becomes the 'current' one */
			EnterCritSec();						/*C*/
			smc_dma_req = this ; 					/*C*/
			ExitCritSec();						/*C*/
		} /* endif res == NETOK */

	} /* end if */
	return this != 0;  /* smc_dma_req now points to the currently executing DMA xfer */
	
} /* Galileo_Initiate_Next_SMC_DMA */

void Galileo_DMA_Invocation( Galileo_DMA *dma_req)
{
/******************************************************************************
	This function will set up the Galileo to execute the DMA
	request pointed to. ChainMod in the Control.dma_bits field
	is checked : if mode is 0, the request pointer is
	passed to Galileo, otherwise the respective registers
	in Galileo are programmed with the request, and fired
	up, as a one shot.
	ONLY CHANNEL 0 IS USED.
	If chained mode is requested, then make *sure* the request
	has been flushed out of the cache!
******************************************************************************/
	if ( dma_req )
	{
		*(VU32 *)GALILEO_DMA0_CTL = 0;          /* disable the channel */

		if (dma_req->Control.dma_bits.ChainMod) {       /* it is a one shot. place req. into Galileo */
#if 0
			smc_putstr("DMA Chain one shot\n");
			nsprintf(buff,sizeof(buff),"ByteCT: %x <= %d, SrcAdd: %x <= %x, DestAdd: %x <= %x\n",
				GALILEO_DMA0_BC,dma_req->ByteCt,
				GALILEO_DMA0_SRC,dma_req->SrcAdd,
				GALILEO_DMA0_DST,dma_req->DestAdd);
			smc_putstr(buff);
#endif
			*(VU32 *)GALILEO_DMA0_BC = dma_req->ByteCt;
			*(VU32 *)GALILEO_DMA0_SRC = dma_req->SrcAdd;
			*(VU32 *)GALILEO_DMA0_DST = dma_req->DestAdd;
			*(VU32 *)GALILEO_DMA0_NXTRCD = 0;
			dma_req->Control.dma_bits.ChanEn = DMA_CHAN_ON; /* Make sure the channel get turned on */
		}
		else {  /* Chained mode, only need to tell Galileo where the first record is. */
#if 0
			smc_putstr("DMA Chain Chained\n");
#endif
			*(VU32 *)GALILEO_DMA0_NXTRCD = PHYS((U32)dma_req);
			dma_req->Control.dma_bits.ChanEn = DMA_CHAN_ON; /* Make sure the channel gets turned on */
			dma_req->Control.dma_bits.FetNextRec = 1;       /* and that the first record gets loaded. */
		}

#if 0
		nsprintf(buff,sizeof(buff),"Control.dma_bits: %x\n",
				dma_req->Control.dma_bits);
		smc_putstr(buff);
#endif
		*(VU32 *)GALILEO_DMA0_CTL = dma_req->Control.dma_word;
	} /* end if */
} /* Galileo_DMA_Invocation */

SMC_DMA_Req *DMA_Dequeue(SMC_DMA_Req **dma_queue) 
{
/******************************************************************************
	This function will dequeue (remove) the first element from the passed
	queue, if availbale, and return it; if none, returns 0
******************************************************************************/
	if(*dma_queue) {	/* There is at least one */
		SMC_DMA_Req *use_this_one = 0;
		EnterCritSec();							/*C*/
		if(*dma_queue)   /* there still is one */			/*C*/
		{								/*C*/
			use_this_one = (*dma_queue)->next;			/*C*/
			if(use_this_one)
			{ 	/* This should always happen, but once it didn't */
				if (*dma_queue == (*dma_queue)->next)			/*C*/
					*dma_queue = 0;	/* last one is used up */	/*C*/
				else							/*C*/
					(*dma_queue)->next = (*dma_queue)->next->next;	/*C*/
			}
#if DMA_STATS
			if(dma_queue == &smc_dma_free)
			{
				dma_stats.free_size--;
				dma_stats.got++;
			}
			else if (dma_queue == &smc_dma_queue)
			{
				dma_stats.q_size--;
				dma_stats.de_queued++;
			}
			else if (dma_queue == &smc_dma_tx_queue)
			{
				dma_stats.tx_q_size--;
				dma_stats.tx_de_queued++;
			}
#endif
		}								/*C*/
		ExitCritSec();							/*C*/
		if (use_this_one)
		{
			use_this_one->next = 0;
			use_this_one->invoked = 0;
			use_this_one->used++;
			use_this_one->head = 0;
		}
		return use_this_one;
	}
	else return 0;
} /* DMA_Dequeue */

U32 DMA_Enqueue(SMC_DMA_Req **dma_queue, SMC_DMA_Req *q_this_one)
{
/******************************************************************************
	This function will place the passed req. buffer at the  end of the 
	queue passed. If the queue is empty, will initiliaze it.
******************************************************************************/
	if(q_this_one && !q_this_one->head) {	/* there is a buffer to be appended */
		EnterCritSec();							/*C*/
		if(*dma_queue) { /* the queue has some entries */		/*C*/
			q_this_one->next = (*dma_queue)->next;			/*C*/
			(*dma_queue)->next = q_this_one;			/*C*/
		}								/*C*/
		else {								/*C*/
			q_this_one->next = q_this_one;				/*C*/
		}								/*C*/
		(*dma_queue) = q_this_one;					/*C*/
		q_this_one->head = dma_queue;	/*this shows which queue it's on C*/
#if DMA_STATS
		if(dma_queue == &smc_dma_free)
		{
			if ( ++dma_stats.free_size > dma_stats.max_free)
				dma_stats.max_free = dma_stats.free_size;;
			dma_stats.freed++;
		}
		else if (dma_queue == &smc_dma_queue)
		{
			if ( ++dma_stats.q_size > dma_stats.max_q)
				dma_stats.max_q = dma_stats.q_size;
			dma_stats.queued++;
		}
		else if (dma_queue == &smc_dma_tx_queue)
		{
			if ( ++dma_stats.tx_q_size > dma_stats.max_tx_q)
				dma_stats.max_tx_q = dma_stats.tx_q_size;
			dma_stats.tx_queued++;
		}
#endif
		ExitCritSec();							/*C*/
		return NETOK;
	}
	else return NETERR_BADPARAMETER;
} /* DMA_Enqueue */

U32 SMC_RX_DMA_Done(SMC_DMA_Req *completed_rx_req)
{
/******************************************************************************
	This routine is invoked when a DMA completes copying data out
	of packet memory.
	First, the packet memory is released.
	Then the DMA Buffer is copied and compressed into the Net_Buffers
	obtained from the application for receiving the data.
	Finally, the application callback is invoked with the requisite 
	net_buffers.
******************************************************************************/

	Net_Desc *this_nd;
	packet_number pkt_no;
	U16 pkt_size;
	U32 cb_result;
	U32 res;

	pkt_no = completed_rx_req->pkt_num;
	/* Now, the DMA buffer needs to be loaded into the net_buffers
	   already obtained from the app.
	*/
	pkt_size = completed_rx_req->pkt_size - completed_rx_req->pkt_offset;
	res = SMC_Load_Net_Buffer(	completed_rx_req->net_buffer,
				completed_rx_req->pkt_offset,
				pkt_size-1,
				1,
				(U32)NO_CACHE(&completed_rx_req->dma_buffer));
	if( !res)
	{
		this_nd = &smc_data[SMC_IDX_FROM_ND(pkt_no)].descriptors[SMC_ND_IDX_FROM_ND(pkt_no)];
		if ( this_nd->Net_Callback && (this_nd->capabilities & CB_Will_Receive)) {
			this_nd->n_stats.receive++;
			cb_result = (this_nd->Net_Callback)(
				      CB_Receive,        /* Receive request */
				      this_nd->nd_id,
				      pkt_no,
				      (U32)pkt_size,
				      (U32)completed_rx_req->net_buffer,
				      0,
				      0 );
		}
#if DMA_STATS
		dma_stats.unloads++;
#endif  /* DMA_STATS */

		SMC_Chip_Pkt_Rls(pkt_no); /* Free up this packet */
 
	}
	else 
	{    /* Bad unload - CRC messed up */
#if DMA_STATS
		dma_stats.bad_unloads++;
#endif  /* DMA_STATS */
	}
	
	return res;

} /* SMC_RX_DMA_Done */

void SMC_TX_DMA_Done(SMC_DMA_Req *completed_tx_req)
{
/******************************************************************************
	This routine is invoked when a DMA completes copying data into
	packet memory.
	All that needs to be done is to initiate the TX on the chip.
******************************************************************************/

	packet_number pkt_no = completed_tx_req->pkt_num;
	packet_number act_pkt_no;

	act_pkt_no = 0x1f & SMC_READ_REG(smc_data[SMC_IDX_FROM_ND(pkt_no)].port,SMC_PNR);

	if ( act_pkt_no != SMC_DRV_PKT(pkt_no) ) 
	{
#if DMA_STATS
		dma_stats.bad_tx_pnrs++;
#endif /* DMA_STATS */
	}
		
	/* Set up the Packet Number Register so SMC knows which packet we mean */
	SMC_WRITE_REG(smc_data[SMC_IDX_FROM_ND(pkt_no)].port,
		SMC_PNR,SMC_DRV_PKT(pkt_no));
        SMC_WRITE_REG(smc_data[SMC_IDX_FROM_ND(pkt_no)].port,SMC_MMUCR,SMC_MMU_TXPUSH);
        smc_data[SMC_IDX_FROM_ND(pkt_no)].p_stats.txqd++;

        SMC_Set_Int_Mask(&smc_data[SMC_IDX_FROM_ND(pkt_no)], SMC_TX_INT , 1);

#if DMA_STATS
	dma_stats.loads++;
#endif
} /* SMC_TX_DMA_Done */

U32 SMC_Start_DMA(network_descriptor nd_id)
{
	/* This function kicks of DMA transfers, if it needs to be done. */
	/* The TX queue should have the DMA request already posted, but  */
	/* a packet is still needed on the chip.			 */
	/* An alloc is attempted, and if it succeeds, the request is     */
	/* moved to the proper DMA queue. If it fails, then it should    */
	/* complete some time later.					 */
	/* In any case, if there is a DMA xfer queued, it is initiated.  */

	U32 res;
	SMC_Data *smc;
	smc = &smc_data[SMC_IDX_FROM_ND(nd_id)];

	SMC_Initiate_Next_Allocate(nd_id);
	/* That will have moved the request to the dma_queue if there    */
	/* was a packet, or it will happen later, when the allocate      */
	/* actually happens.						 */

	/* So lets grab the chip, and start a DMA going */
	EnterCritSec();							/*C*/
	if (smc->mode)							/*C*/
	{	/* Chip NOT available */				/*C*/
		res = NETOK_SEND_PENDING;				/*C*/
#if DMA_STATS
		dma_stats.blockeds++;	/* we've been blocked */
#endif /* DMA_STATS */
	}								/*C*/
	else								/*C*/
	{
		SMC_DMA_Req *this = smc_dma_queue;
		if(this)
		{
			this = smc_dma_queue->next;			/*C*/
		}
		if (Galileo_Initiate_Next_SMC_DMA(this))		/*C*/
		{							/*C*/
			res = NETOK_SEND_COMPLETE;			/*C*/
			smc->mode ++;					/*C*/
#if DMA_STATS
			dma_stats.blocks++;
#endif /* DMA_STATS */
		/* NOTE: The SMC is now blocked until the DMA finishes. */
		}							/*C*/
		else {							/*C*/
		/* problem with the Initiate_Next_SMC_DMA */
			res = NETERR_DMA_ERROR;				/*C*/
#if DMA_STATS
			dma_stats.bad_inits++;
#endif /* DMA_STATS */
		}							/*C*/
	}								/*C*/
	ExitCritSec();							/*C*/

	return res;
}
	
