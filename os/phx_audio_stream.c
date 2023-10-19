#include <config.h>
#include <stdarg.h>
#include <os_proto.h>
#include <intvecs.h>
#include <phx_audio_proto.h>
#include <phx_audio_internal.h>
/***************************************************************************/
/***************************************************************************/
#if defined(SOUND_DCS_STREAMED_AUDIO)
/***************************************************************************/
#define audPutWord aud_put
#define audGetWord aud_get
int aud_put(int code);
int aud_get(U16 *data);
/***************************************************************************/
/***************************************************************************/
#define NULL 0
/***************************************************************************/
#define ADI_STEAM_BLOCK_SIZE 480
/***************************************************************************/
#define ADI_FIFO_SIZE (1<<9)
#define ADI_FIFO_MASK ((ADI_FIFO_SIZE/2)-1)
#define ADI_STREAM_NUMBER 1
/***************************************************************************/
#define ADI_STREAM_OFF (1)
#define ADI_STREAM_ON (2)
#define ADI_STREAM_STARTING  (3)
#define ADI_STREAM_STOPPING  (4)
/***************************************************************************/
struct adiStreamInstance
	{
	struct act_q paq;
	U16 *buff[2];
	U32 size[2];
	int state;

	U16 *curBuff;
	U32 curPosition;
	S32 curSize;
	U32 position;
	int buffNum;

	U16 *emptyBuff;
	U32 emptySize;
	void (*callback)(void *cookie,U16 *buffer,U32 size);
	};
/***************************************************************************/
struct adiStreamInstance asi[ADI_STREAM_NUMBER];
/***************************************************************************/
int _adiMonoPartialFifoXfer(U16 *buff,int size);
int _adiMonoFifoXfer(U16 *buff);
int _adiStereoPartialFifoXfer(U16 *buff,int size);
int _adiStereoFifoXfer(U16 *buff);
int _adiMonoLatchXfer(U16 *buff);
int _adiStreamXfer(struct adiStreamInstance *asi);
void _adiStreamCallbackWrapper(void *cookie);
/***************************************************************************/
/***************************************************************************/
volatile U16 adi_DCS_EMPTY_BLOCKS;
volatile U16 adi_DCS_FULL_BLOCKS; 
volatile U16 adi_STREAM_ON;

/***************************************************************************/
/***************************************************************************/
void _adiSetIrqCtlBit(U16 bit)
{
IO_MAIN_CTL_T = IO_MAIN_CTL_T | (bit);
}
/***************************************************************************/
void _adiClearIrqCtlBit(U16 bit)
{
IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(bit);
}
/***************************************************************************/
void _adi_fifo_irq_action(void *whenRemoved);
/******************************/
struct act_q paqFifoIRQ;
/******************************/
void _adi_fifo_irq_action(void *whenRemoved)
{
void _adiStreamer(void);
int old_ipl;

_adiStreamer();

old_ipl = prc_set_ipl( INTS_OFF );
if (adi_DCS_EMPTY_BLOCKS) IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_FIFO_EMPTY); /* enable IRQ */
prc_set_ipl( old_ipl );
}
/******************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
void _adiStreamInit(void)
{
U16 x;

paqFifoIRQ.next=NULL;
paqFifoIRQ.que=NULL;
paqFifoIRQ.action=_adi_fifo_irq_action;
paqFifoIRQ.param=NULL;

for (x=0;x<ADI_STREAM_NUMBER;x++)
	{
	asi[x].state=ADI_STREAM_OFF;
	asi[x].paq.action=_adiStreamCallbackWrapper;
	}

x = SOUND_CONTROL_REGISTER;
x = x & (~((1<<1)+(1<<2)));	  /* hold in reset */
SOUND_CONTROL_REGISTER = x;

adi_DCS_EMPTY_BLOCKS=0; 
adi_DCS_FULL_BLOCKS=0;
adi_STREAM_ON=0;

prc_delay(1);

x = SOUND_CONTROL_REGISTER;
x = x | (((1<<1)+(1<<2)));	  /* fifo is released and enabled */
SOUND_CONTROL_REGISTER = x;

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY|IO_MAIN_FIFO_HALF|IO_MAIN_FIFO_FULL);

}
/***************************************************************************/
void _adiStreamUninit(void)
{
U16 x;

x = SOUND_CONTROL_REGISTER;
x = x & (~((1<<1)+(1<<2)));	  /* hold in reset */
SOUND_CONTROL_REGISTER = x;

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY|IO_MAIN_FIFO_HALF|IO_MAIN_FIFO_FULL);
prc_set_vec(SND_INTVEC, NULL);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int adiSetStreamVolume(S16 volume)	
{
U16 data[2];

ADI_ASSERT(volume<256);

data[0]=0x55B3;
data[1]=volume;

if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(1);
return(NULL);
}
/***************************************************************************/
/***************************************************************************/
void *adiStartStream(U16 *buff1,U32 size1,U16 *buff2,U32 size2,void (*StreamCallback)(void *cookie,U16 *buffer,U32 size))
{
int x;
ADI_ASSERT(size1);
ADI_ASSERT(buff1);

#if (0)
for (x=0;x<ADI_STREAM_NUMBER;x++)
	{
	if (asi[x].state==ADI_STREAM_OFF) break;
	}
if (x>=ADI_STREAM_NUMBER) return(0);
#else
x=0;
if (asi[x].state!=ADI_STREAM_OFF) return(NULL);
#endif
adi_STREAM_ON=0;

asi[x].buff[0]=buff1;
asi[x].buff[1]=buff2;
asi[x].size[0]=size1;
asi[x].size[1]=size2;
asi[x].callback=StreamCallback;

asi[x].buffNum=0;
asi[x].position=0;
asi[x].curPosition=0;
asi[x].curBuff=asi[x].buff[asi[x].buffNum];
asi[x].curSize=asi[x].size[asi[x].buffNum];

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY); /* turn off irq (on after blks!=0) */
asi[x].state=ADI_STREAM_STARTING;
adi_DCS_EMPTY_BLOCKS=0;
adi_STREAM_ON=1;

return((void *)&asi[x]);
}
/***************************************************************************/
void * adiStopStream(void *cookie)
{
struct adiStreamInstance *asi;

if (!((int)cookie)) return(0);
asi= (struct adiStreamInstance *) cookie;

if (asi->state!=ADI_STREAM_ON) return(0);

asi->state=ADI_STREAM_STOPPING;
adi_DCS_EMPTY_BLOCKS=0;

return(cookie);
}
/***************************************************************************/
void * adiStreamEOF(void *cookie)
{
int block;
struct adiStreamInstance *asi;

if (!((int)cookie)) return(0);
asi= (struct adiStreamInstance *) cookie;

block = (asi->buffNum-1) & 1;

asi->buff[block]=NULL;
asi->size[block]=NULL;
return(cookie);
}
/***************************************************************************/
void * adiChangeStream(void *cookie,U16 *buff1,U32 size1,U16 *buff2,U32 size2)
{
int old_ipl;
int x;
struct adiStreamInstance *asi;

if (!((int)cookie)) return(0);
asi= (struct adiStreamInstance *) cookie;

old_ipl = prc_set_ipl( INTS_OFF );
x=asi->buffNum;

x=(x+1)&1;
asi->buff[x]=buff1;
asi->size[x]=size1;
asi->curSize=0;
prc_set_ipl( old_ipl );

x=(x+1)&1;
asi->buff[x]=buff2;
asi->size[x]=size2;
return(cookie);
}
/***************************************************************************/
int adiStreamState(void * cookie)
{
struct adiStreamInstance *asi;

if (!((int)cookie)) return(-1);
asi= (struct adiStreamInstance *) cookie;

return(asi->state);
}
/***************************************************************************/
void *adiStreamBlock(void *cookie,U16 *buff,U32 size)
{
struct adiStreamInstance *asi;
int block;

if (!((int)cookie)) return(0);
asi= (struct adiStreamInstance *) cookie;

block = (asi->buffNum-1) & 1;

asi->buff[block] = buff;
asi->size[block] = size;
return(cookie);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
void _adiStreamCallbackWrapper(void *cookie)
{
struct adiStreamInstance *asi;
ADI_ASSERT(((int)cookie));
asi= (struct adiStreamInstance *) cookie;

if (asi->callback) 
	asi->callback(cookie,asi->emptyBuff,asi->emptySize);
asi->emptyBuff=NULL;
asi->emptySize=NULL;
}
/***************************************************************************/
void _adiStreamer(void)
{
U16 x;

x=0;
	{
	switch (asi[x].state)
		{
		case ADI_STREAM_OFF:
			adi_DCS_EMPTY_BLOCKS=0;
			adi_STREAM_ON=0;
			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY);
			return;
		break;

		case ADI_STREAM_STARTING:
			if (adi_DCS_EMPTY_BLOCKS>1)
				{
				_adiStreamXfer(&asi[x]);
				}
			else
				{
				U16 data[1];
				data[0]=0x55d6;
				if (adiQueueCommand(data,1,0,NULL,NULL,NULL,NULL,NULL))
					{
					asi[x].state=ADI_STREAM_ON;
					}
				}
		break;

	   	case ADI_STREAM_ON:
			if (adi_DCS_EMPTY_BLOCKS)
				{
				_adiStreamXfer(&asi[x]);
 				}
		break;

		case ADI_STREAM_STOPPING:
			{
			U16 data[2];
			data[0]=0x55d7;
			data[1]=0x55d8;
			if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL))
				{
				adi_STREAM_ON=0;
				adi_DCS_EMPTY_BLOCKS=0;
				IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY);

				asi[x].state=ADI_STREAM_OFF;
				}
			}
		break;
		}
	}
}
/***************************************************************************/
int _adiStreamXfer(struct adiStreamInstance *asi)
{
int size;
int y;
int xxx;

if (asi->curPosition>=asi->curSize)	
	{
	/* go to next big block */
	xxx=asi->buffNum;
	asi->emptyBuff=asi->buff[xxx];
	asi->emptySize=asi->size[xxx];
	asi->buff[xxx]=NULL;
	asi->size[xxx]=NULL;

	asi->paq.param=(void *)asi; 
	prc_q_action((struct act_q *)&asi->paq);  /* call callback (eventually) */

	asi->buffNum = (asi->buffNum+1) & 1;
	asi->curBuff=asi->buff[asi->buffNum];
	asi->curSize=asi->size[asi->buffNum];
	asi->curPosition=0;

	if (asi->curBuff==NULL)	{asi->state=ADI_STREAM_STOPPING;return(0);}
	}

y=asi->curSize - asi->curPosition; 

if ( y >= (ADI_STEAM_BLOCK_SIZE/2) )
	{
	/* xfer complete block */
	size=_adiMonoFifoXfer(&asi->curBuff[asi->curPosition]);
	adi_DCS_EMPTY_BLOCKS--;
	adi_DCS_FULL_BLOCKS++;
	}
else
	{
	
	/* xfer first part of block */
	if(!(size =_adiMonoPartialFifoXfer(&asi->curBuff[asi->curPosition],y)))	return(0);

	asi->position += size;

	/* go to next big block */
	xxx=asi->buffNum;
	asi->emptyBuff=asi->buff[xxx];
	asi->emptySize=asi->size[xxx];
	asi->buff[xxx]=NULL;
	asi->size[xxx]=NULL;

	asi->paq.param=(void *)asi; 
	prc_q_action((struct act_q *)&asi->paq);  /* call callback (eventually) */

	asi->buffNum = (xxx+1) & 1;
	asi->curBuff = asi->buff[asi->buffNum];
	asi->curSize = asi->size[asi->buffNum];
	asi->curPosition = 0;

	if (asi->curBuff==NULL)
		{
		asi->state=ADI_STREAM_STOPPING;
		adi_DCS_EMPTY_BLOCKS--;
		adi_DCS_FULL_BLOCKS++;
		return(size);
		}

	/* xfer second part of block */
	size=_adiMonoPartialFifoXfer(&asi->curBuff[asi->curPosition],(ADI_STEAM_BLOCK_SIZE/2)-y);
	adi_DCS_EMPTY_BLOCKS--;
	adi_DCS_FULL_BLOCKS++;
	}

if (size>(ADI_STEAM_BLOCK_SIZE/2)) while(1==1) ;
asi->curPosition += size;
asi->position += size;

if (asi->curPosition>=asi->curSize)	
	{
	/* go to next big block */
	xxx=asi->buffNum;
	asi->emptyBuff=asi->buff[xxx];
	asi->emptySize=asi->size[xxx];
	asi->buff[xxx]=NULL;
	asi->size[xxx]=NULL;

	asi->paq.param=(void *)asi; 
	prc_q_action((struct act_q *)&asi->paq);  /* call callback (eventually) */

	asi->buffNum = (asi->buffNum+1) & 1;
	asi->curBuff=asi->buff[asi->buffNum];
	asi->curSize=asi->size[asi->buffNum];
	asi->curPosition=0;

	if (asi->curBuff==NULL)	{asi->state=ADI_STREAM_STOPPING;return(0);}
	}
return(ADI_STEAM_BLOCK_SIZE/2);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
#define SOUND_FIFO (*(VU16*)(AUDIO_FIFO))
/***************************************************************************/
int _adiStereoFifoXfer(U16 *buff)
{
int x;

for (x=0;x<(ADI_STEAM_BLOCK_SIZE);)
	{
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	}

return(x);
}
/***************************************************************************/
int _adiStereoPartialFifoXfer(U16 *buff,int size)
{
int x;

for (x=0;x<size;)
	{
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	}

return(x);
}
/***************************************************************************/
int _adiMonoFifoXfer(U16 *buff)
{
int x;

for (x=0;x<(ADI_STEAM_BLOCK_SIZE/2);)
	{
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
	SOUND_FIFO=buff[x++];
/*	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];

	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++];  */
	}

return(x);
}
/***************************************************************************/
int _adiMonoPartialFifoXfer(U16 *buff,int size)
{
int x;

for (x=0;x<size;)
	{
	SOUND_FIFO=buff[x++]; 
/*	SOUND_FIFO=buff[x];
	SOUND_FIFO=buff[x++]; */
	}

return(x);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
#endif
/***************************************************************************/
/***************************************************************************/
