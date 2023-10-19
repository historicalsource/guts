/******************************************************************************
 *	aud_internal.h
 *
 *		Copyright 1997 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 *	Prototypes for basic audio control routines for the DCS audio subsystem.
 *
 *****************************************************************************/
#if !defined(_PHX_AUDIO_INTERNAL_H_)
#define _PHX_AUDIO_INTERNAL_H_
/*****************************************************************************/

#if defined(SOUND_DEBUG)
  #define ADI_ASSERT(x) if(!(x)) while (1==1) ;
#else
  #define ADI_ASSERT(x) ;
#endif
/***************************************************************************/
/* IOASIC_BASE is defined in ../inc/config.h                               */
/***************************************************************************/
#define IO_ASIC_CONTROL_REGISTER (*((VU16*)IO_MAIN_CTL))
#define SOUND_CONTROL_REGISTER   (*((VU16*)IO_H2SND_CTL))
#define SOUND_PORT_GET_DATA      (*((VU16*)IO_SND2H_DTA))
#define SOUND_PORT_FLAGS         (*((VU16*)IO_SND_STS))
#define SOUND_PORT_PUT_DATA      (*((VU16*)IO_H2SND_DTA))
/***************************************************************************/
#define MIN(a,b) {if (a<b) return(a); else return(b);}
#define MAX(a,b) {if (a>b) return(a); else return(b);}
#define STATIC static
#define OK 0
#define ERROR 1
#define NULL 0
/***************************************************************************/
#define SOUND_PORT_SEND_READY 0x80
#define SOUND_PORT_DATA_READY 0x40
/*****************************************************************************/
struct aud_queue_instance
	{
	int data_size;
	int data_addr;
	int cmd_size;
	int cmd_addr;
	int checksum;
	int xfer;
	};
/*****************************************************************************/
int soundGetWord(U16 *data);
int soundPutWord(unsigned short data);
int soundLatchTest(void); 
int soundHardwareReset(void); 
int soundRamTest(int code);
int soundGetAsicRev(U16 *retval); 
int soundGetPMChecksum(U16 *retval); 
int soundGetEppromRev(U16 *retval); 
int soundInternalPMChecksum(U16 *retval); 
int soundEppromBong(void);  
int soundSoftwareReset(void); 
int soundCustomSoftwareReset(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int);
/*****************************************************************************/
#if defined(SOUND_DCS_STREAMED_AUDIO)
void _adiStreamInit(void);
extern volatile U16 adi_DCS_EMPTY_BLOCKS;
extern volatile U16 adi_STREAM_ON;
extern volatile U16 adi_DCS_FULL_BLOCKS;
#endif

struct audio_stats
	{
	U32 num_cmds;
	U8 error[52];
	};

struct audio_stats2
	{
	U16 cmd1[8];
	U16 cmd2[8];
	U16 cmd3[8];
	U16 cmd4[8];
	};
struct audio_stats3
	{
	U16 flags[16];
	U16 data[16];
	};
#endif

