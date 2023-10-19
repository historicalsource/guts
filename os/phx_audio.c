/*
 *	$Id: phx_audio.c,v 1.46 1997/10/13 21:25:49 todd Exp $
 *
 *		Copyright 1996 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 *	'C' versions of basic audio control routines on a board with a separate
 *	audio processor. This version for the Phoenix DSP audio board.
 *
 */
#include <config.h>
#include <stdarg.h>
#include <os_proto.h>
#include <eer_defs.h>
#include <phx_audio_proto.h>
#include <phx_audio_internal.h>
		    
#if !defined(SOUND_NEW_MAKEFILE)
#include <phx_audio_selftest.c>
#endif

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#if !defined(SOUND_CALL_TABLE_SIZE)
	#define SOUND_CALL_TABLE_SIZE 0x1000
#endif

#if !defined(SOUND_MAX_PARTITIONS)
	#define SOUND_MAX_PARTITIONS 32
#endif


int aud_load_partition_internal (int cmd,U8 *data,U16 type);
int download_snd_table (char *table_addr, int start, int end,U16 type);
int download_snd_bank (U8 *buffer, int start, int end,U16 type);
int aud_load_internal (U8 *data_buffer,U32 cmd_addr, U32 data_addr,U16 type);
int aud_block_load_internal (U8 *data_buffer,U32 cmd_addr, U32 data_addr,U16 type);
int aud_load_bank_internal (U8 *data_buffer,U16 type);

U32 auderr;

#define MIN(a,b) {if (a<b) return(a); else return(b);}
#define MAX(a,b) {if (a>b) return(a); else return(b);}

#define STATIC static

typedef unsigned short aud_t;
#define MAX_AUD_TRIES	(0)		/* do not retry if output queue is busy */
#define MAX_EXC_TRIES	(100)		/* at least 10us for exception response */
#define MAX_RESET_TIME 	(30)		/* 30 fields, 1/2 second */
#define CAGE_CTL (SND_BUF)
#define RESETOK (0x000A)		/* first response from audio board */

/*	Try to set a reasonable default for the location and size of the
 *	Attract-mode volume ratio. This will only be used if EER_AUD_ATRVOL
 *	is not defined.
 */
#ifndef AUD_AV_MSK
# ifdef EER_GUTS_OPT
#  define AUD_AV_MSK (3)
#  define AUD_AV_SHF (0)
# else
#  define AUD_AV_MSK (0)
# endif /* EER_GUTS_OPT defined */
#endif /* AUD_AV_MSK defined */

#ifndef SND_IDX_MSK
# define SND_IDX_MSK 	(0x3F)
#endif
#ifndef AUD_MAX_TIME
# define AUD_MAX_TIME 	(40000L)	/* about five frames without output */
#endif

extern U8 phx_audosys[], phx_audosys_e[];
extern U8 phx_audcomp[], phx_audcomp_e[];
extern U8 phx_auddmext[], phx_auddmext_e[];
extern U8 phx_auddmint[], phx_auddmint_e[];

#ifndef SOUND_CALL_TABLE_SIZE
#define SOUND_CALL_TABLE_SIZE 0x1000
#endif
#define SOUND_MAX_PARTITIONS 32
U32 snd_bank_data_start;
U32 snd_bank_cmd_start;
U32 snd_partition_data[SOUND_MAX_PARTITIONS];
U32 snd_partition_cmd[SOUND_MAX_PARTITIONS];
U32 snd_partition_index;
U32 snd_partition_data_addr;
U32 snd_partition_cmd_addr;
U32 snd_bank_data_addr;
U32 snd_bank_cmd_addr;

/***************************************************************************/
/* IOASIC_BASE is defined in /home/sequoia/usr2/guts/3dfx/mace/phr4k/inc/config.h */
/***************************************************************************/
#define IO_ASIC_CONTROL_REGISTER (*((VU16*)IO_MAIN_CTL))
#define SOUND_CONTROL_REGISTER   (*((VU16*)IO_H2SND_CTL))
#define SOUND_PORT_GET_DATA      (*((VU16*)IO_SND2H_DTA))
#define SOUND_PORT_FLAGS         (*((VU16*)IO_SND_STS))
#define SOUND_PORT_PUT_DATA      (*((VU16*)IO_H2SND_DTA))
/***************************************************************************/
#define OK 0
#define ERROR 1
#define NULL 0
#define MASK32 0xFFFFFFFF
#define MASK16 0x0000FFFF
#define SOUND_TIMEOUT 1000000
#define MONITOR_READY 0x000A
#define SOUND_PORT_SEND_READY 0x80
#define SOUND_PORT_DATA_READY 0x40
#define SOUND_LOAD_TYPE_PM 0x00000000
#define SOUND_LOAD_TYPE_DM 0x00000001
#define OPSYS_READY 0x000C
/***************************************************************************/
#define SOUND_CMD_LOAD  0x0000001A
#define SOUND_CMD_RUN   0x0000002A
#define LOAD_DRAM 0x55D0
#define LOAD_DRAM_WITH_PLAYBACK 0x55D1
/***************************************************************************/
int soundHardwareReset(void);
int soundGetWord(U16 *data);
int soundPutWord(U16 data);
/*****************************************************************************/
/****************************************************************************
    		streaming stubs, added to get past broken audio.a
****************************************************************************/
void strmInit() {;}
void strmStartStreaming() {;}
int strmCurrentBufferDonePlaying() {return 1;}
void strmSwapBuffer() {return;}
void strmSetupStreaming() {;}
void strmStop() {;}
void strmPause() {;}
void strmResume() {;}
void strmSetVolume() {;}
int strmGetSampleRate() { return 42;}
void synPrintJVcb() {return;}
void aud_q_dump() {return;}
int aud_q_mput(int num, ...) {return 0;}
int aud_getErrorCount() {return 0;}
void aud_pm() {return;}
int aud_qlen(void) {return 0;}

/*		aud_reset()
 *	Resets audio processor and vital state.
 */
void aud_reset() {
    return;
}

int aud_cmd(int st_val) {
    return 0;
}

/*		aud_poll(usec)
 *	Check on sound processor status, using usec as the amount of time 
 *	since last check.  For queued sounds without CAGE style interrupts
 *	this routine will send a sound too.
 *	Return:
 *		1 if a sound has been sent
 *		0 if nothing to output 
 *		-1 if sound system should not be running from last reset
 *		-2 if sound subsystem was reset 
 */

int aud_poll(unsigned long time) {
    return 0;
}

/*		aud_setvol(volume)				(audio.c)
 *	Set the volume of sound.  A parameter of -1 will set the
 *	volume to the value stored in EEPROM. 
 *	Returns:
 *		-1 for an error
 *		<volume> that was set
 */

int aud_setvol(int volume) {
    return -1;
}

/*		aud_irq()
 *	Interrupt from sound processor.  Called from assembly after registers
 *	saved.
 */
void aud_irq(void) {
    return;
}

/*		aud_f_put(code)
 *	Attempts to send <code> to the sound processor. 
 *	Returns:
 *		0 if sound process (from reset) or port is currently busy
 *		1 if code sent
 */
int aud_f_put(int value) {
    return 0;
}

/*		aud_excw(code,buf,cnt)
 *	Handle an exception-mode transfer, in the "one byte out, 'cnt' bytes in"
 *	way.
 *	Returns:
 *		0 if exception in progress or sound processor is in reset
 *		1 if exception complete or under way
 */
int aud_excw(int code, void *buff, int count) {
    return 0;
}
/*		aud_q_get() 
 *	Returns:
 *		-1 if no input available
 *		else data from port
 */
long aud_q_get(void) {
    return -1;
}

/*		aud_q_put(code)
 *	Adds <code> to queue of "sounds" to play. <code> is assumed to be a
 *	single aud_t. See aud_mput() for multi-aud_t commands. On a system with
 *	external Audio, <code> will then be written to the port communicating
 *	with the sound processor. On a host-based audio system, <code> will
 *	be placed directly in the "action" queue of the sound process. 
 *	Returns:
 *		0 if no room
 *		1 if on queue.
 */
int aud_q_put(int code) {
    return 0;
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundGetWord(unsigned int *data)                          */
/*                                                                         */
/*           Reads a single 16-bit word from the DSP.                      */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/* note: This function polls a bit to determine when the port is ready,    */
/*       this is a horible waste of time in a preemtive multitasking system*/
/*       although this is probably the fastest way.                        */
/***************************************************************************/
int soundGetWord(U16 *data) {
    U32 i;

    i=eer_rtc;
    while (!(SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) if (eer_rtc-i>2) {auderr=AUDERR_GET_XFER_TIMEOUT;return(ERROR);}

    *data = (SOUND_PORT_GET_DATA & 0x0000FFFF);

    SOUND_PORT_GET_DATA = 0x0000; /* this statment should only be nessecarry for a c30 host */

    return(OK);
}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundPutWord(unsigned int data)                           */
/*                                                                         */
/*           Sends a single 16-bit word to the DSP.                        */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/* note: This function polls a bit to determine when the port is ready,    */
/*       this is a horible waste of time in a preemtive multitasking system*/
/***************************************************************************/
int soundPutWord(unsigned short data) {
    int i;

    i=eer_rtc;
    while (!(SOUND_PORT_FLAGS & SOUND_PORT_SEND_READY)) if (eer_rtc-i>2) {auderr=AUDERR_XFER_TIMEOUT;return(ERROR);}

    SOUND_PORT_PUT_DATA = data;
    return(OK);
}

/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundLoadDM(char *buffer, int start, int end)             */
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
int soundLoadDM(U8 *buffer,U16 start,U16 end) {
    S32 size;			/* number of 16-bit words to send */
    S32 i;				/* counters for send loop */
    U16 temp;
    U32 checksum;			/* our computed checksum as data sent */

    if (buffer == NULL) {auderr=AUDERR_NO_DATA;return(ERROR);}
    if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}

/* load command format:  command, start, end, type, data... , checksum */

    if (soundPutWord(SOUND_CMD_LOAD) != OK) return(ERROR);
    if (soundPutWord(start) != OK) return(ERROR);
    if (soundPutWord(end) != OK) return(ERROR);
    if (soundPutWord(SOUND_LOAD_TYPE_DM) != OK) return(ERROR);

    size = (end - start + 1)*2;
    checksum=0;
    i=0;
    while(i<size) {
	temp = (((short)buffer[i++])&0x000000FF) <<8; 
	temp = temp | (((short)buffer[i++])&0x000000FF);
	checksum += temp;

	if (soundPutWord(temp)!=OK)	return(ERROR);
    }

/* after the data is loaded, the sound DSP will return its */
/* checksum... make sure it matches ours */
    if (soundGetWord(&temp) != OK) return(ERROR);
    if ((checksum & MASK16) != temp) {auderr=AUDERR_CHECKSUM;return(ERROR);}

/* after the checksum, monitor should go back to ready mode */
    if (soundGetWord(&temp) != OK ) {auderr=AUDERR_NO_ACK;return(ERROR);}
    if (temp != MONITOR_READY) {auderr=AUDERR_BAD_ACK;return(ERROR);}

    return(OK);
}

/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundLoadPM(char *buffer, int start, int end)             */
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
int soundLoadPM(U8 *buffer,U16 start,U16 end) {
    S32 size;			/* number of 16-bit words to send */
    S32 i;				/* counters for send loop */
    U16 temp,temp2;
    U32 checksum;			/* our computed checksum as data sent */

    if (buffer == NULL) {auderr=AUDERR_NO_DATA;return(ERROR);}
    if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}

/* for each 24-bit word sent to the sound DSP */
/* we actually send two 16-bit words */

    size = (end - start + 1) * 3;

/* send the load command */
/* format is command, start, end, type, data... return(checksum) */

    if (soundPutWord (SOUND_CMD_LOAD) != OK) return(ERROR);
    if (soundPutWord (start) != OK) return(ERROR);
    if (soundPutWord (end) != OK) return(ERROR);
    if (soundPutWord (SOUND_LOAD_TYPE_PM) != OK) return(ERROR);

    i = 0;
    checksum = 0;
    while (i<size) {
	temp = (((short)buffer[i++])&0x000000FF) <<8;
	temp = temp | (((short)buffer[i++])&0x000000FF);
	checksum += temp;
	if (soundPutWord(temp)!=OK) return(ERROR);

	temp2= buffer[i++] | 0x0000FF00;
	checksum += temp2;
	if (soundPutWord(temp2)!=OK) return(ERROR);
    }

/* after the data is loaded, the sound DSP will return its */
/* checksum... make sure it matches ours */	
    if (soundGetWord(&temp) != OK) return(ERROR);
    if ((checksum & MASK16) != temp) {auderr=AUDERR_CHECKSUM;return(ERROR);}

/* after the checksum, monitor should go back to ready mode */
    if (soundGetWord(&temp) != OK ) {auderr=AUDERR_NO_ACK;return(ERROR);}
    if (temp != MONITOR_READY) {auderr=AUDERR_BAD_ACK;return(ERROR);}

    return(OK);
}

/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundHardwareReset(void)                                  */
/*                                                                         */
/*           Performs a hardware reset of the audio subsystem              */
/*                                                                         */
/* RETURNS: OK if successful                                               */
/*          ERROR if the ack does not come back.                           */
/*                                                                         */
/***************************************************************************/
int soundHardwareReset(void) {
    
    U16 sound_ack;

	auderr=AUDERR_NEEDS_RESET;
/* reset bit is bit 0 of I/O ASIC  */
    SOUND_CONTROL_REGISTER &= (~1);
    sound_ack=SOUND_PORT_GET_DATA; /* clear the read latch, just in case */
    prc_delay(2);
    SOUND_CONTROL_REGISTER |= (1);
    prc_delay(1);

/* wait for the acknoledgement */

	if (soundGetWord(&sound_ack)!=OK) return(ERROR);
    if (sound_ack != MONITOR_READY) {auderr=AUDERR_BAD_ACK;return(ERROR);}
	
    return(OK);
}
/***************************************************************************/

/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundSoftwareReset(void)                                  */
/*                                                                         */
/* Loads the os software to the V-- system                                 */
/*                                                                         */
/* The sound operating system consists of four files:                      */
/* 1. _comp.bin - 24-bit external program memory image                     */
/* 2. _osys.bin - 24-bit internal on-chip memory image                     */
/* 3. dm_ext.bin - 16-bit external data memory image                       */
/* 4. dm_int.bin - 16-bit internal on-chip memory image                    */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/***************************************************************************/
int soundSoftwareReset(void) {
    U16 retval;

    if (soundLoadDM(phx_auddmext,0x0800, 0x37ff)) return(ERROR); 
    if (soundLoadDM(phx_auddmint, 0x3800, 0x39ff)) return(ERROR); 
    if (soundLoadPM(phx_audcomp, 0x2800, 0x37ff)) return(ERROR);
    if (soundLoadPM(phx_audosys,0,0x3ff)) return(ERROR); /* load this one last */

    if(soundPutWord(0x002A)) return(ERROR);
    if(soundGetWord(&retval)) return(ERROR);
    if ((retval) != OPSYS_READY)  {auderr=AUDERR_BAD_ACK;return(ERROR);}
	auderr=AUDERR_AUD_OK;
    return(OK);
}
/***************************************************************************/
int soundCustomSoftwareReset(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int)
{
U16 retval;
int x;
int er;

x=0;
er=1;

while (x<5 && er!=0)
	{
	er=0;
    if (soundHardwareReset()) er++;
    if (!er) if (soundLatchTest()) er++;  /* if latch test fails, reset hardware, try again */
	x++;
	}
if(x>=5) {return(ERROR);}


if (soundLoadDM(dm_ext,0x0800, 0x37ff)) return(ERROR); 
if (soundLoadDM(dm_int, 0x3800, 0x39ff)) return(ERROR); 
if (soundLoadPM(comp, 0x2800, 0x37ff)) return(ERROR);
if (soundLoadPM(osys,0,0x3ff)) return(ERROR); /* load this one last */

if(soundPutWord(0x002A)) return(ERROR);
if(soundGetWord(&retval)) return(ERROR);
if ((retval) != OPSYS_READY)  {auderr=AUDERR_BAD_ACK;return(ERROR);}
auderr=AUDERR_AUD_OK;

aud_clear_all();
aud_attract_volume();
return(OK);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/* NOTE; after hearing Ed Keenin's (V++ DSP Programmer) speach about how busy
         he is with war gods and how he should not be contacted, I managed
	 to convice him that it would be easier for him just to give me 5
	 minutes of his time to help us debug the V++ system.  Two minutes
	 into the debugging help (and 10 minutes into the call) he mentioned
	 several useful commands that the epproms boot code contained.
	 These under documented features are used in the functions below  */

/***************************************************************************/
/*                                                                         */
/* FUNCTION: int soundRamTest(void)                                        */
/*                                                                         */
/*           Test all three S/RAM chips and the two banks of D/RAM         */
/*                                                                         */
/* INPUTS:  0 test SRAM / DRAM bank 0                                      */
/*          bit 0 Test SRAM                                                */
/*          bit 1 Test DRAM bank 0                                         */
/*          bit 2 Test DRAM bank 1                                         */
/*                                                                         */
/* RETURNS: OK if successful                                               */
/*          1 if SRAM #1 is faulty.                                        */
/*          2 if SRAM #2 is faulty.                                        */
/*          3 if SRAM #3 is faulty.                                        */
/*          4 if DRAM Bank 0 is faulty.                                    */
/*          5 if DRAM Bank 1 is faulty.                                    */
/*          9 unknown error code.                                          */
/*          <0 if a communication error occured.                           */
/*                                                                         */
/***************************************************************************/
/* note: this call is only valid BEFORE the DCS OS is loaded and run */
int soundRamTest(int code) {
    U16 sound_data;
    U16 sound_ack;

/* test SRAM */
if (code==0 || code & 1)
	{
    if (soundPutWord(0x003A) != OK) return(-1);
    prc_delay(10); /* Give the command a bit of time to complete */
    if (soundGetWord(&sound_data)) return(-2);
    prc_delay(1); 
    if (soundGetWord(&sound_ack)) return(-3);
    if (sound_data != 0xCC01) 
    	{
		if((sound_data & 0xFF00) != 0xEE00) return(9);
		else return(0x00FF & sound_data);
    	}
    if (sound_ack != MONITOR_READY) return(-4);
	}

/* test DRAM  bank 0 */
if (code==0 || code & 2)
	{
    if (soundPutWord(0x004A) != OK) return(-5);
    prc_delay(150); /* Give the command a bit of time to complete */
    if (soundGetWord(&sound_data)) return(-6);
    prc_delay(1); 
    if (soundGetWord(&sound_ack)) return(-7);
    if (sound_data == 0xEE04) return(4);
    else if (sound_data != 0xCC02) return(9);
    if (sound_ack != MONITOR_READY) return(-8);
	}

/* test DRAM bank 1 (NOTE: bank 1 is optional)*/
    if (code & 4) {
	if (soundPutWord(0x005A) != OK) return(-9);
	prc_delay(150); /* Give the command a bit of time to complete */
	if (soundGetWord(&sound_data)) return(-10);
    prc_delay(1); 
	if (soundGetWord(&sound_ack)) return(-11);
	if (sound_data == 0xEE05) return(5);
	else if (sound_data != 0xCC03) return(9);
	if (sound_ack != MONITOR_READY) return(-12);
    }
    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */

int soundGetPMChecksum(U16 *retval) {
    U16 sound_ack;

    if (soundPutWord(0x009A) != OK) return(ERROR);
    if (soundGetWord(retval)) return(ERROR);
    if (soundGetWord(&sound_ack)) return(ERROR);
    if (sound_ack != MONITOR_READY) return(ERROR);
    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */
int soundGetAsicRev(U16 *retval) {
    U16 sound_ack;

    if (soundPutWord(0x007A) != OK) return(ERROR);
    if (soundGetWord(retval)) return(ERROR);
    if (soundGetWord(&sound_ack)) return(ERROR);
    if (sound_ack != MONITOR_READY) return(ERROR);
    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */
int soundGetEppromRev(U16 *retval) {
    U16 sound_ack;

    if (soundPutWord(0x006A) != OK) return(ERROR);
    if (soundGetWord(retval)) return(ERROR);
    if (soundGetWord(&sound_ack)) return(ERROR);
    if (sound_ack != MONITOR_READY) return(ERROR);
    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */
int soundLatchTest(void) {
    U16 retval;
    U16 sound_ack;
    int i;

/* walking ones test */
    for (i=0;i<16;i++) {
	if (soundPutWord(0x008A) != OK) return(ERROR);
	if (soundPutWord((1<<i)) != OK) return(ERROR);
	if (soundGetWord(&retval)) return(ERROR);
	if (soundGetWord(&sound_ack)) return(ERROR);
	if (((1<<i)&0x0000FFFF) != ((~retval)&0x0000FFFF)) return(-(i+1));
	if (sound_ack != MONITOR_READY) return(ERROR);
    }
	
/* walking zeros test */

    for (i=0;i<16;i++) {
	if (soundPutWord(0x008A) != OK) return(ERROR);
	if (soundPutWord((~(1<<i))) != OK) return(ERROR);
	if (soundGetWord(&retval)) return(ERROR);
	if (soundGetWord(&sound_ack)) return(ERROR);
	if ((1<<i) != (retval)) return(-(i+1));
	if (sound_ack != MONITOR_READY) return(ERROR);
    }

    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */
int soundInternalPMChecksum(U16 *retval) {
    U16 sound_ack;

    if (soundPutWord(0x009A) != OK) return(ERROR);
    if (soundGetWord(retval)) return(ERROR+1);
    if (soundGetWord(&sound_ack)) return(ERROR+2);
    if (sound_ack != MONITOR_READY) return(-sound_ack);
    return(OK);
}

/***************************************************************************/
/* note: this call is only valid BEFORE the OS is loaded and run */
int soundEppromBong(void)  { /* blurb blurb blurb */ 
    U16 sound_ack;

    if (soundPutWord(0x00AA) != OK) return(ERROR);
    prc_delay(100); /* wait for bonk to finish */
    if (soundGetWord(&sound_ack)) return(ERROR);
    if (sound_ack != 0xCC04) return(ERROR);
    if (soundGetWord(&sound_ack)) return(ERROR);
    if (sound_ack != MONITOR_READY) return(ERROR);

    return(OK);
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
void aud_clear_all(void) {
    int x;
    U16 data;

    for(x=0;x<SOUND_MAX_PARTITIONS;x++) {
	snd_partition_data[x]=-1;
	snd_partition_cmd[x]=-1;
    }
    snd_partition_index=0;
    snd_partition_data[0]=SOUND_CALL_TABLE_SIZE;
    snd_partition_cmd[0]=2;

    snd_partition_data_addr = SOUND_CALL_TABLE_SIZE;
    snd_partition_cmd_addr = 2;

    snd_bank_data_start=-1;
    snd_bank_cmd_start=-1;
    snd_bank_data_addr = -1;
    snd_bank_cmd_addr = -1;

    while ((SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) 
    	{
		data = (SOUND_PORT_GET_DATA & 0x0000FFFF);
		prc_delay(0);
		auderr=AUDERR_WARNING_DATA_LEFT_ON_LATCH;
	    }
}

/***************************************************************************/
void aud_clear_all_banks() {
    U16 data;

    snd_bank_data_start=-1;
    snd_bank_cmd_start=-1;
    snd_bank_data_addr = -1;
    snd_bank_cmd_addr = -1;

    while ((SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) 
    	{
		data = (SOUND_PORT_GET_DATA & 0x0000FFFF);
		prc_delay(0);
		auderr=AUDERR_WARNING_DATA_LEFT_ON_LATCH;
	    }
}

/***************************************************************************/
/*                                                                         */
/* FUNCTION: aud_load_bank()                                               */
/*                                                                         */
/* Loads a .BNK sound bank file from the disk. Computes and stores the     */
/* sound call table, and stores the compressed data.                       */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_load_bank_internal (U8 *data_buffer,U16 type) 
	{
    unsigned int temp;
    int num_calls;
    int bank_size;
    int retval;

	if(data_buffer==0) {auderr=AUDERR_NO_DATA;return(0);}

    if (snd_bank_cmd_start==-1) 
    	{
		snd_bank_cmd_start=snd_partition_cmd_addr;
		snd_bank_data_start=snd_partition_data_addr;
		snd_bank_data_addr = snd_partition_data_addr;
		snd_bank_cmd_addr = snd_partition_cmd_addr;
	    }

    retval=aud_load_internal(data_buffer,snd_bank_cmd_addr, snd_bank_data_addr,type);

	if (retval)
		{ 
	    num_calls=aud_bank_cmd_size(data_buffer);
   		bank_size=aud_bank_data_size(data_buffer);

    	snd_bank_cmd_addr = (snd_bank_cmd_addr + (2 * num_calls) - 1) + 1;
    	temp = (snd_bank_data_addr / 2) + (bank_size / 2) + 1;
    	snd_bank_data_addr = (temp + 1) * 2;
		}
    return(retval);
	}
/***************************************************************************/
int aud_load_bank (U8 *data_buffer) 
	{
	aud_stop_all_sounds();
    return(aud_load_bank_internal(data_buffer,LOAD_DRAM));
	}
/***************************************************************************/
int aud_load_bank_with_playback (U8 *data_buffer) 
	{
    return(aud_load_bank_internal(data_buffer,LOAD_DRAM_WITH_PLAYBACK));
	}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_make_partition (int cmd_size,int data_size) 
	{
    if (snd_bank_cmd_start!=-1) {auderr=AUDERR_BANK_LOADED_PARTITION_FAILED;return(0);} /* bank already loaded, partitions locked */
    if (snd_partition_index>=SOUND_MAX_PARTITIONS) {auderr=AUDERR_TOO_MANY_PARTITIONS;return(0);}

    snd_partition_index++;

    snd_partition_data_addr=snd_partition_data_addr+data_size+2;
    snd_partition_data[snd_partition_index]=snd_partition_data_addr;

    snd_partition_cmd_addr=snd_partition_cmd_addr+(cmd_size*2)+2;
    snd_partition_cmd[snd_partition_index]=snd_partition_cmd_addr;

    return(snd_partition_cmd[snd_partition_index-1]/2);
	}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_load_partition (int cmd,U8 *data) 
	{
	aud_stop_all_sounds();
	return(aud_load_partition_internal(cmd,data,LOAD_DRAM));
	}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_load_partition_with_playback (int cmd,U8 *data) 
	{
	return(aud_load_partition_internal(cmd,data,LOAD_DRAM_WITH_PLAYBACK));
	}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_load_partition_internal (int cmd,U8 *data,U16 type) 
	{
    int x;
    int retval;

    if (data==0) {auderr=AUDERR_NO_DATA;return(0);}

    retval=0;

    if (cmd==0) 
    	{
		if (snd_bank_cmd_start!=-1) {auderr=AUDERR_BANK_LOADED_PARTITION_FAILED;return(0);}
		if (snd_partition_index>=SOUND_MAX_PARTITIONS) {auderr=AUDERR_TOO_MANY_PARTITIONS;return(0);}
	
		retval=aud_load_internal(data,snd_partition_cmd_addr,snd_partition_data_addr,type);

		if (retval) 
			{
		    snd_partition_cmd_addr=snd_partition_cmd_addr+(aud_bank_cmd_size(data)*2);
		    snd_partition_data_addr=snd_partition_data_addr+aud_bank_data_size(data)+2;

		    snd_partition_index++;

		    snd_partition_data[snd_partition_index]=snd_partition_data_addr;
	    	snd_partition_cmd[snd_partition_index]=snd_partition_cmd_addr;
			}
		return(retval);
    	}

    for (x=0;x<SOUND_MAX_PARTITIONS;x++) 
    	{
		if (snd_partition_cmd[x]==(cmd*2)) 
			{
		    if(aud_bank_cmd_size(data) > ((snd_partition_cmd[x+1]-snd_partition_cmd[x])/2)) {auderr=AUDERR_PARTITION_CMD_TOO_BIG;return(0);}
		    if(aud_bank_data_size(data) > ((snd_partition_data[x+1]-snd_partition_data[x]))) {auderr=AUDERR_PARTITION_DATA_TOO_BIG;return(0);}

		    retval=aud_load_internal(data,snd_partition_cmd[x],snd_partition_data[x],type);

		    return(retval);
			}
	    }
	auderr=AUDERR_COULD_NOT_FIND_PARTITION;
    return(0);
	}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_load_internal (U8 *data_buffer,U32 cmd_addr, U32 data_addr,U16 type) 
	{
    unsigned int temp1,temp2;
    U32 sound_call_table[1024];  /* fix me!! */

    int num_calls;
    int bank_size;
    int pos;
    int i;

    int retval;

	if (data_buffer == NULL) {auderr=AUDERR_NO_DATA;return(0);}
	if (type!=LOAD_DRAM && type!=LOAD_DRAM_WITH_PLAYBACK) {auderr=AUDERR_WRONG_LOAD_TYPE;return(0);}

	/* there is a 128 byte header that we can ignore */
	/* number of sound calls in the bank is stored in first data loc */

    num_calls=aud_bank_cmd_size(data_buffer);
    bank_size=aud_bank_data_size(data_buffer);
    pos=134;
	if (num_calls >= (1024>>1)) {auderr=AUDERR_INVALID_DATA;return(0);}
	if (bank_size > (1024*1024*8)) {auderr=AUDERR_INVALID_DATA;return(0);}

#if 0
	/* the table could be sparse, so be safe */
    for (i=0; i < num_calls; i++) sound_call_table[i] = 1;
#endif
	/* create the local, relocated sound call table */
    for (i=0; i < num_calls; i++) 
    	{
		temp1 = data_buffer[pos++];
		temp1 += (data_buffer[pos++] << 8);
		temp1 += (data_buffer[pos++] << 16);
		temp1 += (data_buffer[pos++] << 24);

		/* relative to last ending point */
		if (temp1 != MASK32) temp1 = temp1 + data_addr;
		sound_call_table[i] = temp1;
    	}

    temp1 = cmd_addr;
    retval= cmd_addr/2;
    temp2 = cmd_addr + (2 * num_calls) - 1;

    if ((download_snd_table ((char *)&sound_call_table[0], temp1, temp2,type)) != OK) return(0);

/* startin	 g address of D/RAM to load */
/* note the "addressing compensation" div by 2 */
    temp1 = data_addr / 2;
    temp2 = temp1 + (bank_size) / 2 + 1;
/* k still holds the index into the buffer loaded from disk */
    if ((download_snd_bank((&data_buffer[pos]), temp1, temp2,type)) != OK) return (0);

    return(retval);
   	}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/

int download_snd_table (char *table_addr, int start, int end,U16 type) {
    int i;
    unsigned int size;
    unsigned int checksum;
    U16 table_checksum;
    U16 temp;
    U16 temp2;

	if (type!=LOAD_DRAM && type!=LOAD_DRAM_WITH_PLAYBACK) {auderr=AUDERR_WRONG_LOAD_TYPE;return(ERROR+3);}
	if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}
	
	size = (end - start + 1)*2;                 

    if (soundPutWord(type) != OK) return(ERROR+4);           

/* send the starting load address */
    if (soundPutWord((start >> 16) & MASK16) != OK) return(ERROR+5);         
    if (soundPutWord(start & MASK16) != OK) return(ERROR+6);         

/* send the ending load address */
    if (soundPutWord((end >> 16) & MASK16) != OK) return(ERROR+7);         
    if (soundPutWord(end & MASK16) != OK) return(ERROR+8);         

/* now send the data */

    i = 0;
    checksum = 0;

    while (i < size) {
	temp=(table_addr[i++] &0x00FF);
	temp=temp|((table_addr[i++] &0x00FF)<<8);
	temp2=(table_addr[i++] &0x00FF);
	temp2=temp2|((table_addr[i++] &0x00FF)<<8);
	if (soundPutWord(temp2)!= OK) return(ERROR+9);
	if (soundPutWord(temp)!= OK) return(ERROR+10);
	checksum += temp;
	checksum += temp2;
    }
#if 0
    for (j= 0; j < size; j++) {
	temp=(table_addr[i++]) << 8;
	temp=temp|(table_addr[i++]&0x00FF);
	if (soundPutWord(temp)!= OK) return(ERROR+9);
	checksum += temp;
    }
#endif

    if (soundGetWord(&table_checksum)!= OK) return(ERROR+11);         
    if ((checksum & MASK16) != table_checksum) {auderr=AUDERR_CHECKSUM;return(ERROR+12);}

    return(OK);
}

/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/

int download_snd_bank (U8 *buffer, int start, int end,U16 type) { 
    int i,j;
    unsigned int size;
    unsigned int checksum;
    U16 bank_checksum;
    U16 temp;

	if (type!=LOAD_DRAM && type!=LOAD_DRAM_WITH_PLAYBACK) {auderr=AUDERR_WRONG_LOAD_TYPE;return(ERROR+3);}
	if (start>=end) {auderr=AUDERR_START_GE_END;return(ERROR);}
	if (soundPutWord(type) != OK) return(ERROR+12);           

    if (soundPutWord((start >> 16) & MASK16) != OK) return(ERROR+13);         
    if (soundPutWord(start & MASK16) != OK) return(ERROR+14);         

    if ((soundPutWord(end >> 16) & MASK16) != OK) return(ERROR+15);        
    if ((soundPutWord(end & MASK16)) != OK) return(ERROR+16);         

/* now send the data */
    i = 0;

    checksum = 0;

    size = (end - start + 1);
    checksum=0;
    i=0;
/*while(i<=size) */
    for (j= 0; j < size; j++) {
	if (!(j&0x000007FF)) WDOG=0;
	temp = (((short)buffer[i++])&0x000000FF) <<8; 
	temp = temp | (((short)buffer[i++])&0x000000FF);
	checksum += temp;
	if (soundPutWord(temp)!=OK)	return(ERROR+17);
    } 

    if((soundGetWord(&bank_checksum))!=OK) return(ERROR+18);         
    if ((checksum & MASK16) != bank_checksum) {auderr=AUDERR_CHECKSUM;return(ERROR+12);}

    return(OK);
}
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
struct aud_queue_instance aqi;
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_block_load_partition_internal (int cmd,U8 *data,U16 type) 
	{
	int x;
    int retval;

	if (data == NULL) {auderr=AUDERR_NO_DATA;return(0);}
    retval=0;

    if (cmd==0) 
    	{
		if (snd_bank_cmd_start!=-1) {auderr= AUDERR_BANK_LOADED_PARTITION_FAILED;return(0);}
		if (snd_partition_index>=SOUND_MAX_PARTITIONS) {auderr=AUDERR_TOO_MANY_PARTITIONS;return(0);}
	
		retval=aud_block_load_internal(data,snd_partition_cmd_addr,snd_partition_data_addr,type);

		if (retval) 
			{
		    snd_partition_cmd_addr=snd_partition_cmd_addr+(aud_bank_cmd_size(data)*2);
		    snd_partition_data_addr=snd_partition_data_addr+aud_bank_data_size(data)+2;

		    snd_partition_index++;

		    snd_partition_data[snd_partition_index]=snd_partition_data_addr;
	    	snd_partition_cmd[snd_partition_index]=snd_partition_cmd_addr;
			}
		return(retval);
    	}

    for (x=0;x<SOUND_MAX_PARTITIONS;x++) 
    	{
		if (snd_partition_cmd[x]==(cmd*2)) 
			{
 			retval=aud_block_load_internal(data,snd_partition_cmd[x],snd_partition_data[x],type);

		    return(retval);
			}
	    }
	auderr=AUDERR_COULD_NOT_FIND_PARTITION;
    return(0);
	}
/***************************************************************************/
int aud_block_load_bank_internal (U8 *buff,U16 type) 
	{
    unsigned int temp;
    int num_calls;
    int bank_size;
    int retval;

	if (buff == NULL) {auderr=AUDERR_NO_DATA;return(0);}

    if (snd_bank_cmd_start==-1) 
    	{
		snd_bank_cmd_start=snd_partition_cmd_addr;
		snd_bank_data_start=snd_partition_data_addr;
		snd_bank_data_addr = snd_partition_data_addr;
		snd_bank_cmd_addr = snd_partition_cmd_addr;
	    }

    retval=aud_block_load_internal(buff,snd_bank_cmd_addr, snd_bank_data_addr,type);
	
	if (retval)
		{ 
	    num_calls=aqi.cmd_size;
   		bank_size=aqi.data_size;

    	snd_bank_cmd_addr = (snd_bank_cmd_addr + (2 * num_calls) - 1) + 1;
    	temp = (snd_bank_data_addr / 2) + (bank_size / 2) + 1;
    	snd_bank_data_addr = (temp + 1) * 2;
		}
    return(retval);
	}
/***************************************************************************/
/***************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int aud_block_load_internal (U8 *data_buffer,U32 cmd_addr, U32 data_addr,U16 type) 
	{
    unsigned int temp1,temp2;
    U32 sound_call_table[1024];  /* fix me!! */

    int num_calls;
    int bank_size;
    int pos;
    int i;

    int retval;

	if (data_buffer == NULL) {auderr=AUDERR_NO_DATA;return(0);}
	if (type!=LOAD_DRAM && type!=LOAD_DRAM_WITH_PLAYBACK) {auderr=AUDERR_WRONG_LOAD_TYPE;return(0);}

    num_calls=aqi.cmd_size;
	bank_size=aqi.data_size;

    pos=0;
	if (num_calls >= (1024>>1)) return(0);
	if (bank_size > (1024*1024*8)) return(0);

	/* create the local, relocated sound call table */
    for (i=0; i < num_calls; i++) 
    	{
		temp1 = data_buffer[pos++];
		temp1 += (data_buffer[pos++] << 8);
		temp1 += (data_buffer[pos++] << 16);
		temp1 += (data_buffer[pos++] << 24);

		/* relative to last ending point */
		if (temp1 != MASK32) temp1 = temp1 + data_addr;
		sound_call_table[i] = temp1;
    	}

    temp1 = cmd_addr;
    retval= cmd_addr/2;
    temp2 = cmd_addr + (2 * num_calls) - 1;

    if ((download_snd_table ((char *)&sound_call_table[0], temp1, temp2,type)) != OK) return(0);

/* startin	 g address of D/RAM to load */
/* note the "addressing compensation" div by 2 */
    temp1 = data_addr / 2;
    temp2 = temp1 + (bank_size) / 2 + 1;

	aqi.checksum=0;
	aqi.xfer= (bank_size + 4);

	if (soundPutWord(type) != OK) return(0);           
	
	if (soundPutWord((temp1 >> 16) & MASK16) != OK) return(0);         
    if (soundPutWord(temp1 & MASK16) != OK) return(0);         
	
	if ((soundPutWord(temp2 >> 16) & MASK16) != OK) return(0);        
    if ((soundPutWord(temp2 & MASK16)) != OK) return(0);         
	
 	return(cmd_addr/2);
	}
/***************************************************************************/
/*****************************************************************************/
/*                                                                         */
/*                                                                         */
/***************************************************************************/
int audInitBlockedLoad(U8 *buff,int size)
	{
	if (buff == NULL) {auderr=AUDERR_NO_DATA;return(0);}
	if (size!=134) {auderr=AUDERR_WRONG_DATA_SIZE;return(0);}
	aqi.cmd_size = aud_bank_cmd_size(buff);
	aqi.data_size=aud_bank_data_size(buff);
	return(aqi.cmd_size<<2);
	}
/***************************************************************************/
int audPreloadBlockedBankLoadWithPlayback(U8 *buff,int len)
	{
	if (len!=(aqi.cmd_size<<2)) {auderr=AUDERR_WRONG_DATA_SIZE;return(0);}
    return(aud_block_load_bank_internal(buff,LOAD_DRAM_WITH_PLAYBACK));
	}
/***************************************************************************/
int audPreloadBlockedBankLoad(U8 *buff,int len)
	{
	if (len!=(aqi.cmd_size<<2)) {auderr=AUDERR_WRONG_DATA_SIZE;return(0);}
	aud_stop_all_sounds();
    return(aud_block_load_bank_internal(buff,LOAD_DRAM));
	}
/***************************************************************************/
int audPreloadBlockedPartitionLoad(U16 cmd,U8 *buff,int len)
	{
	if (len!=(aqi.cmd_size<<2)) {auderr=AUDERR_WRONG_DATA_SIZE;return(0);}
	aud_stop_all_sounds();
    return(aud_block_load_partition_internal(cmd,buff,LOAD_DRAM));
	}
/***************************************************************************/
int audPreloadBlockedPartitionLoadWithPlayback(U16 cmd,U8 *buff,int len)
	{
	if (len!=(aqi.cmd_size<<2)) {auderr=AUDERR_WRONG_DATA_SIZE;return(0);}
	return(aud_block_load_partition_internal(cmd,buff,LOAD_DRAM_WITH_PLAYBACK));
	}
/*****************************************************************************/
int audLoadBlock(U8 *buffer,int size)
	{
	int j;
	U16 temp;
	if (buffer == NULL) {auderr=AUDERR_NO_DATA;return(0);}

	if (size>aqi.xfer) size=aqi.xfer;
    for (j= 0; j < size; )
       	{
       	temp = (((short)buffer[j++])&0x000000FF) <<8; 
		temp = temp | (((short)buffer[j++])&0x000000FF);
		aqi.checksum += temp;

		if (soundPutWord(temp)!=OK)	return(-1);
    	}

	aqi.xfer -= size;

	if (aqi.xfer==0)
		{
		if((soundGetWord(&temp))!=OK)
			if((soundGetWord(&temp))!=OK)
				if((soundGetWord(&temp))!=OK) 
					if((soundGetWord(&temp))!=OK)
						if((soundGetWord(&temp))!=OK) return(-2);
		if ((aqi.checksum & MASK16) != temp) {auderr=AUDERR_CHECKSUM;return(-3);}
		}
	return(aqi.xfer);
	}
/************************************************************************/
int audGetExtendedError(void)
{
return(auderr);
}
/************************************************************************/
void audClearExtendedError(void)
{
auderr=AUDERR_AUD_OK;
}
/************************************************************************/
/*****************************************************************************/
#if 0
#define BLK_SIZE (48)
int block_test(const struct menu_d *smp) 
	{
    int idx;
	int i,pos,len;
	U16 temp;

    if (!aud_init(0)) return -1;
    if (aud_game_volume()==0) return(-1);

	len=audInitBlockedLoad(phx_testbank,134);
	pos=134;
	pos=pos+len;
 	idx=audPreloadBlockedBankLoad(&phx_testbank[pos-len],len);
	len=BLK_SIZE;

	while(len)
		{
		if (len>BLK_SIZE) len=BLK_SIZE;
		pos=pos+len;
		len=audLoadBlock(&phx_testbank[pos-len],len);
		if(!len) break;
		if(len<0) {PrintNum2(len);break;/* Unrecoverable Error */}
		}

    ExitInst(INSTR_PAL);

    i=0;
    while ( 1 )
       	{
       	U32 ctls;
		ctls = ctl_read_sw(SW_NEXT);

		if ((ctls&SW_NEXT)) break;

		if ((i&0x001f)==0) 
			{
		    if (soundPutWord(idx+1)) return(-1);
		    if (soundPutWord(0xFF00)) return(-1);
	    	if (soundPutWord(0x0000)) return(-1);
	    	if (soundGetWord(&temp)) return(-1);
			}
		if ((i&0x001f)==0x0010) 
			{
		    if (soundPutWord(idx+2)) return(-1);
		    if (soundPutWord(0xFFFF)) return(-1);
	    	if (soundPutWord(0x0000)) return(-1);
	    	if (soundGetWord(&temp)) return(-1);
			}
		prc_delay(0);
		i++;
	    }
    ctl_read_sw(-1);			/* flush all edges */
    return soundHardwareReset();
}
#endif
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
int aud_bank_data_size(U8 *data_buffer) {
    int bank_size;
    int pos;

    pos=130;

    bank_size = data_buffer[pos++];
    bank_size += (data_buffer[pos++] << 8);
    bank_size += (data_buffer[pos++] << 16);
    bank_size += (data_buffer[pos++] << 24);

    return(bank_size);
}

/***************************************************************************/
int aud_bank_cmd_size(U8 *data_buffer) {
    int num_calls;
    int pos;

    pos=128;

    num_calls =  data_buffer[pos++];
    num_calls += (data_buffer[pos++] << 8);

    return(num_calls);
}
/************************************************************************/
int aud_number_of_sounds(void)
{
if (snd_bank_cmd_addr != -1) return(snd_bank_cmd_addr/2);
if (snd_partition_cmd_addr != -1) return(snd_partition_cmd_addr/2);
return(0);
}
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/*		aud_init(level)
 *	Resets audio processor (or process) and varying amounts of
 *	local state, depending on <level>.
 *
 *		If level is 0, reset the sound process[or] and any crucial
 *	state (e.g. coin variables) but do not wait for the sound processor
 *	to "come up". 
 *	Returns: 0x10000 = 1 sound and all status bits OK.
 *
 *		If level is < 0, reset as for 0, but leave the sound
 *	process[or] "dead" if possible. Not all configurations can do this,
 *	but it is provided as sort of a "finished with sounds" call for
 *	those that can, and would benefit in some way from having the sounds
 *	processor stopped. 
 *	Returns: 0x10000 = 1 sound and all status bits OK.
 *
 *		If level > 0, reset as for 0, but also wait for the sound
 *	process[or] to "come up", then return a "status" and the "number of 
 *	sounds".  
 *	Returns:
 *		-1 = sound processor did not respond or more than 32768 sounds
 *		0 = user aborted (there are always 1 sound STOP, NOISY, ...)
 *		0xnnnnssss = nnnn sounds and status of ssss.
 */
long aud_init(int level) 
	{
	int x;
	int er;

	x=0;
	er=1;

	while (x<5 && er!=0)
		{
		er=0;
	    if (soundHardwareReset()) er++;

	    if (!er) if (soundLatchTest()) er++;
	    if (!er) if (soundSoftwareReset()) er++;
		x++;
		}
	if(x>=5) {return(0);}
	
    aud_clear_all();
    aud_attract_volume();
	return 1;
	}
/************************************************************************/
void aud_stop_all_sounds()
	{
	U16 temp;

	if(soundPutWord(0x55d2)) return;	  /* flush all pending soundcalls */
	if(soundGetWord(&temp)) return;
	if(soundPutWord(0x55d3)) return;	 /* flush i/o queues */
	if(soundGetWord(&temp)) return;
	if(soundPutWord(0)) return;
	}
/***************************************************************************/
int aud_game_volume(void)
	{
    U16 vol;
    vol=eer_gets(EER_AUD_VOL);
    vol=vol&0x00FF;
    vol=(vol<<8)|((~vol) & 0x00FF);
    if (soundPutWord(0x55AA)) return(0);
    if (soundPutWord(vol)) return(0);
    return(1);
	}
/************************************************************************/
int aud_attract_volume(void) 
	{
    U16 vol;
    vol=eer_gets(EER_AUD_VOL);
    vol=(vol>>8)&0x00FF;
    vol=(vol<<8)|((~vol) & 0x00FF);

    if (soundPutWord(0x55AA)) return(0);
    if (soundPutWord(vol)) return(0);
    return(1);
	}
/*****************************************************************************/

U16 aud_play_sound(U16 sound,U8 vol,U8 pan,U16 priority) {
U16 temp;

if(soundPutWord(sound)) return(0);
temp=(vol<<8)+pan;
if(soundPutWord(temp)) return(0);
if(soundPutWord(priority)) return(0);
if(soundGetWord(&temp)) return(0);
return(temp);
}

/*****************************************************************************/

/*		aud_mput(buff,cnt)
 *	Adds <cnt> aud_t's in buffer pointed to by <buff> to queue of "sounds"
 *	to play. <buff> is assumed to contain _one_ complete command, so the
 *	first aud_t will have D15 set, and the remainder will have D15 cleared.
 *	Also, this routine will place the whole buffer, or none of it, on the
 *	queue.
 *		Note that the <param> are masked with 0x7FFF
 *	to insure that they are not treated as commands.
 *	Returns:
 *		0 if no room (i.e. not sent)
 *		<cnt> if queued
 */
/*****************************************************************************/
int aud_mput( unsigned short *buff, int cnt) {
    while (cnt) {
	if (soundPutWord(*buff)) return(0);
	buff++;
	cnt--;
    }
    return 1;
}
/*****************************************************************************/

/*		aud_put(code)
 *	Tries up to MAX_AUD_TRIES times to write <code> to sound port.
 *	Returns:
 *		0 if failed
 *		1 if sent
 *		2 if on queue (if MAX_AUD_TRIES == 0)
 */
/*****************************************************************************/
int aud_put(int code) {
    return((soundPutWord(code) ? 1:0));
}
/*****************************************************************************/
int aud_get(U16 *data) {
    return((soundGetWord(data) ? 1:0));
}
/*****************************************************************************/
int aud_get_safe(U16 *data) { /* Safe to call from irq */
    U32 i;

    i=0;
    while (!(SOUND_PORT_FLAGS & SOUND_PORT_DATA_READY)) if (i++>500000) {auderr=AUDERR_SAFE_GET_XFER_TIMEOUT;return(0);}

    *data = (SOUND_PORT_GET_DATA & 0x0000FFFF);

    SOUND_PORT_GET_DATA = 0x0000; /* this statment should only be nessecarry for a c30 host */

    return(1);
}
/***************************************************************************/
int aud_put_safe(U16 data) {
    int i;

    i=0;
    while (!(SOUND_PORT_FLAGS & SOUND_PORT_SEND_READY)) if (i++>500000) {auderr=AUDERR_SAFE_XFER_TIMEOUT;return(0);}

    SOUND_PORT_PUT_DATA = data;
    return(1);
}
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/***************************************************************************/
/*                                                                         */
/* FUNCTION: int aud_InitLoadSoftware(char *comp , char *osys ,			   */
/*                                         char *dm_ext , char *dm_int)    */
/* Loads the os software to the DCS system                                 */
/*                                                                         */
/* INPUTS: (The sound operating system consists of four files)             */
/* 1. comp   =  _comp.bin - 24-bit external program memory image           */
/* 2. osys   =  _osys.bin - 24-bit internal on-chip memory image           */
/* 3. dm_ext = dm_ext.bin - 16-bit external data memory image              */
/* 4. dm_int = dm_int.bin - 16-bit internal on-chip memory image           */
/*                                                                         */
/* RETURNS:  OK if successful                                              */
/*           ERROR if timeout                                              */
/*                                                                         */
/*   (note: this function assumes the size of the files are constant)      */
/***************************************************************************/
int aud_InitLoadSoftware(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int)
{
    U16 retval;
	int x;
	int er;

	x=0;
	er=1;

	while (x<5 && er!=0)
		{
		er=0;
	    if (soundHardwareReset()) er++;
	    if (!er) if (soundLatchTest()) er++;  /* if latch test fails, reset hardware, try again */
		x++;
		}
	if(x>=5) {return(ERROR);}


    if (soundLoadDM(dm_ext,0x0800, 0x37ff)) return(ERROR); 
    if (soundLoadDM(dm_int, 0x3800, 0x39ff)) return(ERROR); 
    if (soundLoadPM(comp, 0x2800, 0x37ff)) return(ERROR);
    if (soundLoadPM(osys,0,0x3ff)) return(ERROR); /* load this one last */

    if(soundPutWord(0x002A)) return(ERROR);
    if(soundGetWord(&retval)) return(ERROR);
    if ((retval) != OPSYS_READY)  {auderr=AUDERR_BAD_ACK;return(ERROR);}
	auderr=AUDERR_AUD_OK;

    aud_clear_all();
    aud_attract_volume();
    return(OK);
}
/***************************************************************************/
/* int aud_AddressOfPartition(int part_num)                                */
/*  get the DSP DM (data memory) address of a partition                    */
/*                                                                         */
/* inputs:  part_num = partition number (1-x) OR '0' for last partition    */
/* returns: partition size , or NULL if an error occured                   */
/***************************************************************************/
int aud_AddressOfPartition(int part_num)
{
if (!snd_partition_index) return(0);	   /* no partitions loaded */
if (part_num>snd_partition_index) return(0);
if (part_num==0) part_num=snd_partition_index;

return(snd_partition_data[part_num-1]);
}
/***************************************************************************/
/* int aud_SizeOfPartition(int part_num)                                   */
/*  get the size of a partition                                            */
/*                                                                         */
/* inputs:  part_num = partition number (1-x) OR '0' for last partition    */
/* returns: partition size , or NULL if an error occured                   */
/***************************************************************************/
int aud_SizeOfPartition(int part_num)
{
if (!snd_partition_index) return(0);	   /* no partitions loaded */
if (part_num>snd_partition_index) return(0);
if (part_num==0) part_num=snd_partition_index;

return(snd_partition_data[part_num] - snd_partition_data[part_num-1]);
}
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
