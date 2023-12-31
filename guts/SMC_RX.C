/**********************************************************************/
/*File:           smc_rx.c                                            */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       SMC Receive functions.                              */
/*                                                                    */
/*(C) Copyright Time Warner Interactive 1995                          */
/*                                                                    */
/*      This file implements the device level function of the network */
/*      interface. The device supported, obviously, is the SMC 919c92 */
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

#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b?a:b)

static U32 SMC_RX_Packet(SMC_Data *smc,
                          U16 pkt_status ,
                          U16 pkt_size,
                          packet_number pkt_num);
static void SMC_Offload(
                SMC_Data *smc,
                packet_number pkt_no,   /* If -1, then use RX FIFO pkt no. */
                U16 start,              /* byte into packet to start at    */
                U16 end,                /* last byte we want in the packet */
                Net_Buffer *buff        /* buffer to store it all in.     */
);
static smc_rx_too_short;
void SMC_RXHndlr(SMC_Data *smc)
{
    U16 old_PTR;
    U16 pkt_status, pkt_size;
    U32 rx_res=0;		/* 0 if we've done with the packet, otherwise will be released later. */
    U32 pkt_num,busy_loop;      /* Used to prevent loops on the same pkt */
 
    /* Now, save the value of the pointer register, then use it */
    old_PTR = SMC_READ_REG(smc->port,SMC_PTR);

    do {/* At least one packet to handle, maybe more ... */
	/* Set up the packet data pointer. Do it here, so that the first */
	/* data access is somewhat delayed to avoid timing violations    */
	/* on the chip access. On a realy _fast_ system, perhaps more    */
	/* delay is needed...						 */
	/* We need the first word of the packet data. So no offset needed */
	SMC_WRITE_REG(smc->port,SMC_PTR,SMC_PTR_RCV|SMC_PTR_AUTO_INC|SMC_PTR_READ);

        /* get the packet num - for RX, it is in byte 1 */
        pkt_num = SMC_READ_REG(smc->port,SMC_FIFO) & SMC_FIFO_RXMSK;

	{
		/* The data pointer was already set up a little earlier. (> 400nS ago, I hope...) */
		/* and now get the data, which is the pkt status of the first RCV        */
		/* packet on the Rx FIFO queue                                           */
		pkt_status = SMC_READ_REG(smc->port,SMC_DATA);
		pkt_size = SMC_READ_REG(smc->port,SMC_DATA) - 6 ;
		/* the size needs to be tweaked, as it includes control words, etc. */
		/* Also, the last 4 bytes should be the CRC, which we get first */

		/* Is it an odd size frame ? */
		if ( pkt_status & SMC_ODDFRM ) pkt_size++;

		/* Now, let's see what we have here.        */
		/* Any errors ?                             */
		if (pkt_status & SMC_RCV_ERROR) {
		    /* Some errors in this packet, so handle and discard */
		    if (pkt_status & SMC_ALGN_ERR) smc->e_stats.algn_err++;
		    if (pkt_status & SMC_BADCRC) smc->e_stats.bad_crc++;
		    if (pkt_status & SMC_TOO_LONG) smc->e_stats.too_long++;
		    if (pkt_status & SMC_TOO_SHORT) smc->e_stats.too_short++;
		    rx_res = 0;	/* We've done with this packet. Free it */
		}
		else
		{
		    if (pkt_size < 64) {
		    ++smc_rx_too_short;
		    rx_res = 0;	/* We've done with this packet. Free it */
		    }
		    else
		    {
			/* Only count good packets... */
			/* Count the packet size in the size stats */
			/* local pointer to the actual s_stats employed */
			struct SMC_Size_Stats *stat;
			U32 *p;
			U16 size ;
			stat = &smc->s_stats;
			if (pkt_size < 128) {
				stat->pkt_64to128++;
			} 
			else if (pkt_size<= MAX_ENET_PACKET_SIZE)
			{
				size = (pkt_size >> 8);
				p = (U32 *)stat;
				p[size+1]++;
			} /* endif */
			/* That's it for pkt sizes. */
			if (pkt_status & SMC_BROADCAST) {
			    /* A broadcast, so we have to take it. */
			    smc->p_stats.bcasts++;
			}
			else if (pkt_status & SMC_MULTICAST) {
			    smc->p_stats.mcasts++;
			}
			else { /* Well, a regular Packet - whatever next ? */
			    smc->p_stats.recvs++;
			} /* endif */
			rx_res = SMC_RX_Packet(smc,pkt_status,pkt_size,pkt_num);
		    }
		}
	}

	/* We've done with that packet, so release.  */
	if (rx_res)
	{	/* The packet needs to be kept arround, probably for DMA offloading */
		/* But we DO need to remove the pkt from the RX FIFO... */
		SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_RXPOP);
	}
	else
	{	/* All done with the packet, Neeeeext */
		SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_RXRLS);
	}
	busy_loop = SMC_BUSY_LOOP;

	while (busy_loop && (SMC_READ_REG(smc->port,SMC_MMUCR) & SMC_MMU_BUSY)) {
	    /* The release is in progress, so hang about */
	    smc->control.rbusys++;
	    busy_loop--;
	} /* endwhile BUSY*/

	if (!busy_loop) {
	    smc->control.drv_errs++;
	    SMC_WRITE_REG(smc->port,SMC_MMUCR,SMC_MMU_RESET);
	    break; /* If MMU_busy stayed, bail out */
	   /************** WE SHOULD THINK OF SOMETHING MORE INTELLIGENT TO DO HERE!! ***/
	}
	    /* BUT, if there is another packet in the RXFIFO, do it now.  */
    } while (SMC_READ_REG(smc->port,SMC_ISR)& SMC_RCV_INT); /* End of DO loop */

    /* Restore the pointer register, wouldn't want to wedge someone.. */
    SMC_WRITE_REG(smc->port,SMC_PTR,old_PTR);
}

void SMC_RXError(SMC_Data *smc)
{
 U16 rx_control;

 SMC_SET_BANK(smc->port,SMC_BANK(SMC_RCR));

 rx_control = SMC_READ_REG(smc->port,SMC_RCR);

 if (rx_control & SMC_RX_ABORT)
 {
  smc->e_stats.rx_jammed++;
  SMC_WRITE_REG(smc->port,SMC_RCR,rx_control & ~SMC_RX_ABORT);
 }
 else
 {
  smc->e_stats.rx_nomem++;
#if 0
  SMC_WRITE_REG(smc->port,SMC_RCR,rx_control & ~SMC_RX_ENABLE);
#endif
 }

 SMC_SET_BANK(smc->port,SMC_BANK(SMC_ISR));
 SMC_WRITE_REG(smc->port,SMC_ISR,SMC_RXOVRN_INT);
} /* SMC_RXError */

static U16 SMC_Prefetch_Sigs(SMC_Data *smc, U16 pkt_size, Net_Buffer **buff )
/****************************************************************************/
/*     Prefetch the signatures,if possible/allowed. sig_ctl shows what      */
/*     is - if copy_all, get the packet first, then pass it arround,        */
/*     - if copy_sigs, get the part that covers the sigs, and               */
/*     - if copy_none, do nothing here - prefetch is OFF                    */
/****************************************************************************/
{
    U16 start_offset=0, end_offset=0;
    U16 lsig_mode;              /* A local copy, so we can override it if needed */

    lsig_mode = smc->sig_ctl.sig_mode;
    if ( lsig_mode == SMC_SIG_COPY_ALL ) {
        start_offset = 0;
        end_offset = pkt_size & 0X1FFF;
    }
    else if (lsig_mode == SMC_SIG_COPY_SIGS ) {
        start_offset = smc->sig_ctl.sig_low;
        end_offset = min(smc->sig_ctl.sig_high,(pkt_size & 0x1fff));
    }
    if ((lsig_mode == SMC_SIG_COPY_ALL)|(lsig_mode == SMC_SIG_COPY_SIGS)) {
        /* Copy the pre-set amount into a local buffer, if one can be had */
        if ( NETOK == Net_Get_Buffer(buff,end_offset-start_offset+1, NETOPT_NONBLOCK)) {
            /* buff is now a buffer chain big enough to hold our needs. */
            /* So copy the stuff of the chip into it.                     */
            SMC_Offload(smc,-1,start_offset,end_offset,*buff);
        }
        else {  /* no buffer available, so fall back to copy_none mode */
             lsig_mode = SMC_SIG_COPY_NONE;
             *buff = 0;
        }
    }
    return lsig_mode;
} /* SMC_Prefetch_Sigs */

static U32 SMC_RX_Packet(SMC_Data *smc,
                          U16 pkt_status ,
                          U16 pkt_size,
                          packet_number pkt_num)
{
/****************************************************************************/
/*     This function gets the signatures, and passes it upto                */
/*     the application, or whomever is interested.                          */
/*     For each network descriptor, prepare to do the callback,             */
/*     and handle the results.                                              */
/*     An optimisation that may be tried is to keep track of where the      */
/*     copy_to is going, and using it as a source if there is any overlap...*/
/****************************************************************************/
    Net_Buffer *lbuff,*your_pkt_buffer,nb_dummy;
    U16 lsig_mode;         /* A local copy, so we can override it if needed */
    Net_Desc *this_nd;
    int i,cb_result;
    U32 res=0;	/* Indicate whether the packet has been finished with (0), or not (1) */

    lsig_mode = SMC_Prefetch_Sigs(smc,pkt_size,&lbuff);

    nb_dummy.flags = NB_RX | (pkt_status & SMC_BROADCAST)?NB_BCAST:0 |
                         (pkt_status & SMC_MULTICAST)?NB_MCAST:0 ;
    /* Now, for each registered callback who will filter, do the callback */
    for (  this_nd = smc->descriptors,i=0;   /* Get the first one */
           i<NET_DESCRIPTORS_TOTAL;          /* There can only be this many */
           i++,this_nd++ ) {                 /* step to next one */
        /* Step through the net_descriptor list */
        if ( this_nd->nd_id == 0 )
            continue ;        /* note there may be holes in it ! */
        if ( this_nd->Net_Callback && (this_nd->capabilities & CB_Will_Filter)) {
           U8 *psig_buff,         /* pointer to bytes the filter wants to see */
               sig_buff[NET_FIXED_SIG_SIZE + sizeof(Net_Buffer)];
                                  /* Temp holding place, in case needed. Only */
                                  /* used if we've been unable to get any sigs*/
                                  /* of the chip.                             */
           /* Ok, here is a nd who wants to filter on this packet */
           if (this_nd->signature.sig_len) {
                /* There is a signature requested, so figure   */
                /* out where the signature bytes are at        */
                if (lsig_mode == SMC_SIG_COPY_NONE) {
                   /* We have to get just the sig bytes into our little local buffer */
                   /* IFF it will fit... */
                   if (this_nd->signature.sig_len <= NET_FIXED_SIG_SIZE) {
                      /* It should fit */
                      lbuff = (Net_Buffer *)&sig_buff;
                      lbuff->size = sizeof(sig_buff) - sizeof(Net_Buffer);
                      lbuff->offset = 0 ;
                      lbuff->next = 0 ;
                      SMC_Offload(smc, -1,
                                  this_nd->signature.sig_offset,
                                  this_nd->signature.sig_offset + this_nd->signature.sig_len-1,
                                  lbuff);
                      psig_buff = &lbuff->data[0];
                   }
                   else {
                      this_nd->n_stats.no_resrc++;
                      continue;      /* Can't do anything for this guy */
                   } /* endif */
                }
                else if (lsig_mode == SMC_SIG_COPY_SIGS ) {
                    /* lbuff already contains all the signatures, but only */
                    /* to cover ALL the signatures. SO the one we need is  */
                    /* at an offset from in the actual packet, so we need  */
                    /* to adjust accordingly.                              */

/************************** TO BE FIXED UP *******************************/
/******* The signature may _NOT_ lie entirely in the first buffer !  *****/
                    psig_buff = &lbuff->data[this_nd->signature.sig_offset-smc->sig_ctl.sig_low];

                }
                else if (lsig_mode == SMC_SIG_COPY_ALL ) {
                    /* lbuf contains entire packet, so merely offset into it... */

/************************** TO BE FIXED UP *******************************/
/******* The signature may _NOT_ lie entirely in the first buffer !  *****/
                    psig_buff = &lbuff->data[this_nd->signature.sig_offset];
                }
                else {
                     /* This condition is very anomalous - should at least be */
                     /* counted somewhere...                                  */
                     continue;
                } /* endif */
           }
           else {
               /* The filter cares for no signature, so at least pass a NULL */
               psig_buff = 0;
           } /* endif */
           /* If signatures are required, psig_buff points to them */
           /* So go ahead, do the upcall...                   */
           this_nd->n_stats.filter++;
           smc->p_stats.filtcalls++;
	   your_pkt_buffer = &nb_dummy;
           cb_result = (this_nd->Net_Callback)(
                              CB_Filter,        /* Filter request */
                              this_nd->nd_id,
                              pkt_num,
                              (U32)pkt_size,
                              (U32)this_nd->signature.sig_len,
                              (U32)psig_buff,
                              (U32)&your_pkt_buffer);
           /* Wow, we've gone and called a filter... Now what? */
           /* Well, most returns aren't implemented yet -      */
           /* only need to worry about CB_Copy_Packet, for now */

	   switch (cb_result) {
		   case CB_OK:          /* Here, this causes the packet to be dropped */
		   case CB_FAILED:      /* Ditto */
		   case CB_Drop_Packet: /* BUT we realy would like to see this if your */
					/* not interested...                           */
		       this_nd->n_stats.filt_drop++;
		       break;
		   case CB_Copy_Packet: 
		   case CB_Copy_Part: {
			U16 start,end;

#			if SMC_DMA & 2	/* Use DMA for RX requests */
			SMC_DMA_Req* rx_req;	/* DMA request record, if enabled */
#			endif /* SMC_DMA */
			this_nd->n_stats.filt_pass++;
			if(your_pkt_buffer == &nb_dummy) {
			   /* didn't actually give me a buffer !!! */
				this_nd->n_stats.errors++;
				break;
			}
			if(your_pkt_buffer->flags & NB_FREE) {
			/* buffer was not allocated properly!!! */
				this_nd->n_stats.errors++;
				break;
			}
			smc->p_stats.recvups++;
			this_nd->rx_buff = your_pkt_buffer;
			if(cb_result == CB_Copy_Part) {
				start = (U16)your_pkt_buffer->data[your_pkt_buffer->offset];
				end = start + (U16)your_pkt_buffer->data[your_pkt_buffer->offset+2];
			}
			else {
				start = 0;
				end = pkt_size-1;
			}
			your_pkt_buffer->flags |= NB_RX |
				 (pkt_status >> 1 & NB_HASH) |
				 (pkt_status & SMC_BROADCAST)?NB_BCAST:0 |
				 (pkt_status & SMC_MULTICAST)?NB_MCAST:0 ;

#if SMC_DMA & 2	/* Use DMA for RX requests */
			if (( rx_req = DMA_Dequeue(&smc_dma_free))) {	/* A DMA Request was available, */
				rx_req->net_buffer = your_pkt_buffer;
				rx_req->req_type = SMC_DMA_RX;
				rx_req->pkt_size = end - start +1;
				rx_req->pkt_offset = start;
				rx_req->pkt_num = SMC_RXPKT_DRV(pkt_num) | this_nd->nd_id;
				DMA_Enqueue(&smc_dma_queue,rx_req);
				res = 1;	/* The packet is still being accessed - DO NOT RELEASE yet */
			}
			else    /* No DMA Request buffers, do it direct */
#endif /* SMC_DMA */
			{
				SMC_Offload(smc,-1,start,end & 0X1FFF,your_pkt_buffer);
				/* Packet present and acounted for in the given buffer  */
				/* Now tell 'm it's there. If not Will_Receive, perhaps */
				/* he polls the buffer, somehow? ( not MY problem... )  */
				if ( this_nd->Net_Callback && (this_nd->capabilities & CB_Will_Receive)) {
					this_nd->n_stats.receive++;
					cb_result = (this_nd->Net_Callback)(
						CB_Receive,        /* Receive request */
						this_nd->nd_id,
						pkt_num,
						(U32)pkt_size,
						(U32)your_pkt_buffer,
						0,
						0 );
			       }
			}
		       break;
		    }
		   case CB_Extract_Part:
	/************************** TO BE FIXED UP *******************************/
		       break;
		   case CB_Take_Ownership:
	/************************** TO BE FIXED UP *******************************/
		       break;
		   default:
		       break;
	   } /* endswitch */
        } /* endif */
    } /* endfor */

    /* If signature pre-fetch occured, need to release the buffer used. */
    if ((lsig_mode == SMC_SIG_COPY_SIGS)|(lsig_mode == SMC_SIG_COPY_ALL)) {
        Net_Free_Buffer(lbuff,NETOPT_NONBLOCK);
    } /* endif */

    return res;

} /* SMC_RX_Packet */

static void SMC_Offload(
                SMC_Data *smc,
                packet_number pkt_no,   /* If -1, then use RX FIFO pkt no. */
                U16 start,              /* byte into packet to start at    */
                U16 end,                /* last byte we want in the packet */
                Net_Buffer *buff        /* buffer to store it all in.     */
)
/*****************************************************************************/
/* A function that actually does some work...                                */
/* In this case, move stuff from the on-chip buffer into the Net_Buffer      */
/* provided, from start to end (inclusive).                                  */
/* If the buffer chain isn't long enough, as much as can be fitted is        */
/* copied.                                                                   */
/*****************************************************************************/
{
    U16 ptr_mask;       /* Pre-set a mask to 'or' with the data pointer    */

    /* We need to set up the PNR before getting the data */
    if ( pkt_no != -1 ) {
        SMC_WRITE_REG(smc->port,SMC_PNR,SMC_DRV_PKT(pkt_no));
        ptr_mask = SMC_PTR_AUTO_INC | SMC_PTR_READ;
    }
    else {
        ptr_mask = SMC_PTR_RCV | SMC_PTR_AUTO_INC | SMC_PTR_READ;
    }

    /* Set up the SMC's data pointer so we can access the actual packet  */
    /* on the chip. ( Add 4, as there are 4 bytes of internal stuff )    */
    SMC_WRITE_REG(smc->port,SMC_PTR,ptr_mask | (4+start));

    /* Conveniently, the SMC will auto-increment its data pointer,      */
    /* all that's left to do is to read the data register, and          */
    /* stow it away. It even provides for 32 bit access, but this is    */
    /* not supported on the stream I/O board, so is not used.           */
    /* But still need a little house-keeping to handle odd starts and   */
    /* and ends.                                                        */
    if( 0 > (end - start)) {
        /* Nothing to do, so fail safe */
        buff->len = 0;
    }
    else {
int old_ipl = prc_set_ipl(INTS_OFF);
	SMC_Load_Net_Buffer(buff,start,end,0,smc->port);
prc_set_ipl(old_ipl);
    }
} /* SMC_Unload */

U32  SMC_Load_Net_Buffer( Net_Buffer* buff, U16 start, U16 end, U32 mode, U32 source )
{
/*********************************************************************
	This function will move data into a chain of net_buffers, 
	taking into acount the size and offset of each. If the
	source is the SMC, start and end are the first and last
	bytes in the whole packet we're unloading. If the source
	is a DMA buffer, start is 0, as only the part of the packet
	we're interested in has been DMA'd. Also, if sourcing from
	DMA buffers, data is compressed on the fly.
*********************************************************************/

        int b_in_p;         /* Byte_IN_Packet index. This var tracks the pos.  */
                            /* in the packet, in lumps of NET_BUFFER_SIZE.     */
        int j=0;            /* counter for moving the bytes/words of chip      */
                            /* within a buffer lump.                           */
        U16 data=0 ;        /* tmp var for 16 bits of data, to be placed in dp */
        int block;          /* Number of bytes to go into a given buffer - can */
                            /* go negative on boundary conditions.             */
        char *dp;
	U16 *psource;            /* pointer for use if mode = DMA (1) */

        b_in_p = start;
	psource = (U16 *) source;
        do {
            dp = (char *)&buff->data[buff->offset];
            block = buff->size - buff->offset; /* This much space in this buffer */
            if ((b_in_p + block) > end ) {    /* We're in last buffer */
                block = end  - b_in_p  +1;       /* so only do the remaining bytes */
            } /* endif */
            buff->len = block;    /* This is how much we're going to put in this buffer */
#if 1
            if ( b_in_p&1 ) {       /* Odd start, do single byte read   */
               block --;
		if (mode) {
			*dp++ = data>>8;
		}
		else *dp++ = SMC_READ_B_REG(source,SMC_DATA_LOWb);
            }
#endif
            for (j=1; j < (block) ;j+=2 ) {
                if(mode) {
			data = *psource++;psource ++;
		}
		else data = SMC_READ_REG(source,SMC_DATA);
                *dp++ = (char) data & 0xff;
                *dp++ = (char) ((data &0xff00) >> 8);
            } /* endfor j */

            if ((block & 1)) {                  /* is there a trailing byte? */
                if(mode) {
			data = *psource++;psource++;
			*dp++ = data & 0xff;
		}
		else data = *dp++ = SMC_READ_B_REG(source,SMC_DATA_LOWb);
            }
            b_in_p += buff->len;
            buff = buff->next;
        } while ( (b_in_p <= end) && buff ); /* enddo */

	return 0;
} /* SMC_Load_Net_Buffer */
