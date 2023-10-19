/***************************************************************************/
/*
 *	aud_queue_interface.c
 *
 *		Copyright 1996/1997 Atari Games.			  
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 *
 */ 
/***************************************************************************/
#define ADI_VERSION        0x0101
/* Atari DCS Interface (ADI) */

#include <config.h>
#include <os_proto.h>
#include <eer_defs.h>
#include <intvecs.h>
#include <phx_audio_internal.h>
#include <phx_audio_proto.h>
/***************************************************************************/
#if !defined(SOUND_DCS_CHANNELS)
	#define SOUND_DCS_CHANNELS 6
#endif

#if !defined(SOUND_QUEUE_TIMER_TIME)
	#define SOUND_QUEUE_TIMER_TIME 4000 
#endif
/***************************************************************************/
	/* ADI_QUEUE_SIZE must be a power of 2 */
#if !defined(SOUND_QUEUE_SIZE)
	#define SOUND_QUEUE_SIZE (1<<8)
#endif
	#define SOUND_QUEUE_MASK (SOUND_QUEUE_SIZE-1)
/***************************************************************************/
struct audio_stats *aud_stat;
struct audio_stats2 *aud_stat2;
struct audio_stats2 *aud_stat2b;
struct audio_stats3 *aud_stat3;
int aud_stat_touched;
/***************************************************************************/
#define ADI_LOG_ERROR(x,y) if (!(x)) {if (aud_stat->error[y]<255) {aud_stat->error[y]++;aud_stat_touched++;}}
/***************************************************************************/
#define DCS_RESERVE_TRACK   (0x8000)
#define DCS_CHECK_PRIORITY  (0x4000)
#define ADI_XFER_HEAD  1
#define ADI_XFER_PUT   2
#define ADI_XFER_GET   3
#define ADI_XFER_TAIL  4
#define ADI_XFER_SYNC1  5
#define ADI_XFER_SYNC2  6
#define ADI_XFER_SYNC3  7
#define ADI_XFER_GET_MT_BLOCKS	8
#define ADI_XFER_RESYNC  9
#define ADI_XFER_GET_STREAM_RETVAL 10

#define ADI_XFER_STREAM_RESYNC 11
/***************************************************************************/
/***************************************************************************/
typedef struct adi_track_info
	{
	U32 cookie;
	U8 priority;
    U8 sound;
    } ADI_TRACK_INFO;
/***************************************************************************/
static int ADI_FATAL_ERROR;
/***************************************************************************/
static ADI_TRACK_INFO _adi_track_table[SOUND_DCS_CHANNELS];
static U16 ADI_DCS_COOKIE;
static U16 ADI_TRACK_RESERVE;
static U16 DCS_SOFTWARE_VERSION=0xDEAD;
/***************************************************************************/
static void (*ADI_SIGNAL_CALLBACK)(U16 signal,U16 data,U32 user,U16 user2,U16 user3);
/***************************************************************************/
U16 adi_LatchQueue[SOUND_QUEUE_SIZE];
volatile U16 adi_LatchIndex;
volatile U16 adi_LatchOutIndex;
volatile int adi_QueueDisable;  /* if nonzero, don't send queued data */
volatile int adi_XferState;
/***************************************************************************/
struct adi_call_parm 
	{
	struct act_q aque;
	U32 user;
	U16 user2;
	U16 user3;
	U16 data;
	U16 signal;
	};
#define adi_call_parm_size (1<<4)
#define adi_call_parm_mask (adi_call_parm_size-1)
struct adi_call_parm adi_callback_parms[adi_call_parm_size];
U32 adi_callback_parms_idx;
/***************************************************************************/
int _adiGetLatch(U16 *data); 
int _adiPutLatch(U16 data);
int _adiTryPutLatch(U16 data);
void _adiAudioFatalError(int err);
U16 _adiGetNextCookie(void);
int _adiTrackMaskToValue(U16 mask);
U16 _adiSoundToTrackMask(U16 snd); 
U16 _adiCookieToTrackMask(U32 cookie); 
U16 _adiPriorityToTrackMask(U16 Priority); 
void adiXferQueue(void *timer);
void adiMainlineXferQueue(void);
/***************************************************************************/
struct tq adi_time_queue;
/***************************************************************************/
U16 errorLog[8];
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int _adiGetLatch(U32 *data)                                   */
/*                                                                         */
/*           Reads a single 16-bit word from the latch.                    */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/* note: This function polls a bit to determine when the port is ready,    */
/*       this is a horible waste of time in a preemtive multitasking system*/
/*       although this is probably the fastest way.                        */
/***************************************************************************/
int _adiGetLatch(U16 *data) 
{
U32 i;

i=eer_rtc;
while (!(SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) 
    	if (eer_rtc-i>2) {return(ERROR);}

*data = (SOUND_PORT_GET_DATA & 0x0000FFFF);
/*SOUND_PORT_GET_DATA = 0x0000;*/ /* this statment should only be nessecarry for a c30 host */
return(OK);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int _adiPutLatch(U32 data)                                    */
/*                                                                         */
/*           Sends a single 16-bit word to the DSP.                        */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/* note: This function polls a bit to determine when the port is ready,    */
/*       this is a horible waste of time in a preemtive multitasking system*/
/***************************************************************************/
int _adiPutLatch(U16 data)
{
int i;

i=eer_rtc;
while (!(SOUND_PORT_FLAGS & SOUND_PORT_SEND_READY)) if (eer_rtc-i>2) {return(ERROR);}

SOUND_PORT_PUT_DATA = data;
return(OK);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int _adiTryPutLatch(U32 data)                                 */
/*                                                                         */
/*           Sends a single 16-bit word to the DSP, if latch is free       */
/*                                                                         */
/* RETURNS:  OK if data sent                                               */
/*           ERROR if no send                                              */
/*                                                                         */
/***************************************************************************/
int _adiTryPutLatch(U16 data)
{
if (!(SOUND_PORT_FLAGS & SOUND_PORT_SEND_READY)) return(ERROR);

SOUND_PORT_PUT_DATA = data;
return(OK);
}
/***************************************************************************/
void _adiAudioFatalError(int err)
{
ADI_FATAL_ERROR=err;
}
/***************************************************************************/
U16 _adiGetNextCookie(void)
{
ADI_DCS_COOKIE++;
if (ADI_DCS_COOKIE==0) ADI_DCS_COOKIE++;
return(ADI_DCS_COOKIE);
}
/***************************************************************************/
int _adiTrackMaskToValue(U16 mask)
{
int x;
int track;

track=0;
for (x=0;x<SOUND_DCS_CHANNELS && (!(mask&1));x++)
	{
	mask = mask >> 1;
	track++;
	}
return(track);
}
/***************************************************************************/
U16 _adiSoundToTrackMask(U16 snd) 
{
int x;
U16 mask;

mask=0;
for (x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	if (_adi_track_table[x].sound==snd)	mask=mask+(1<<(x+8));
	}
return (mask);
}
/***************************************************************************/
U16 _adiCookieToTrackMask(U32 cookie) 
{
int x;
U16 mask;

mask=0;
for (x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	if (_adi_track_table[x].cookie==cookie)	mask=mask+(1<<(x+8));
	}
return (mask);
}
/***************************************************************************/
U16 _adiPriorityToTrackMask(U16 Priority) 
{
int x;
U16 mask;

mask=0;
for (x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	if (_adi_track_table[x].priority>=Priority)	mask=mask+(1<<(x+8));
	}
return (mask);
}
/***************************************************************************/
U16 _adiRangePriorityToTrackMask(U16 Priority,U16 start,U16 end) 
{
int x;
U16 mask;

mask=0;
for (x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	if (_adi_track_table[x].priority>=Priority
		&& _adi_track_table[x].sound>=start
		&& _adi_track_table[x].sound<=end)	mask=mask+(1<<(x+8));
	}
return (mask);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int adiStopAllSounds( void ) 
{
int _adiStopAllSoundsCallback(U16 index);
U16 data[3];

data[0]=0;
data[1]=0;
data[2]=0; 
if (adiQueueCommand(data,3,0,NULL,(U32)_adiStopAllSoundsCallback,NULL,NULL,NULL)) return(1);
else return(NULL);
}
/*******************************/
int _adiStopAllSoundsCallback(U16 index)
{
int x;

for (x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	_adi_track_table[x].sound=NULL;
	_adi_track_table[x].cookie=NULL;
	_adi_track_table[x].priority=NULL;
	}
return(0); 
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
int adiReserveTrack(U16 track)
{
U16 data[2];
ADI_ASSERT(track<SOUND_DCS_CHANNELS);

ADI_TRACK_RESERVE = (ADI_TRACK_RESERVE | ((1<<track)<<8));

data[0]=0x55AF;
data[1]=ADI_TRACK_RESERVE;
if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(ADI_TRACK_RESERVE);
else return(NULL);
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
int adiUnreserveTrack(U16 track)
{
U16 data[2];
ADI_ASSERT(track<SOUND_DCS_CHANNELS);

ADI_TRACK_RESERVE = (ADI_TRACK_RESERVE & (~((1<<track)<<8)));

data[0]=0x55AF;
data[1]=ADI_TRACK_RESERVE;
if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(ADI_TRACK_RESERVE);
else return(NULL);
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
int adiSetTrackPriority( U16 track, U16 priority ) 
{
int _adiSetTrackPriorityCallback(U16 index);
U16 data[2];
ADI_ASSERT(track<SOUND_DCS_CHANNELS);
ADI_ASSERT(priority<128);

data[0]=0x55AD;
data[1]=priority | ((1<<track)<<8);
if (adiQueueCommand(data,2,0,NULL,(U32)_adiSetTrackPriorityCallback,track,priority,NULL)) return(1);
else return(NULL);
}
/*******************************/
int _adiSetTrackPriorityCallback(U16 index)
{
_adi_track_table[adi_LatchQueue[(index+7)&SOUND_QUEUE_MASK]].priority= adi_LatchQueue[(index+8)&SOUND_QUEUE_MASK];
return(0); /* end of command */
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
int adiStopTrack(U16 track) 
{
int _adiStopTrackCallback(U16 index);
U16 data[6];
ADI_ASSERT(track<SOUND_DCS_CHANNELS);

data[0]=0x55AF;
data[1]=0;

data[2]=0x55AE;
data[3]=((1<<track)<<8);
data[4]=0x55AF;
data[5]=ADI_TRACK_RESERVE;

if (adiQueueCommand(data,6,0,NULL,(U32)_adiStopTrackCallback,track,NULL,NULL)) return(1);
else return(NULL);
}
/*******************************/
int _adiStopTrackCallback(U16 index)
{
int track;

track=adi_LatchQueue[(index+11)&SOUND_QUEUE_MASK];
_adi_track_table[track].cookie=0;
_adi_track_table[track].sound=0;
_adi_track_table[track].priority=0;
return(0); /* end of command */
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
U16 adiPlayReservedSound(U16 SndNum,U8 volume,U8 pan,U8 priority,U16 track)
{
int _adiPlaySoundCallback(U16 index);
U16 cookie;
U16 data[3];

#if !defined(SOUND_DCS_STREAMED_AUDIO)
if (SndNum>=adiNumberOfSounds()) return(NULL);  
#endif
cookie=_adiGetNextCookie();

data[0]=SndNum;
data[1]=(volume<<8)+pan;
data[2]=priority|((track)<<7)|DCS_RESERVE_TRACK;
if (adiQueueCommand(data,3,1,NULL,(U32)_adiPlaySoundCallback,cookie,priority,NULL)) return(cookie);
else return(NULL);
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
U16 adiPlaySound(U16 SndNum,U8 volume,U8 pan,U8 priority)
{
int _adiPlaySoundCallback(U16 index);
U16 cookie;
U16 data[3];

ADI_ASSERT(priority<128);
#if !defined(SOUND_DCS_STREAMED_AUDIO)
if (SndNum>=adiNumberOfSounds()) return(NULL);  
#endif
cookie=_adiGetNextCookie();

data[0]=SndNum;
data[1]=(volume<<8)+pan;
data[2]=priority | DCS_CHECK_PRIORITY;
if (adiQueueCommand(data,3,1,NULL,(U32)_adiPlaySoundCallback,cookie,priority,NULL)) return(cookie);
else return(NULL);
}
/*******************************/
int _adiPlaySoundCallback(U16 index)
{
U16 chan;

chan = adi_LatchQueue[(index+5)&SOUND_QUEUE_MASK] >> 8;
if( chan )
	{
	chan=_adiTrackMaskToValue(chan);
	_adi_track_table[chan].sound = adi_LatchQueue[(index+2)&SOUND_QUEUE_MASK];
	_adi_track_table[chan].priority = adi_LatchQueue[(index+10)&SOUND_QUEUE_MASK];
	_adi_track_table[chan].cookie = adi_LatchQueue[(index+9)&SOUND_QUEUE_MASK];
	}
return(0); /* end of command */
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
U16 adiSetSoundPriority(U16 cookie,U8 priority)
{
int _adiSetSoundPriorityCallback(U16 index);

ADI_ASSERT(priority<128);

if (adiQueueCommand(NULL,0,2,NULL,(U32)_adiSetSoundPriorityCallback,cookie,priority,NULL)) return(cookie);
return(NULL);
}
/*******************************/
int _adiSetSoundPriorityCallback(U16 index)
{
U16 trackMask;

trackMask=_adiCookieToTrackMask(adi_LatchQueue[(index+7)&SOUND_QUEUE_MASK]);
if (trackMask)
	{
	U16 priority;
	int x;

	priority = adi_LatchQueue[(index+8)&SOUND_QUEUE_MASK];

	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x0002;
	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AD;
	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=priority|trackMask;
	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1) & SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	/* 2 more till end */
	for(x=0;x<SOUND_DCS_CHANNELS;x++)
		{
		if (trackMask & (1<<(x+8)))	_adi_track_table[x].priority= priority;
		}
	return(1); /* insert command */
	}
return(0); /* end of command */
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
U16 adiStopSound(U16 cookie)
{
int _adiStopSoundCallback(U16 index);
if (adiQueueCommand(NULL,0,6,NULL,(U32)_adiStopSoundCallback,cookie,NULL,NULL)) return(1);
return(NULL);
}
/*******************************/
int _adiStopSoundCallback(U16 index)
{
U16 mask;

mask = _adiCookieToTrackMask(adi_LatchQueue[(index+11)&SOUND_QUEUE_MASK]);
if (mask)
	{
	int x;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x0006; /* six out zero in */

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AE;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=mask;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=ADI_TRACK_RESERVE;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	for(x=0;x<SOUND_DCS_CHANNELS;x++)
		{
		if (mask & (1<<(x+8)))	
			{
			_adi_track_table[x].cookie=0;
			_adi_track_table[x].sound=0;
			_adi_track_table[x].priority=0;
			}
		}
	return(1); /* insert a command */
	}						
return(0); /* end of command */
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
int adiStopSoundNumber(U16 SndNum)
{
int _adiStopSoundNumberCallback(U16 index);
if (adiQueueCommand(NULL,0,6,NULL,(U32)_adiStopSoundNumberCallback,SndNum,NULL,NULL)) return(1);
return(NULL);
}
/*******************************/
int _adiStopSoundNumberCallback(U16 index)
{
U16 mask;

mask = _adiSoundToTrackMask(adi_LatchQueue[(index+11)&SOUND_QUEUE_MASK]);

if (mask)
	{
	int x;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x0006; /* two out zero in */

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AE;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=mask;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=ADI_TRACK_RESERVE;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	for(x=0;x<SOUND_DCS_CHANNELS;x++)
		{
		if (mask & (1<<(x+8)))	
			{
			_adi_track_table[x].cookie=0;
			_adi_track_table[x].sound=0;
			_adi_track_table[x].priority=0;
			}
		}
	return(1); /* insert a command */
	}
return(0); /* end of command */
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
int adiStopSoundPriority( U16 priority ) 
{
int _adiStopSoundPriorityCallback(U16 index);
if (adiQueueCommand(NULL,0,6,NULL,(U32)_adiStopSoundPriorityCallback,priority,NULL,NULL)) return(1);
return(NULL);
}
/*******************************/
int _adiStopSoundPriorityCallback(U16 index)
{
U16 mask;

mask = _adiPriorityToTrackMask(adi_LatchQueue[(index+11)&SOUND_QUEUE_MASK]);

if (mask)
	{
	int x;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x0006; /* two out zero in */

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AE;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=mask;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=ADI_TRACK_RESERVE;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	for(x=0;x<SOUND_DCS_CHANNELS;x++)
		{
		if (mask & (1<<(x+8)))	
			{
			_adi_track_table[x].cookie=0;
			_adi_track_table[x].sound=0;
			_adi_track_table[x].priority=0;
			}
		}
	return(1); /* insert a command */
	}
return(0); /* end of command */
}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int adiStopSoundRangePriority(U16 start,U16 end, U16 priority ) 
{
int _adiStopSoundRangePriorityCallback(U16 index);
if (adiQueueCommand(NULL,0,6,NULL,(U32)_adiStopSoundRangePriorityCallback,priority,start,end)) return(1);
return(NULL);
}
/*******************************/
int _adiStopSoundRangePriorityCallback(U16 index)
{
U16 mask;

mask = _adiRangePriorityToTrackMask(adi_LatchQueue[(index+11)&SOUND_QUEUE_MASK],adi_LatchQueue[(index+12)&SOUND_QUEUE_MASK],adi_LatchQueue[(index+13)&SOUND_QUEUE_MASK]);

if (mask)
	{
	int x;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x0006; /* two out zero in */

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AE;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=mask;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0x55AF;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=ADI_TRACK_RESERVE;

	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;
	index = (index+1)&SOUND_QUEUE_MASK;
	adi_LatchQueue[index]=0;

	for(x=0;x<SOUND_DCS_CHANNELS;x++)
		{
		if (mask & (1<<(x+8)))	
			{
			_adi_track_table[x].cookie=0;
			_adi_track_table[x].sound=0;
			_adi_track_table[x].priority=0;
			}
		}
	return(1); /* insert a command */
	}
return(0); /* end of command */
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
U16 adiSetSoundVolume(U16 cookie,U8 volume)
{
U16 trackMask;

ADI_ASSERT(volume<256);
trackMask=_adiCookieToTrackMask(cookie);
if (trackMask)
	{
	U16 data[2];
	data[0]=0x55AB;
	data[1]=volume|trackMask;
	if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(cookie);
	}
return(NULL);
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
U16 adiSetTrackVolume(U16 track,U8 volume)
{
U16 trackMask;

ADI_ASSERT(volume<256);
ADI_ASSERT(track<SOUND_DCS_CHANNELS);
trackMask=(1<<track)<<8;
if (trackMask)
	{
	U16 data[2];
	data[0]=0x55AB;
	data[1]=volume|trackMask;
	if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(1);
	}
return(NULL);
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
U16 adiUpdatePlayerEngine(U8 volume,U8 pan,U8 pitch)
{
U16 data[3];
data[0]=0x55E0;
data[1]=volume|(pitch<<8);
data[2]=pan;
if (adiQueueCommand(data,3,0,NULL,NULL,NULL,NULL,NULL)) return(1);	

return(NULL);
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
U16 adiUpdateDroneEngine(U8 drone,U8 volume,U8 pan,U8 pitch)
{
U16 data[3];
data[0]=0x55E3+drone;
data[1]=volume|(pitch<<8);
data[2]=pan;
if (adiQueueCommand(data,3,0,NULL,NULL,NULL,NULL,NULL)) return(1);	

return(NULL);
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
U16 adiSetSoundPan(U16 cookie,U8 pan)
{
U16 trackMask;

ADI_ASSERT(pan<256);
trackMask=_adiCookieToTrackMask(cookie);
if (trackMask)
	{
	U16 data[2];
	data[0]=0x55AC;
	data[1]=pan|trackMask;
	if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(cookie);
	}
return(NULL);
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
U16 adiSetTrackPan(U8 track,U8 pan)
{
U16 trackMask;

ADI_ASSERT(pan<256);
ADI_ASSERT(track<SOUND_DCS_CHANNELS);
trackMask=(1<<track)<<8;
if (trackMask)
	{
	U16 data[2];
	data[0]=0x55AC;
	data[1]=pan|trackMask;
	if (adiQueueCommand(data,2,0,NULL,NULL,NULL,NULL,NULL)) return(1);
	}
return(NULL);
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
U16 adiOSVersion(void) 
{
return(DCS_SOFTWARE_VERSION);
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
U16 adiInterfaceVersion(void) 
{
return (ADI_VERSION);
}
/***************************************************************************/
__inline__ void adiEnable(void)  
{
int old_ipl;
old_ipl = prc_set_ipl( INTS_OFF );
adi_QueueDisable--;
prc_set_ipl( old_ipl );
}
/***************************************************************************/
__inline__ void adiDisable(void) 
{
int old_ipl;
old_ipl = prc_set_ipl( INTS_OFF );
adi_QueueDisable++;
prc_set_ipl( old_ipl );
}
/***************************************************************************/
int adiResyncQueue(void)
{
U16 xxx,yyy;

adiDisable();
ADI_LOG_ERROR(1==0,40);

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);

xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);
xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  

xxx=0;
yyy=0;

while (xxx!=0xDead+yyy && yyy<10)
	{
	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  
	if (yyy>6) _adiPutLatch(0);
	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  
	if (yyy>7) _adiPutLatch(0);
	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  
	if (yyy>8) _adiPutLatch(0);
	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  
	if (yyy>9) _adiPutLatch(0);
	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);	  

	_adiPutLatch(0x55B0);
	_adiPutLatch(0xDEAD+yyy+1);
	_adiGetLatch(&xxx); 
	yyy++;
	}

if (xxx!=(0xDEAD+yyy)) 
	{
	ADI_LOG_ERROR(1==0,41);
	adiEnable();
	return(1);
	}	
adiEnable();
return(0);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiLockQueue(void)                                       */
/*                                                                         */
/*           disallows data to be transfered from the queue to the latch.  */
/*                                                                         */
/*           NOTE: this command may transfer data from the queue to the    */
/*           latch if part of a command has already has been Xfered.       */
/*           therefore this command may not return immediatly              */
/***************************************************************************/
void adiLockQueue(void)
{
int timeout;

adiDisable();

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);

while (adi_XferState!=ADI_XFER_HEAD)
	{
	switch(adi_XferState)
		{
		case ADI_XFER_HEAD:
		case ADI_XFER_PUT:
		case ADI_XFER_TAIL:
		case ADI_XFER_SYNC1:
		case ADI_XFER_SYNC2:
		IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
		break;

		case ADI_XFER_GET:
		case ADI_XFER_SYNC3:
		case ADI_XFER_GET_MT_BLOCKS:
		IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);
		break;
		}

	timeout=eer_rtc;
	while ((adi_XferState!=ADI_XFER_HEAD)) 
		{
		if (eer_rtc-timeout<30)
			{
			; /* waitin' */
			}
		else
			{
			adiResyncQueue();

			adi_XferState=ADI_XFER_RESYNC;

			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
			IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
			return;	
			}
		}
	}
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiUnlockQueue(void)                                     */
/*                                                                         */
/*           allows data to be transfered from the queue to the latch.     */
/*                                                                         */
/***************************************************************************/
void adiUnlockQueue(void)
{
adiEnable();
ADI_ASSERT(adi_QueueDisable>=0);
if (adi_QueueDisable<0) adi_QueueDisable=0;
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiFlushQueue(void)                                      */
/*                                                                         */
/*           clears the entire queue.                                      */
/*                                                                         */
/***************************************************************************/
void adiFlushQueue(void)
{
adiLockQueue();
adi_LatchOutIndex=0;
adi_LatchIndex=0;
adiUnlockQueue();
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiRegisterCallback(void (*SignalCallback)(U16 signal	   */
/*                                ,U16 data,U32 user,U16 user2,U16 user3)) */
/*                                                                         */
/*           Returns the amount of free space on the queue.                */
/*                                                                         */
/* RETURNS:  OK if data sent                                               */
/*           ERROR if no send                                              */
/*                                                                         */
/***************************************************************************/
void adiRegisterCallback(void (*SignalCallback)(U16 signal,U16 data,U32 user,U16 user2,U16 user3))
{
ADI_SIGNAL_CALLBACK=SignalCallback;
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiSendAllQueue(void)                                    */
/*                                                                         */
/*           Sends all the data on the latch.                              */
/*                                                                         */
/*           NOTE: this command may be slow to execute                     */
/***************************************************************************/
void adiSendAllQueue(void)
{
int timeout;

adiDisable();

while (adiQueueAmountFree()!=SOUND_QUEUE_MASK)
	{
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);

	switch(adi_XferState)
		{
		case ADI_XFER_HEAD:
		case ADI_XFER_PUT:
		case ADI_XFER_TAIL:
		case ADI_XFER_SYNC1:
		case ADI_XFER_SYNC2:
		IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
		break;

		case ADI_XFER_GET:
		case ADI_XFER_SYNC3:
		case ADI_XFER_GET_MT_BLOCKS:
		IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);
		break;

		}

	timeout=eer_rtc;
	while ((adi_XferState!=ADI_XFER_HEAD) && (eer_rtc-timeout<30)) ; /* waitin' */
	}
adiEnable();
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int adiQueueAmountFree(void)                                  */
/*                                                                         */
/*           Returns the amount of free space on the queue.                */
/*                                                                         */
/* RETURNS:  OK if data sent                                              */
/*           ERROR if no send                                              */
/*                                                                         */
/***************************************************************************/
int adiQueueAmountFree(void)
{
return(((adi_LatchOutIndex+SOUND_QUEUE_MASK)-adi_LatchIndex) & SOUND_QUEUE_MASK);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int adiQueueCommand(U16 *data,U16 outsize,U16 insize		   */
/*                             ,U16 signal,U32 user,U16 user2,U16 user3,user4)   */
/*                                                                         */
/*           data                                                          */
/*           outsize                                                       */
/*           insize                                                        */
/*           signal                                                        */
/*                                                                         */
/*           Puts a sound command on the queue.                            */
/*                                                                         */
/* RETURNS:  The 1 if ok                                                   */
/*           or 0 if ERROR or not enough space available                   */
/*                                                                         */
/* note: the implimentation is a bit ugly.  It uses a unified queue for    */
/*       both incomming and outgoing data as well as control data (size).  */
/*       I can get away with this becuase of the half duplex nature of DCS.*/
/*       After the command is completed and if signal is nonzero a callback*/
/*       function gives you the results of the sound call.                 */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int adiQueueCommand(U16 *data,U16 outsize,U16 insize,U16 signal,U32 user,U16 user2,U16 user3,U16 user4)
{
U32 x;
U16 *retdat;
int old_ipl;

ADI_ASSERT(outsize<7);
ADI_ASSERT(insize<7);

if(adiQueueAmountFree() < (outsize+insize+2+2+2+2)) 
	{
	return(0);
	} /* not enough space on queue */

old_ipl = prc_set_ipl( INTS_OFF );

adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=(insize<<8)+outsize;

if (outsize)
	{
	for (x=0;x<outsize;x++)
		{
		adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
		adi_LatchQueue[adi_LatchIndex]=data[x];
		}

	retdat= &adi_LatchQueue[adi_LatchIndex];
	}

if (insize)
	for (x=0;x<insize;x++)
		{
		adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
		adi_LatchQueue[adi_LatchIndex]=0;
		}
/* or */
/*adi_LatchIndex = (adi_LatchIndex+insize) & SOUND_QUEUE_MASK;
  */

adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=signal;

adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=(U16)((user&0xFFFF0000)>>16);
adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=(U16)(user&0x0000FFFF);

adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=user2;
adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=user3;
adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=user4;

adi_LatchIndex = (adi_LatchIndex+1) & SOUND_QUEUE_MASK;
adi_LatchQueue[adi_LatchIndex]=0x1111;	/* command wall */
prc_set_ipl( old_ipl );

return(1);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiXferQueue(void)                                       */
/*                                                                         */
/*           sends or retrieves the next latch command if flags are set    */
/*                                                                         */
/*       The queue provides state information so commands can be           */
/*       inserted in between blocked memory transfers through the latch    */
/***************************************************************************/
void _adiQXfer();

void adiXferQueue(void *timer)
{
static int wcntr;

adi_time_queue.delta=SOUND_QUEUE_TIMER_TIME;
tq_ins(&adi_time_queue);

if (adi_QueueDisable) return;	
if (ADI_FATAL_ERROR) return;

if(adi_XferState==ADI_XFER_HEAD) 
	{
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
	IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
	 /* turn on IRQ, now we are waiting for DCS */
	wcntr=0;
	}
else if (wcntr<30) wcntr++; 
else 
	{
	ADI_LOG_ERROR(1==0,1);
	wcntr=0;

	while (SOUND_PORT_FLAGS & (SOUND_PORT_DATA_READY))
		{
		U16 data;

		ADI_LOG_ERROR(1==0,0);
		data = (SOUND_PORT_GET_DATA & 0x0000FFFF);
		prc_delay(1);
		}	

	adi_XferState=ADI_XFER_RESYNC;
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
	IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
	}
}
/********************************/
/********************************/
void _adiSignal(U32 idx)	
{
if (idx< (adi_call_parm_size-1)) 
	{
	if (ADI_SIGNAL_CALLBACK) 
		ADI_SIGNAL_CALLBACK(adi_callback_parms[idx].signal
							,adi_callback_parms[idx]
							.data,adi_callback_parms[idx].user
							,adi_callback_parms[idx].user2
							,adi_callback_parms[idx].user3);
	}
}
/********************************/
#if defined(SOUND_DCS_STREAMED_AUDIO)
static int scntr;
#endif
/********************************/
void LOG_ERROR_COMMAND(void)
{
int x;
U16 indx;

for(x=0;x<8;x++)
	{
	aud_stat2b->cmd4[x]=aud_stat2b->cmd3[x];
	aud_stat2b->cmd3[x]=aud_stat2b->cmd2[x];
	aud_stat2b->cmd2[x]=aud_stat2b->cmd1[x];
	aud_stat2b->cmd1[x]=aud_stat2->cmd4[x];

	aud_stat2->cmd4[x]=aud_stat2->cmd3[x];
	aud_stat2->cmd3[x]=aud_stat2->cmd2[x];
	aud_stat2->cmd2[x]=aud_stat2->cmd1[x];
	}
  
x=0;

indx=adi_LatchOutIndex;
indx = (indx+1) & SOUND_QUEUE_MASK;
indx = (indx+1) & SOUND_QUEUE_MASK;

while(adi_LatchQueue[indx]!=0x1111 && x<8)
	{
	aud_stat2->cmd1[x++]=adi_LatchQueue[indx];
	indx = (indx+1) & SOUND_QUEUE_MASK;
	}
aud_stat_touched++;
}
/********************************/
void LOG_ERROR_MULTI_COMMAND(void)
{
int x;

for(x=0;x<8;x++)
	{
	aud_stat2b->cmd4[x]=aud_stat2b->cmd3[x];
	aud_stat2b->cmd3[x]=aud_stat2b->cmd2[x];
	aud_stat2b->cmd2[x]=aud_stat2b->cmd1[x];
	aud_stat2b->cmd1[x]=aud_stat2->cmd4[x];

	aud_stat2->cmd4[x]=aud_stat2->cmd3[x];
	aud_stat2->cmd3[x]=aud_stat2->cmd2[x];
	aud_stat2->cmd2[x]=aud_stat2->cmd1[x];
	}

for (x=0;x<8;x++)
	{
	aud_stat2->cmd1[x]=errorLog[x];
	}

aud_stat_touched++;
}
/********************************/
/********************************/
void LOG_ERROR_DATA(U16 flags,U16 data)
{
int x;

for(x=14;x>=0;x--)
	{
	aud_stat3->flags[x+1]=aud_stat3->flags[x];
	aud_stat3->data[x+1]=aud_stat3->data[x];
	}
aud_stat3->flags[0]=flags;
aud_stat3->data[0]=data;

aud_stat_touched++;
}
/********************************/
void _adiQXfer()
{
/**/	/**/	/**/	/**/	/**/	/**/	/**/	/**/	/**/	/**/
static U16 index;
static U16 in_index;
static int outcnt;
static int incnt;
U16 xxx;U16 yyy;
static int instate;
 
/**/	/**/	/**/	/**/

if (ADI_FATAL_ERROR) 
	{
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
	return;
	}

if(adi_XferState==ADI_XFER_HEAD)
	{
	index=adi_LatchOutIndex;	

	if ((SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) 
		{
		xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);
		ADI_LOG_ERROR(1==0,39);
		}
	
#if defined(SOUND_DCS_STREAMED_AUDIO)
	if (adi_DCS_FULL_BLOCKS) 
		{
		/* streaming block is in the buffer, tell dcs about it */
		SOUND_PORT_PUT_DATA = 0x55de;
		adi_DCS_FULL_BLOCKS--;

 			adi_XferState=ADI_XFER_GET_STREAM_RETVAL;
			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_HTS_DATA_EMPTY);
			IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);

		return;
		}		 
	if (adi_STREAM_ON)
		{
		if (scntr>0) scntr--;
		else if(!adi_DCS_EMPTY_BLOCKS)
			{
			SOUND_PORT_PUT_DATA = 0x55d9;
 			adi_XferState=ADI_XFER_GET_MT_BLOCKS;
			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_HTS_DATA_EMPTY);
			IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);
			return;
			}
		}
	else IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY);	
#endif

if (adi_LatchIndex==adi_LatchOutIndex) 
	{
	/* nothing to do ... wait for timer to try again */
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
	return;
	} 

	index=adi_LatchOutIndex;	

	while(0x1111!=adi_LatchQueue[index])
		{
		ADI_LOG_ERROR(1==0,2);
		
		index = (index+1) & SOUND_QUEUE_MASK;
		if (adi_LatchIndex==index) 
			{
			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
			return;
			} 
		}
	
	index = (index+1) & SOUND_QUEUE_MASK;
	xxx=adi_LatchQueue[index];

	incnt =(xxx >> 8) & 0x00FF;
	outcnt= xxx & 0x00FF;

	ADI_LOG_ERROR(outcnt<8,3);
	ADI_LOG_ERROR(incnt<8,4);
	if (incnt>7 || outcnt>7) return;

	for (xxx=7;xxx>=1;xxx--) errorLog[xxx]=errorLog[xxx-1];
	errorLog[0]=adi_LatchQueue[(index+1) & SOUND_QUEUE_MASK];

	if (outcnt) {adi_XferState=ADI_XFER_PUT;}
	else
		{ 
		index=(index+incnt)&SOUND_QUEUE_MASK;
		adi_XferState=ADI_XFER_TAIL;
		}
	}
/**/	/**/	/**/	/**/
/*else*/
if(adi_XferState==ADI_XFER_PUT)
	{
	index = (index+1) & SOUND_QUEUE_MASK;
	SOUND_PORT_PUT_DATA = adi_LatchQueue[index];

	outcnt--;
	if(outcnt==0) 
		{
		if (incnt)
			{
			IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_HTS_DATA_EMPTY);
			IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);
			adi_XferState=ADI_XFER_GET; 
			in_index=(index+1) & SOUND_QUEUE_MASK;
			}
		else adi_XferState=ADI_XFER_TAIL;
		}
	}
/**/	/**/	/**/	/**/
else if(adi_XferState==ADI_XFER_GET)
	{
	xxx=SOUND_PORT_FLAGS;

	switch((xxx&0x7F00))
		{
		case 0x0100: /* play list signal */
			ADI_LOG_ERROR(1==0,7);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x0200: /* track allocation info */
		case 0x1A00: /* track allocation info */
			/* should be me */
		break;

		case 0x0300: /* checksum */
			ADI_LOG_ERROR(1==0,8);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x0400: /* synchronation */
			ADI_LOG_ERROR(1==0,9);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x0500: /* streaming handshake */
			ADI_LOG_ERROR(1==0,10);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x0600: /* track busy mask */
			ADI_LOG_ERROR(1==0,11);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x9900: /* system error */
			ADI_LOG_ERROR(1==0,12);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		case 0x8000: /* real time flag */
			ADI_LOG_ERROR(1==0,13);
			LOG_ERROR_COMMAND();
		   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			return;
		break;

		default:
			xxx=xxx&0xFF00;
			xxx=xxx|(aud_stat->num_cmds&0x00000FF);
			ADI_LOG_ERROR(1==0,14);
			LOG_ERROR_COMMAND();
		   	yyy = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
			LOG_ERROR_MULTI_COMMAND();
			LOG_ERROR_DATA(xxx,yyy);
			return;
		break;
		}

	index = (index+1) & SOUND_QUEUE_MASK;
   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);
   	adi_LatchQueue[index] =	xxx;

	incnt--;
	if(incnt==0) 
		{
		adi_XferState=ADI_XFER_TAIL;
		IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
		IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);
		}
	}
/**/	/**/	/**/	/**/
else if(adi_XferState==ADI_XFER_TAIL)
	{
	S32 x;
	U16 signal;

	index = (index+1) & SOUND_QUEUE_MASK;
	signal=adi_LatchQueue[index];

	index = (index+1) & SOUND_QUEUE_MASK;
	x=adi_LatchQueue[index]<<16;
	index = (index+1) & SOUND_QUEUE_MASK;
	x=x+adi_LatchQueue[index];

	aud_stat->num_cmds++;
	aud_stat_touched++;

	if (x && (!signal))
		{
		int (*_callback)(U16 index);
		
		_callback=(int (*)(U16))x;

		if (_callback(adi_LatchOutIndex)) 
			{
			adi_XferState=ADI_XFER_HEAD;
			return;
			}
		}

	if (signal)
		{
		adi_callback_parms_idx= (adi_callback_parms_idx+1) & adi_call_parm_mask;
		adi_callback_parms[adi_callback_parms_idx].data = adi_LatchQueue[in_index];
		adi_callback_parms[adi_callback_parms_idx].user = x;
		adi_callback_parms[adi_callback_parms_idx].signal = signal;

		index = (index+1) & SOUND_QUEUE_MASK;	/* user2 */
		adi_callback_parms[adi_callback_parms_idx].user2 = adi_LatchQueue[index];
		index = (index+1) & SOUND_QUEUE_MASK;	/* user3 */
		adi_callback_parms[adi_callback_parms_idx].user3 = adi_LatchQueue[index];
		index = (index+1) & SOUND_QUEUE_MASK;	/* user4 */

		adi_callback_parms[adi_callback_parms_idx].aque.action=_adiSignal;
		adi_callback_parms[adi_callback_parms_idx].aque.param=(void *)adi_callback_parms_idx;
		prc_q_action((struct act_q *)&adi_callback_parms[adi_callback_parms_idx]);
		} 
	else
		{
		index = (index+1) & SOUND_QUEUE_MASK;	/* user2 */
		index = (index+1) & SOUND_QUEUE_MASK;	/* user3 */
		index = (index+1) & SOUND_QUEUE_MASK;	/* user4 */
		}

	index = (index+1) & SOUND_QUEUE_MASK;
	ADI_LOG_ERROR(0x1111==adi_LatchQueue[index],15); /* sync word */


	adi_XferState=ADI_XFER_SYNC1; 
	}
else if (adi_XferState==ADI_XFER_SYNC1)
	{
	SOUND_PORT_PUT_DATA = 0x55B0;
	adi_XferState=ADI_XFER_SYNC2;
	}
else if (adi_XferState==ADI_XFER_SYNC2)
	{
	SOUND_PORT_PUT_DATA = 0xBeef;
	adi_XferState=ADI_XFER_SYNC3;
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_HTS_DATA_EMPTY);
	IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_STH_DATA_FULL);
	}
else if (adi_XferState==ADI_XFER_SYNC3)
	{
   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 

	if (xxx!=0xBeef)
		{
		ADI_LOG_ERROR(xxx==0xBEEF,16);
/*		LOG_ERROR_DATA(-1,xxx);	 */
		LOG_ERROR_MULTI_COMMAND();
		}
	if (xxx!=0x0BEEF) return;
	adi_LatchOutIndex=index;
	adi_XferState=ADI_XFER_HEAD;

	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
	/* turn off irq.... end of command... Wait for timer to wake me up */
	}
/**/	/**/	/**/	/**/
else if (adi_XferState==ADI_XFER_RESYNC)
	{
	index=adi_LatchOutIndex;
   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF); 
	adi_XferState=ADI_XFER_SYNC1;
	}
/**/	/**/	/**/	/**/
#if defined(SOUND_DCS_STREAMED_AUDIO)
else if (adi_XferState==ADI_XFER_GET_STREAM_RETVAL)
	{
   	xxx = (SOUND_PORT_GET_DATA & 0x0000FFFF);

	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
	IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);

	adi_XferState=ADI_XFER_HEAD;

	if(xxx)	
		{
		ADI_LOG_ERROR(1==0,38);
		instate=0;
		adi_XferState=ADI_XFER_STREAM_RESYNC;
		}
	}
else if (adi_XferState==ADI_XFER_GET_MT_BLOCKS)
	{
	xxx=SOUND_PORT_FLAGS;
   	adi_DCS_EMPTY_BLOCKS = (SOUND_PORT_GET_DATA & 0x0000FFFF);
	if (adi_DCS_EMPTY_BLOCKS>16)
		{
	   	ADI_LOG_ERROR(1==0,17);
	   	LOG_ERROR_DATA(xxx,adi_DCS_EMPTY_BLOCKS); 
		adi_DCS_EMPTY_BLOCKS=0;
		}
   	 
	if (adi_DCS_EMPTY_BLOCKS) IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_FIFO_EMPTY);
	else IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY);
	if (adi_DCS_EMPTY_BLOCKS<2)	scntr=(14000/SOUND_QUEUE_TIMER_TIME)+1;

	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL);
	IO_MAIN_CTL_T = IO_MAIN_CTL_T | (IO_MAIN_HTS_DATA_EMPTY);

	adi_XferState=ADI_XFER_HEAD;
	}
else if (adi_XferState==ADI_XFER_STREAM_RESYNC)
	{
	if (instate==0)
		{
		SOUND_PORT_PUT_DATA = 0x55d7; /* stop */
		instate++;
		}
	else if (instate==1)
		{
		SOUND_PORT_PUT_DATA = 0x55d8; /* flush */
		instate++;
		}
	else if (instate==2)
		{
		SOUND_PORT_PUT_DATA = 0x55d6;  /* start */
		adi_DCS_EMPTY_BLOCKS=0;
		adi_XferState=ADI_XFER_HEAD;
		}
	}
#endif
else 
	{
	ADI_LOG_ERROR(1==0,18);
	adi_XferState=ADI_XFER_RESYNC;
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
	return;
	}
return;

}
/***************************************************************************/
/***************************************************************************/
void _adi_fifo_irq_action(void *whenRemoved);
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
void _adi_irq_vector(void *whenRemoved)
{ /* called within an interupt context */
#if defined(SOUND_DCS_STREAMED_AUDIO)
extern struct act_q paqFifoIRQ;

if ((IO_MAIN_STS_T & IO_MAIN_FIFO_EMPTY) && (IO_MAIN_CTL_T & IO_MAIN_FIFO_EMPTY)) /* did I cause the IRQ? */
	{
	IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_FIFO_EMPTY); /* disable myself */
	prc_q_action(&paqFifoIRQ);  /* call callback (eventually) */
	}
#endif

if ((IO_MAIN_STS_T & IO_MAIN_STH_DATA_FULL) && (IO_MAIN_CTL_T & IO_MAIN_STH_DATA_FULL)) /* did I cause the IRQ? */
	{
	_adiQXfer();
	}
if ((IO_MAIN_STS_T & IO_MAIN_HTS_DATA_EMPTY) && (IO_MAIN_CTL_T & IO_MAIN_HTS_DATA_EMPTY)) /* did I cause the IRQ? */
	{
	_adiQXfer();
	}

}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: void adiInit(void)                                            */
/*                                                                         */
/*           Initializes adi varables.                                     */
/*                                                                         */
/***************************************************************************/
long adiInit(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int) 
{
int x;
int er;
U16 temp;

#if defined(SOUND_DCS_STREAMED_AUDIO)
scntr=0;
#endif

ADI_FATAL_ERROR=0;
#if defined(USER_REC_UREC10)
aud_stat=(struct audio_stats *)eer_user_rd(USER_REC_UREC10,NULL);
eer_user_free(USER_REC_UREC10);
aud_stat2=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC11,NULL);
eer_user_free(USER_REC_UREC11);
aud_stat2b=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC12,NULL);
eer_user_free(USER_REC_UREC12);
aud_stat3=(struct audio_stats3 *)eer_user_rd(USER_REC_UREC13,NULL);
eer_user_free(USER_REC_UREC13);
aud_stat_touched=0;
#endif

temp=SOUND_PORT_GET_DATA; /* remove extra data if any */
temp=SOUND_PORT_GET_DATA; /* remove extra data if any */
temp=SOUND_PORT_GET_DATA; /* remove extra data if any */

x=0;er=1;
while (x<5 && er!=0)
	{
	er=0;
    if (soundHardwareReset()) er++;
	temp=SOUND_PORT_GET_DATA; /* remove extra data if any */

    if (!er) if (soundLatchTest()) er++;
    if (!er) if (soundCustomSoftwareReset(comp ,osys ,dm_ext ,dm_int)) er++;
	x++;
	}
if(x>=5) {_adiAudioFatalError(1);return(0);}

prc_set_vec(SND_INTVEC,_adi_irq_vector);  /* Load the Sound Int Vector */

#if defined(SOUND_DCS_STREAMED_AUDIO)
_adiStreamInit();
#endif

adiClearAll();
adiAttractVolume();

ADI_DCS_COOKIE=1;
ADI_TRACK_RESERVE=0;
adi_LatchIndex=0;
adi_LatchOutIndex=0;
adi_LatchQueue[0]=0x1111;
adi_XferState=ADI_XFER_HEAD;

adi_QueueDisable=0;

ADI_SIGNAL_CALLBACK=0;
adi_callback_parms_idx=0;

for(x=0;x<SOUND_DCS_CHANNELS;x++)
	{
	_adi_track_table[x].cookie=NULL;
	_adi_track_table[x].priority=NULL;
	_adi_track_table[x].sound=NULL;
	}

if (_adiPutLatch(0x55D3)) {_adiAudioFatalError(2);return(0);}
if (_adiPutLatch(0x55D4)) {_adiAudioFatalError(3);return(0);}
if (_adiGetLatch(&DCS_SOFTWARE_VERSION)) {_adiAudioFatalError(4);return(0);}

adi_time_queue.next=NULL;
adi_time_queue.que=NULL;
adi_time_queue.vars=NULL;
adi_time_queue.func=adiXferQueue;
adi_time_queue.delta=SOUND_QUEUE_TIMER_TIME;

IO_MAIN_CTL_T = IO_MAIN_CTL_T & ~(IO_MAIN_STH_DATA_FULL|IO_MAIN_HTS_DATA_EMPTY);
tq_ins(&adi_time_queue); 

return(1);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
void adiOnOffSwitch(int x)
{
static int lastX=1;
U16 data[2];

if (x) x=1;

if (x==lastX) return;
else
	if (x)
		{
		data[0]=0x55aa;
		data[1]=0x40bF;
		adiQueueCommand(data,2,0,0,0,0,0,0);
		}
	else
		{
		data[0]=0x55aa;
		data[1]=0x00FF;
		adiQueueCommand(data,2,0,0,0,0,0,0);
		}
lastX=x; 
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
int adiGameVolume(void)
{
U16 vol;
U16 data[2];

vol=eer_gets(EER_AUD_VOL);
vol=vol&0x00FF;
vol=(vol<<8)|((~vol) & 0x00FF);

data[0]=0x55aa;
data[1]=vol;
if (adiQueueCommand(data,2,0,0,0,0,0,0)) return(1);
return(0);
}
/************************************************************************/
int adiAttractVolume(void) 
{
U16 vol;
U16 data[2];

vol=eer_gets(EER_AUD_VOL);
vol=(vol>>8)&0x00FF;
vol=(vol<<8)|((~vol) & 0x00FF);

data[0]=0x55aa;
data[1]=vol;

if (adiQueueCommand(data,2,0,0,0,0,0,0)) return(1);
return(0);
}
/*****************************************************************************/
/***************************************************************************/
/***************************************************************************/
int adiNumberOfSounds(void)
{
int aud_number_of_sounds(void);
return(aud_number_of_sounds());
}
/***************************************************************************/
int adiAddressOfPartition(int part_num)
{
int aud_AddressOfPartition(int part_num) ;
return(aud_AddressOfPartition(part_num));
}
/***************************************************************************/
int adiSizeOfPartition(int part_num)
{
int aud_SizeOfPartition(int part_num) ;
return(aud_SizeOfPartition(part_num));
}
/***************************************************************************/
void adiClearAll(void)
{
void aud_clear_all(void);
aud_clear_all();
}
/***************************************************************************/
void adiClearAllBanks(void)
{
void aud_clear_all_banks(void);
aud_clear_all_banks();
}
/***************************************************************************/
int adiLoadBank(U8 *data_buffer)
{
int aud_load_bank (U8 *data_buffer);
int x;

adiLockQueue();
/*adiSendAllQueue();*/
x=aud_load_bank (data_buffer);
adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiLoadBankWithPlayback(U8 *data_buffer)
{
int aud_load_bank_with_playback (U8 *data_buffer);
int x;

adiLockQueue();
x=aud_load_bank_with_playback(data_buffer);
adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiMakePartition(int cmd_size,int data_size)
{
int aud_make_partition (int cmd_size,int data_size);

return(aud_make_partition (cmd_size,data_size));
}
/***************************************************************************/
int adiLoadPartition(int cmd,U8 *data)
{
int aud_load_partition (int cmd,U8 *data);
int x;

adiLockQueue();
x=aud_load_partition(cmd,data);
adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiLoadPartitionWithPlayback(int cmd,U8 *data)
{
int aud_load_partition_with_playback(int cmd,U8 *data);
int x;

adiLockQueue();
x=aud_load_partition_with_playback(cmd,data);
adiUnlockQueue();
return(x);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
int adiInitBlockedLoad(U8 *buff,int size)
{
int audInitBlockedLoad(U8 *buff,int size);
int x;
x=audInitBlockedLoad(buff,size);
return(x);
}
/***************************************************************************/
int adiPreloadBlockedBankLoad(U8 *buff,int len)
{
int audPreloadBlockedBankLoad(U8 *buff,int len);
int x;
adiLockQueue();
x=audPreloadBlockedBankLoad(buff,len);
if (!x) adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiPreloadBlockedBankLoadWithPlayback(U8 *buff,int len)
{
int audPreloadBlockedBankLoadWithPlayback(U8 *buff,int len);
int x;
adiLockQueue();
x=audPreloadBlockedBankLoadWithPlayback(buff,len);
if (!x) adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiPreloadBlockedPartitionLoad(U16 cmd,U8 *buff,int len)
{
int audPreloadBlockedPartitionLoad(U16 cmd,U8 *buff,int len);
int x;
adiLockQueue();
x=audPreloadBlockedPartitionLoad(cmd,buff,len);
if (!x) adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiPreloadBlockedPartitionLoadWithPlayback(U16 cmd,U8 *buff,int len) 
{
int audPreloadBlockedPartitionLoadWithPlayback(U16 cmd,U8 *buff,int len) ;
int x;

adiLockQueue();
x=audPreloadBlockedPartitionLoadWithPlayback(cmd,buff,len) ;
if (!x) adiUnlockQueue();
return(x);
}
/***************************************************************************/
int adiLoadBlock(U8 *buffer,int size)
{
int audLoadBlock(U8 *buffer,int size);
int x;

x=audLoadBlock(buffer,size);

if (x<1) adiUnlockQueue();
return(x);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
#if 0
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int adiSendDMBlock(char *buffer, int start, int end)          */
/*                                                                         */
/*           Transfers a block on memory from the host to the V++ DSP's	   */
/*           Data Memory Segment (data width = 16 bits)                    */
/*                                                                         */
/* INPUTS:   char *buffer - The block of memory to xfer to the dsp         */
/*           long start   - DSP Data Memory starting address 		       */
/*           long end     - DSP Data Memory ending address                 */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout or checksum failure                          */
/*                                                                         */
/***************************************************************************/
int adiSendDMBlock(U8 *buffer,U16 start,U16 end) 
{
S32 size;			/* number of 16-bit words to send */
S32 i;				/* counters for send loop */
U16 temp;
U32 checksum;			/* our computed checksum as data sent */

if (buffer == NULL) {auderr=AUDERR_NO_DATA;return(ERROR);}
if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}

adiLockQueue();

/* load command format:  command, start, end, type, data... , checksum */

if (_adiPutLatch(SOUND_CMD_LOAD) != OK) return(ERROR);
if (_adiPutLatch(start) != OK) return(ERROR);
if (_adiPutLatch(end) != OK) return(ERROR);
if (_adiPutLatch(SOUND_LOAD_TYPE_DM) != OK) return(ERROR);

size = (end - start + 1)*2;
checksum=0;
i=0;

while(i<size) 
   	{
	temp = (((short)buffer[i++])&0x000000FF) <<8; 
	temp = temp | (((short)buffer[i++])&0x000000FF);
	checksum += temp;

	if (_adiPutLatch(temp)!=OK)	return(ERROR);
	}

/* after the data is loaded, the sound DSP will return its */
/* checksum... make sure it matches ours */

if (_adiGetLatch(&temp) != OK) return(ERROR);
if ((checksum & MASK16) != temp) {auderr=AUDERR_CHECKSUM;return(ERROR);}

/* after the checksum, monitor should go back to ready mode */

if (_adiGetLatch(&temp) != OK ) {auderr=AUDERR_NO_ACK;return(ERROR);}
if (temp != MONITOR_READY) {auderr=AUDERR_BAD_ACK;return(ERROR);}

adiUnlockQueue();

return(OK);
}

/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int adiSendPMBlock(char *buffer, int start, int end)          */
/*                                                                         */
/*           Transfers a block on memory from the host to the V++ DSP's	   */
/*           Program Memory Segment (data width = 24 bits)                 */
/*                                                                         */
/* INPUTS:   char *buffer - The block of memory to xfer to the dsp         */
/*           long start   - DSP Data Memory starting address 		       */
/*           long end     - DSP Data Memory ending address                 */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout or checksum failure                          */
/*                                                                         */
/***************************************************************************/
int adiSendPMBlock(U8 *buffer,U16 start,U16 end) 
{
S32 size;			/* number of 16-bit words to send */
S32 i;				/* counters for send loop */
U16 temp,temp2;
U32 checksum;			/* our computed checksum as data sent */

if (buffer == NULL) {auderr=AUDERR_NO_DATA;return(ERROR);}
if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}

adiLockQueue();

/* send the load command */
/* format is command, start, end, type, data... return(checksum) */

if (_adiPutLatch(SOUND_CMD_LOAD) != OK) return(ERROR);
if (_adiPutLatch(start) != OK) return(ERROR);
if (_adiPutLatch(end) != OK) return(ERROR);
if (_adiPutLatch(SOUND_LOAD_TYPE_PM) != OK) return(ERROR);

/* for each 24-bit word sent to the sound DSP */
/* we actually send two 16-bit words */

size = (end - start + 1) * 3;

i = 0;
checksum = 0;
while (i<size) 
	{
	temp = (((short)buffer[i++])&0x000000FF) <<8;
	temp = temp | (((short)buffer[i++])&0x000000FF);
	checksum += temp;
	if (_adiPutLatch(temp)!=OK) return(ERROR);

	temp2= buffer[i++] | 0x0000FF00;
	checksum += temp2;
	if (_adiPutLatch(temp2)!=OK) return(ERROR);
    }

/* after the data is loaded, the sound DSP will return its */
/* checksum... make sure it matches ours */	
if (_adiGetLatch(&temp) != OK) return(ERROR);
if ((checksum & MASK16) != temp) {auderr=AUDERR_CHECKSUM;return(ERROR);}

/* after the checksum, monitor should go back to ready mode */
if (_adiGetLatch(&temp) != OK ) {auderr=AUDERR_NO_ACK;return(ERROR);}
if (temp != MONITOR_READY) {auderr=AUDERR_BAD_ACK;return(ERROR);}

adiUnlockQueue();
return(OK);
}
/***************************************************************************/
#endif
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
int adiPoll(unsigned long time) 
{
if (aud_stat_touched)
	{
	aud_stat_touched=0;
#if defined(USER_REC_UREC10)
	eer_user_wrt(USER_REC_UREC10);
	eer_user_free(USER_REC_UREC10);
	eer_user_wrt(USER_REC_UREC11);
	eer_user_free(USER_REC_UREC11);
	eer_user_wrt(USER_REC_UREC12);
	eer_user_free(USER_REC_UREC12);
	eer_user_wrt(USER_REC_UREC13);
	eer_user_free(USER_REC_UREC13);
#endif
	}
return 0;
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

