#ifndef smc_dma_defs
#define smc_dma_defs
/**********************************************************************/
/*File:           SMC_DMA.h                                           */
/*Author:         Mark C van der Pol                                  */
/*Organisation:   Technology Group, Time Warner Interactive.          */
/*Contents:       Data definitions for the SMC DMA Module.            */
/*                                                                    */
/*(C) Copyright Atari Games Corporation 1996                          */
/*                                                                    */
/**********************************************************************/

#ifndef __U32_TYPE_DEFINED
#include <config.h>
#endif

typedef struct {
	int	:2;
	int	SrcDir:2;		/* Source direction. 						*/
#	 define	DMA_INC	  0		/* 00 -  Increment the source address				*/
#	 define	DMA_DEC	  1		/* 01 -  Decrement the source address				*/
#	 define	DMA_HOLD  2		/* 10 -  Hold the same value					*/
	int	DestDir:2;		/* Destination direction.
					   00 - Increment the destination address
					   01 - Decrement the destination address
					   10 - Hold the same value					*/
	int	DatTransLim:3;		/* Data Transfer Limit in each DMA access			*/
#	 define	DMA_4BYTES  0		/* 000 - 4 Bytes						*/
#	 define	DMA_8BYTES  1		/* 001 - 8 Bytes						*/
#	 define	DMA_16BYTES 3		/* 011 - 16 Bytes						*/
#	 define	DMA_32BYTES 7		/* 111 - 32 Bytes						*/
	int	ChainMod:1;		/* Chained Mode.						*/
#	 define	DMA_CHAINED  0		/*  0 - Chained Mode; when DMA access is terminated,
					   the parameters of the next DMA access will come
					   from a record in memory that a NextRecPtr register 
					   points at.							*/
#	 define DMA_NOCHAIN  1		/* 1 - Non-Chained mode; only the values that are pro-
					   grammed by the CPU (or PCI) directly into the ByteCt,
					   SrcAdd, and DestAdd register are used.			*/

	int	IntMode:1;		/* Interrupt Mode.						*/
#	 define	DMA_INT_BY_BLOCK  0	/* 0 - Interrupt asserted every time the DMA byte count
					   reaches terminal count.					*/
#	 define	DMA_INT_WHEN_DONE 1	/* 1 - Interrupt every NULL pointer (in Chained mode)		*/
	int	TransMod:1;		/* Transfer Mode.						*/
#	 define	DMA_ON_DEMAND  0	/* 0 - Demand : Tranfer is initiated by a device asserting on 
						of DMAReq[3:0] 						*/
#	 define DMA_BY_REQ   1		/* 1 - Block : Tranfer is started by CPU or PCI setting bit in 
						control register.					*/
	int	ChanEn:1;		/* channel Enable.						*/
#	 define	DMA_CHAN_OFF 0		/* 0 - Disable							*/
#	 define DMA_CHAN_ON  1		/* 1 - Enable							*/
	int	FetNextRec:1;		/* Fetch Next Record.
					   1 - Forces a fetch of the next record (even if the cur-
					   rent DMA has not ended). This bit is reset after fetch
					   is completed (meaningful only in Chained mode).		*/
	int	DMAActSt:1;		/* DMA Acticity Status (read only).
					   0 - Channel is not active
					   1 - Channel is active					*/
} Galileo_DMA_Control;

#define PHYS(x) ((U32)(x)&0x1fffFFFF)
#define NO_CACHE(anon) ((void *)(((U32)anon) | 0x20000000))	/* Force the address to be un-cached */

typedef struct GALILEO_DMA {
	U32	ByteCt;			/* The number of bytes that are left in the DMA transfers      */
					/* Only 16 bits are significant = 64k max                      */
	U32	SrcAdd;			/* The address that the DMA controller will read the data from */
	U32	DestAdd;		/* The address that the DMA controller will write the data to  */
	struct GALILEO_DMA *NextRecPtr;	/* The address for the nest record of DMA. A value of 0        */
					/* means a NULL pointer (end of the chained list)              */
	union {
		Galileo_DMA_Control dma_bits;/* DMA Control register value.                                 */
		U32		    dma_word;
	} Control;
} Galileo_DMA;

extern Galileo_DMA dma_req_uncached;
/*
	This structure is used as a holding place for the data moving
	to/from the SMC ship through the Galileo DMA channels. The 
	Galileo has brain damage as far as 16 bit wide devices go, and 
	moves 32 bits regardless of the actual device. Hence when
	DMA'ing from the SMC, every other word is garbage; when DMA'ing
	to the SMC, the data has to be 'spread out' to be 16 bits of
	valid data in the lower 2 bytes of a 32 bit word.
	Also, as the Galileo has no way of understanding the Net_Buffer
	things, data must be made contiguous before it is passed to the
	DMA engine.
	The data address passed to the Galileo will have to transformed
	into a physical address, or the poor thing won't be able to find
	a thing...
	And finally, do make sure that the contents have cleared the
	cache before Galileo gets it.
*/
typedef U8 DMA_Buffer[MAX_ENET_PACKET_SIZE << 1 ];

typedef struct SMC_DMA_REQ {
	struct SMC_DMA_REQ* next;	/* Pointer to the next SMC packet movement request.  */
	struct SMC_DMA_REQ** head;	/* Pointer to the head pointer.  */
	Galileo_DMA dma_req;		/* Galileo format REQ - can be loaded directly into it */
					/* Also a record of the current DMA request in progressi */
	Net_Buffer*  net_buffer;	/* Pointer to first buffer in chain if this is a Tx Req */
					/* or a pointer to the buffer returned by the app for RX */
	U8          req_type;		/* SMC_DMA_RX or SMC_DMA_TX */
#	define	SMC_DMA_RX	1
#	define  SMC_DMA_TX	2	/* TX req. - a buffer already allocated on chip */
#	define  SMC_DMA_ALLOC	3	/* Allocation of buffer required */
	U8          invoked;		/* inc'd every time Galileo is programmed with this request */
	U16         used;		/* Inc'd every time taken of a queue. */
	U16	    pkt_size;		/* The size of the packet. This number of bytes are what */
					/* is needed on the chip(Tx), or what we want(Rx)  	 */
	U16	    pkt_offset;		/* 0 for Tx, first byte we want for Rx.			 */
	packet_number pkt_num;		/* The driver/SMC allocated packet number for this xfer  */
	U32	    pad1[8];		/* Make sure that there is no accesses to this area, as  */
					/* the dma_buffer is access *uncached*, which confuses   */
					/* the data cache on the chip at cache line ends. Talk   */
					/* to Dave Shepperd or other MIPS guru.			 */
	DMA_Buffer  dma_buffer;		/* The expanded data buffer, ignore every other word...  */
	U32	    pad2[8];		/* More padding, same reason.				 */
} SMC_DMA_Req;
	
/* smc_dma.c */
SMC_DMA_Req *DMA_Dequeue(SMC_DMA_Req **dma_queue) ;
U32 DMA_Enqueue(SMC_DMA_Req **dma_queue, SMC_DMA_Req *q_this_one);

typedef struct {
	U32 freed;
	U32 got;
	U32 max_free;
	S32 free_size;
	U32 queued;
	U32 de_queued;
	U32 max_q;
	S32 q_size;
	U32 tx_queued;
	U32 tx_de_queued;
	U32 max_tx_q;
	S32 tx_q_size;
	U32 loads;
	U32 unloads;
	U32 bad_unloads;
	U32 re_issues;
	U32 bad_req;
	U32 suspends;
	U32 resumes;
	U32 blocks;
	U32 unblocks;
	U32 blockeds;
	U32 bad_inits;
	U32 bad_allocs;
	U32 bad_banks;
	U32 bad_tx_pnrs;
	U32 bad_crc;
	U32 ints;
	U32 busy;
	U32 actr;
	U32 reqs;
	U32 vfys;
	U32 nxtr;
	U32 smcq;
} DMA_stats;

#if DMA_STATS
extern DMA_stats dma_stats;
#endif /* DMA_STATS */

extern SMC_DMA_Req  *smc_dma_free;		/* pointer to a queue of free request blks  */
extern SMC_DMA_Req  *smc_dma_queue;		/* pointer to queue of pending requests     */
extern SMC_DMA_Req  *smc_dma_tx_queue;		/* pointer to current outstanding ALLOC request queue */
extern SMC_DMA_Req  *smc_dma_req;		/* pointer to current outstanding request (NOT on any queue)*/

#endif
