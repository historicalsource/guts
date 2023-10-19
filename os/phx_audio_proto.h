/******************************************************************************
 *	phx_audio_proto.h
 *
 *		Copyright 1997 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 *	Prototypes for basic audio control routines for the DCS audio subsystem.
 *
 *****************************************************************************/
#include <config.h>
#if !defined(_PHX_AUDIO_PROTO_H_)
#define _PHX_AUDIO_PROTO_H_
/*****************************************************************************/	
/*****************************************************************************/	
/**                                                                         **/
/**  Phx_audio function protos                                              **/
/**                                                                         **/
/*****************************************************************************/
long aud_init(int level);
int aud_InitLoadSoftware(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int)	;
int aud_attract_volume(void);
int aud_game_volume(void);
int aud_put(int code);
int aud_get(U16 *data);
int aud_get_safe(U16 *data);
int aud_put_safe(U16 data);
int aud_mput( unsigned short *buff, int cnt);

void aud_clear_all(void);
void aud_clear_all_banks(void);
int aud_load_bank (U8 *data_buffer);
int aud_load_bank_with_playback (U8 *data_buffer);
int aud_make_partition (int cmd_size,int data_size);
int aud_load_partition (int cmd,U8 *data);
int aud_load_partition_with_playback(int cmd,U8 *data);

int audInitBlockedLoad(U8 *buff,int size);
int audPreloadBlockedBankLoad(U8 *buff,int len);
int audPreloadBlockedBankLoadWithPlayback(U8 *buff,int len);
int audPreloadBlockedPartitionLoad(U16 cmd,U8 *buff,int len);
int audPreloadBlockedPartitionLoadWithPlayback(U16 cmd,U8 *buff,int len) ;
int audLoadBlock(U8 *buffer,int size);

U16 aud_play_sound(U16 sound,U8 vol,U8 pan,U16 priority);
void aud_stop_all_sounds(void);
int aud_bank_data_size(U8 *data_buffer);
int aud_bank_cmd_size(U8 *data_buffer);

int aud_number_of_sounds(void);
int aud_AddressOfPartition(int part_num) ;
int aud_SizeOfPartition(int part_num) ;
void audClearExtendedError(void);
/*****************************************************************************/	
/**                                                                         **/
/**  Phx_audio error codes                                                  **/
/**                                                                         **/
/*****************************************************************************/
#define AUDERR_AUD_OK						 0
#define AUDERR_GET_XFER_TIMEOUT				 1
#define AUDERR_XFER_TIMEOUT					 2
#define AUDERR_CHECKSUM						 3
#define AUDERR_NO_ACK						 4
#define AUDERR_BAD_ACK						 5
#define AUDERR_NO_DATA						 6
#define AUDERR_START_GE_END					 7
#define AUDERR_TOO_MANY_PARTITIONS			 8
#define AUDERR_BANK_LOADED_PARTITION_FAILED	 9
#define AUDERR_COULD_NOT_FIND_PARTITION		10
#define AUDERR_PARTITION_CMD_TOO_BIG		11
#define AUDERR_PARTITION_DATA_TOO_BIG		12
#define AUDERR_WRONG_LOAD_TYPE				13
#define AUDERR_INVALID_DATA					14
#define AUDERR_WRONG_DATA_SIZE				15
#define AUDERR_SAFE_GET_XFER_TIMEOUT		16
#define AUDERR_SAFE_XFER_TIMEOUT			17
#define AUDERR_WARNING_DATA_LEFT_ON_LATCH 	18
#define AUDERR_NEEDS_RESET					19
/*****************************************************************************/
#if defined(SOUND_QUEUED_LATCH)
int adiStopAllSounds( void ); 
int adiReserveTrack(U16 track);
int adiUnreserveTrack(U16 track);
int adiSetTrackPriority( U16 track, U16 priority ); 
int adiStopTrack(U16 track); 
U16 adiPlayReservedSound(U16 SndNum,U8 volume,U8 pan,U8 priority,U16 track);
U16 adiPlaySound(U16 SndNum,U8 volume,U8 pan,U8 priority);
U16 adiSetSoundPriority(U16 cookie,U8 priority);
U16 adiStopSound(U16 cookie);
int adiStopSoundNumber(U16 SndNum);
int adiStopSoundPriority( U16 priority ); 
U16 adiSetSoundVolume(U16 cookie,U8 volume);
U16 adiSetSoundPan(U16 cookie,U8 pan);
U16 adiOSVersion(void); 
U16 adiInterfaceVersion(void); 
void adiLockQueue(void);
void adiUnlockQueue(void);
void adiFlushQueue(void);
void adiSendAllQueue(void);
int adiQueueAmountFree(void);
void adiRegisterCallback(void (*SignalCallback)(U16 signal,U16 data,U32 user,U16 user2,U16 user3));
int adiQueueCommand(U16 *data,U16 outsize,U16 insize,U16 signal,U32 user,U16 user2,U16 user3,U16 user4);
long adiInit(U8 *comp , U8 *osys , U8 *dm_ext , U8 *dm_int); 

void adiOnOffSwitch(int x);
int adiSendDMBlock(U8 *buffer,U16 start,U16 end);
int adiSendPMBlock(U8 *buffer,U16 start,U16 end);

int adiGameVolume(void);
int adiAttractVolume(void); 
int adiNumberOfSounds(void);
int adiAddressOfPartition(int part_num);
int adiSizeOfPartition(int part_num);
void adiClearAll(void);
void adiClearAllBanks(void);
int adiLoadBank(U8 *data_buffer);
int adiLoadBankWithPlayback(U8 *data_buffer);
int adiMakePartition(int cmd_size,int data_size);
int adiLoadPartition(int cmd,U8 *data);
int adiLoadPartitionWithPlayback(int cmd,U8 *data) ;
int adiStopSoundRangePriority(U16 start,U16 end, U16 priority ); 
int adiLoadBlock(U8 *buffer,int size);
int adiPreloadBlockedPartitionLoadWithPlayback(U16 cmd,U8 *buff,int len); 
int adiPreloadBlockedPartitionLoad(U16 cmd,U8 *buff,int len);
int adiPreloadBlockedBankLoadWithPlayback(U8 *buff,int len);
int adiPreloadBlockedBankLoad(U8 *buff,int len);
int adiInitBlockedLoad(U8 *buff,int size);
U16 adiUpdatePlayerEngine(U8 volume,U8 pan,U8 pitch);
U16 adiUpdateDroneEngine(U8 drone,U8 volume,U8 pan,U8 pitch);
U16 adiSetTrackVolume(U16 track,U8 volume);
U16 adiSetTrackPan(U8 track,U8 pan);
int adiPoll(unsigned long time);
#endif
/*****************************************************************************/	
#if defined(SOUND_DCS_STREAMED_AUDIO)
void * adiStartStream(U16 *buff1,U32 size1,U16 *buff2,U32 size2,void (*StreamCallback)(void *cookie,U16 *buffer,U32 size));
void * adiStopStream(void *cookie);
void * adiStreamEOF(void *cookie);
void * adiChangeStream(void *cookie,U16 *buff1,U32 size1,U16 *buff2,U32 size2);
void * adiStreamBlock(void *cookie,U16 *buff,U32 size);
int adiStreamState(void * cookie);
int adiSetStreamVolume(S16 volume);	
#endif
/*****************************************************************************/
/*****************************************************************************/
#endif
