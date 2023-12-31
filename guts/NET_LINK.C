/**********************************************************************/
/*File:           Net_Link.c                                          */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       Function Prototypes for the Network Link driver     */
/*                                                                    */
/*(C) Copyright Time Warner Interactive 1995                          */
/*                                                                    */
/*      This file implements the top-most functions for the network   */
/*      interface.                                                    */
/**********************************************************************/

#include <net_link.h>
#include <net_data.h>
#include <net_smc.h>
#include <smc_link.h>
#include <smc_dma.h>
#include <string.h>
#include <stdarg.h>
#include <os_proto.h>

Net_Buffer_Pool buffer_pool;

/* This is a sort-of private thingie that points to the action to take place */
/* to handle an SMC interrupt. It gets initialized to point to plain old     */
/* Net_Interrupt_Handler, but may get bent to point to the NU_Activate_HISR  */
/* which in turn will do the Net_Interrupt_Handler, but this time in a       */
/* Nucleus aware context.                                                    */
struct act_q smc_act_q_elt;

static Net_Desc nd_pool[SMC_CHIP_TOTAL * NET_DESCRIPTORS_TOTAL];

U32 Net_Initialize(U8 *nb_pool, U32 pool_size,
                 void (*handler)(),void *func)
{
/*************************************************************************/
/*        This function is called at 'boot' time by some kind startup    */
/*        demon. Here, knowledge about the hardware is assumed, in the   */
/*        form of where any SMC chips may be found in this build, and    */
/*        how many and what sort of buffers, etc, need to be initiated.  */
/*                                                                       */
/*************************************************************************/
    int rtcode,res=NETERR_BADPARAMETER;
    U8 *step;

    /* Initialize the global Net_Buffer_pool */
    step = nb_pool;
    memset(&buffer_pool,0,sizeof(buffer_pool));
    buffer_pool.pool_address = nb_pool;
    buffer_pool.pool_size = pool_size;
    buffer_pool.pool_chunks = sizeof(Net_Buffer);

    while (step<(nb_pool+pool_size)) {
       /* Fake up a buffer in the raw pool, and then 'free' it */
        ((Net_Buffer *)step)->flags = NB_INUSE;
        ((Net_Buffer *)step)->self = (Net_Buffer *)step;
        ((Net_Buffer *)step)->len = 0;     /* Flag as empty buffer */
        ((Net_Buffer *)step)->size = NET_BUFFER_SIZE;
        ((Net_Buffer *)step)->next = 0;
        if(NETOK != (res = Net_Free_Buffer((Net_Buffer *)step,0)))
            break;
        step += sizeof(Net_Buffer);
    }
    smc_act_q_elt.next = 0;
    smc_act_q_elt.que = 0;
    if(handler) {
         smc_act_q_elt.action = handler;
         smc_act_q_elt.param = func;
    }
    else {
         smc_act_q_elt.action = Net_Interrupt_Handler;
         smc_act_q_elt.param = 0;
    }
    /* Clear the area where ALL Net_Desc get put / allocated from */
    memset((void *)nd_pool,0,sizeof(nd_pool));
    /* Initialize the SMC Driver and get the chip started */
    rtcode = SMC_Initialize(nd_pool);

    return res==NETOK ? rtcode: res;
}

network_descriptor Net_Open_Interface(
        char *device,             /* Upto 8 chars - name of device to open   */
        net_callback_fn *cb,      /* address of the callback function        */
        Net_CB_Caps caps,         /* Or'd number indicating the functions    */
                                  /* the cb supports.                        */
        int sig_len,              /* the number of bytes needed in the sig.  */
        int sig_offset            /* the offset from the start of the packet */
                                  /* to the signature bytes.                 */
)
{
/*****************************************************************************/
/*        Currently, two device classes are supported - SMC 91C92/4 and      */
/*        loopback. These are called 'SMCx' and 'loopback' [ x is replaced   */
/*        with the specific SMC desired. 0 is the first one, etc ]           */
/*****************************************************************************/
    network_descriptor res;
    Net_Desc *new_nd;
    /* open the device requested, and get a new nd area */
    new_nd = SMC_Open(device);
    if ( new_nd != 0) {
       /* OK, there is a device that supports the request */
       /* So fill in the reset of the new_nd              */
       new_nd->Net_Callback = cb;
       new_nd->capabilities = caps;
       new_nd->signature.sig_len = sig_len;
       new_nd->signature.sig_offset = sig_offset;
       res = new_nd->nd_id;
       SMC_Add_Sigs(new_nd);    /* Set up the signature for the new one */
    }
    else {
       res = 0;
    } /* endif */
    return res;
} /* Net_Open_Interface */

U32 Net_Close_Interface(network_descriptor nd_id)
{
    return SMC_Close(nd_id);
}  /* Net_Close_Interface */

U32 Net_Send_Packet(
        network_descriptor nd,
        const netadd dest,
        Net_Buffer *pkt,
        int opts
)
{
/*********************************************************************/
/*   This routine will take a chain of buffers from the application, */
/*   prepend the packet header, and passes it down to the driver.    */
/*********************************************************************/

        int pkt_size;
        int res=0;
        packet_number pkt_num;
        Net_Buffer *header, *nb;
#if SMC_DMA & 1
	SMC_DMA_Req* tx_req;
#endif

        /* Verify that the ND is valid */
        if ((nd == 0) ||
            (pkt == 0)) return NETERR_BADPARAMETER;
        if (!SMC_Is_Open(nd)) return NETERR_BADPARAMETER;

        /* Preset the packet number, indicating invalid on-chip buffer (-1) */
        pkt_num = nd | (SMC_Stats_Inc(nd,txin) << 17) | SMC_PKT_DRV(-1);
        SMC_ND_Stats_Inc(nd,sends);
        pkt->flags |= NB_TXPND | NB_INUSE;
        header = 0;
        do {    /* Loop construct to have easy break out */
            /* Request buffer for header */
            res = Net_Get_Buffer(&header,
                                 NET_PKT_HEADER_SIZE+sizeof(network_descriptor),
                                 NETOPT_NONBLOCK | opts );
            if ( res != NETOK ) {
               break;      /* To enddo */
            } /* endif */
            res = SMC_Init_Header(pkt_num,header,dest);
            if (res & NETERR_MASK) {
                Net_Free_Buffer(header,NETOPT_NONBLOCK);
                break;      /* To enddo */
            } /* endif */

            /* Place header at head of buffer chain */
            header->next = pkt;
            /* Check the buffer chain, gauge length */
            pkt_size = 0;
            for (nb = header; nb != 0 && pkt_size< 1600 ;nb=nb->next) {
                pkt_size += nb->len;
            } /* endfor */

		/* We now have a chain of net_buffers that is the packet to go.
		   Let's see if a DMA request can be made, otherwise,
		   do it by hand.
		*/
#if SMC_DMA & 1	/* Use DMA for TX requests */
		if (( tx_req = DMA_Dequeue(&smc_dma_free))) {	/* A DMA Request was available, */
			tx_req->net_buffer = header;
			tx_req->req_type = SMC_DMA_ALLOC;
			tx_req->pkt_size = pkt_size;
			tx_req->pkt_offset = 0;
			tx_req->pkt_num = pkt_num;  /* Still a *non* chip packet number */
			SMC_Copy_Net_Buffer(header,pkt_size,1,(U32)NO_CACHE(&tx_req->dma_buffer));
			DMA_Enqueue(&smc_dma_tx_queue,tx_req);
			SMC_Stats_Inc(nd,txpnd);

			if( !smc_dma_req )  /* If no request is in progress, initiate one */
			{
				if ( SMC_Start_DMA(nd))
					res = NETOK_SEND_COMPLETE;
				else
					res = NETERR_DMA_ERROR;
			}
			else
			{
				res = NETOK_SEND_COMPLETE;
			}
		}
		else /* No DMA, so do it ourselves */
#endif /* SMC_DMA */
		{
			/* Here we try and get the required buffers from the chip */
			/* If there are no buffers available at this time, a      */
			/* request will have been queued and an alloc interrupt   */
			/* will occur to satisfy the request.                     */

			res = SMCP_Alloc_Buff(nd,pkt_size,&pkt_num,opts);

			/****** NOTE For the time being, things have been streamlined      */
			/* so only COMPLETE or NORESOURCE can happen. This is done to */
			/* eliminate having to queue the packets, and to avoid having */
			/* to match up aquired buffers with packets. Main reason is   */
			/* create a version that does NOT use Nucleus in the driver.  */
			/* The old code is still here, in case we ever resurrect it   */
			if ((res == NETOK_SEND_PENDING) | (res == NETERR_SEND_NORESOURCE)) {
				res = NETERR_SEND_NORESOURCE ;
				/* res is no-resource, so no alloc requested. */
				SMC_ND_Stats_Inc(nd,no_resrc);
				break; /* to enddo */
			}
			else { /* it is a send_complete - that is, buffer requested and available */
				/* Copy data and enqueue it */
				/* if the allocate was pending, then this is done in  */
				/* the allocate interrupt service routine.            */
				res = SMCP_TX_Enqueue(pkt_num,pkt_size,header,opts);
			}
		}
        } while ( 0 ); /* enddo (once only!)*/

        if (header &&         /* Header could be 0, if Get_Buffer failed! */
             res != NETOK_SEND_PENDING ) {
            /* The first buffer was provided by us, so free it. */
            header->next = 0; /* BUT only the first buffer - the rest is the apps! */
            Net_Free_Buffer(header,NETOPT_NONBLOCK);
        }
    return res;
}

static Net_Buffer *net_deq( void )
{
 int old_ipl = prc_set_ipl(INTS_OFF);
 Net_Buffer *dnb = buffer_pool.free_buffer;

 if ( dnb )
 {
  buffer_pool.free_buffer = dnb->next;
  if ( buffer_pool.free_buffer == (Net_Buffer *)0 )
       buffer_pool.last_buffer = (Net_Buffer *)0;
  dnb->next = (Net_Buffer *)0;
  --buffer_pool.stats.freed_buffers;
 }

 prc_set_ipl(old_ipl);
 return dnb;
}

U32 Net_Get_Buffer( Net_Buffer **return_pointer, int size, int opts )
{
 if ( ( *return_pointer = net_deq() ) == 0 ) return NETERR_NOMEMORY;
 (*return_pointer)->offset = 0;
 return NETOK;
}

static void net_que( Net_Buffer *qnb )
{
 int old_ipl = prc_set_ipl(INTS_OFF);
 if ( buffer_pool.last_buffer )
 {
  buffer_pool.last_buffer->next = qnb;
  buffer_pool.last_buffer = qnb;
 }
 else
 {
  buffer_pool.free_buffer = buffer_pool.last_buffer = qnb;
 }
 buffer_pool.last_buffer->next = (Net_Buffer *)0;
 ++buffer_pool.stats.freed_buffers;
 prc_set_ipl(old_ipl);
}


U32 Net_Free_Buffer( Net_Buffer *buffer_pointer, int opts )
{
 while ( buffer_pointer )
 {
  Net_Buffer *tmp = buffer_pointer->next;
  net_que( buffer_pointer );
  buffer_pointer = tmp;
 }
 return NETOK;
} /* Net_Free_Buffer */

U32 Net_Control(
	network_descriptor nd,	    /* Network Descriptor of already opened  */
				    /* interface which needs an out-of-band  */
				    /* command or state change.		     */
	Net_Control_Cmd command,    /* Command to be performed.		     */	
	...			    /* variable numbers of parameters to     */
				    /* satisfy command.			     */	
)
{
    va_list	ap;
    U32 ret = NETOK;
    if(SMC_Is_Open(nd)) {
	va_start(ap,command);
	switch(command) {
	case NETCTL_CB_CAPABILITY:{	/*	Net_CB_Caps	Capability to change
	     ====================		U32	   0 = Capability off
							  ~0 = Capability supported */
	    U32 cap = va_arg(ap,U32);
	    if(va_arg(ap,U32))	/* Turn it on, or off ? */
	    	smc_data[SMC_IDX_FROM_ND(nd)].descriptors[SMC_ND_IDX_FROM_ND(nd)].capabilities |= cap; 	
	    else
	    	smc_data[SMC_IDX_FROM_ND(nd)].descriptors[SMC_ND_IDX_FROM_ND(nd)].capabilities &= ~cap; 	
	    break;
	}
	case NETCTL_MC_CHANNEL: {	/*	U32	  Channel number 0-63
	     =================			U32	  0 = turn of listening to MC
							 ~0 = turn on receiving on MC */
	    U32 chan;
	    if (64>(chan = va_arg(ap,U32)))
		SMC_Set_MC_Mask(&smc_data[SMC_IDX_FROM_ND(nd)], chan, va_arg(ap,U32));
	    else
		ret = NETERR_BADPARAMETER;
	    break;
	}
	case NETCTL_MC_PROMISCOUS: {	/*	U32	 0 = MC Promiscous off
	     ====================			~0 = MC Promiscous mode */
    	    SMC_Set_MC_Mask(&smc_data[SMC_IDX_FROM_ND(nd)],64,va_arg(ap,U32));
	    break;
	}	
	case NETCTL_PROMISCOUS: {	/*	U32	 0 = Promiscous off
	     ====================			~0 = Promiscous mode */
    	    SMC_Set_MC_Mask(&smc_data[SMC_IDX_FROM_ND(nd)],65,va_arg(ap,U32));
	    break;
	}	

	case NETCTL_LOOPBACK: {		/*	U32	 NET_LOGICAL_LOOPBACK
    	     ====================			 NET_PHYSICAL_LOOPBACK
							 NET_FULLDUPLEX
							 NET_HALFDUPLEX		*/
    	    SMC_Set_Loopback(&smc_data[SMC_IDX_FROM_ND(nd)],va_arg(ap,U32));
	    break;
	}
	case NETCTL_NULL:                 	       /* NONE
	     ===========*/
	    break;
	} /* end SWITCH */
        va_end(ap);
    }
    else /* Bad parameter - ND not valid */
	ret = NETERR_BADPARAMETER;
    return ret;

} /* Net_Control */
static const U8 xlate[16] = {0,26,13,23,38,60,43,49,19,9,30,4,53,47,56,34};
void Net_Aquire_MC( network_descriptor nd, U32 node_id, netadd *aquired_add)
{
/*
	This is a crude hack to turn the systems node-id into
	the equivalent channel, assuming the com address is
	set as in the second column.
            Channel	Actual Address
		 0	    Atari0
		26	    Atari1
		13	    Atari2
		23	    Atari3
		38	    Atari4
	        60	    Atari5
	        43	    Atari6
	        49	    Atari7
		19	    Atari8
		 9	    Atari9
		30	    Atari:
		 4	    Atari;
		53	    Atari<
		47	    Atari=
		56	    Atari>
		34	    Atari?
    */
    smc_data[SMC_IDX_FROM_ND(nd)].mc[node_id & 0x0f] = 1;
    Net_Control(nd,NETCTL_MC_CHANNEL,xlate[node_id & 0x0f], 1 );
    (*aquired_add)[0] = 'A'; 
    (*aquired_add)[1] = 't'; 
    (*aquired_add)[2] = 'a'; 
    (*aquired_add)[3] = 'r'; 
    (*aquired_add)[4] = 'i';
    (*aquired_add)[5] = '0'+node_id;
    return ; 
}
U32 Net_Release_MC(const network_descriptor nd,const U32 node_id , const netadd mc_add)
{

    netadd adr = {'A','t','a','r','i','0'};
    U32 res=NETOK;
    if (!SMC_Is_Open(nd)) return NETERR_BADPARAMETER;
    adr[5] += node_id;
    if(!memcmp(mc_add,adr,6))
    {
	    smc_data[SMC_IDX_FROM_ND(nd)].mc[node_id & 0x0f] = 0;
	    Net_Control(nd,NETCTL_MC_CHANNEL,xlate[node_id & 0x0f], 0 );
    }
    else
	res =NETERR_BADPARAMETER;
    return res;
}
void Net_Restore_MC( SMC_Data *smc )
{
    int ii;

    for ( ii = 0; ii < 10; ++ii )
     Net_Control( smc->descriptors->nd_id, NETCTL_MC_CHANNEL, xlate[ ii ], smc->mc[ ii ] );
}

/**************************************************************/
/*    This routine will attempt to call the application's     */
/*    or game's Relinquish Callback function. It is essential */
/*    that if one *is* defined, it actually relinquishes to   */
/*    the OS, as this func may be called when resources are   */
/*    busy or locked by other tasks, which will need to run   */
/*    before we can proceed.                                  */
/*    NETOPT_NONBLOCK is returned to indicate that we can't   */
/*    (or won't) relinquish, otherwise NETOPT_BLOCKING is.    */
/**************************************************************/
U32 Net_Relinquish(const network_descriptor nd_id)
{
	U32 ret = NETOPT_NONBLOCK;
	Net_Desc *this_nd = &smc_data[SMC_IDX_FROM_ND(nd_id)].descriptors[SMC_ND_IDX_FROM_ND(nd_id)];
	if ((this_nd->capabilities & CB_Will_Relinquish) && !(this_nd->capabilities & CB_Non_Block))
	{   /* There is Relinquish support in the callback function, and the */
	    /* mode is NOT overridden to non-blocking _only_.                */
            ret = (this_nd->Net_Callback)(
			CB_Relinquish,        /* Relinquish request */
			nd_id, 0, 0, 0, 0, 0);
	    ret = ret==CB_OK ? NETOPT_BLOCKING : NETOPT_NONBLOCK;
	    
	}
	else	/* we can't do it */
		ret =  NETOPT_NONBLOCK;
	return ret;
}
