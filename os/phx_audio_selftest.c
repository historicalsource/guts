#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <stdarg.h>
#include <eer_defs.h>
#include <nsprintf.h>
#include <phx_audio_internal.h>
#include <phx_audio_proto.h>

/*****************************************************************************/

extern U8 phx_testbank[], phx_testbank_e[];
extern U8 phx_lr_bank[];
#define SOUND_FIFO (*(VU16*)(AUDIO_FIFO))
int soundFifoTest(void);
/*****************************************************************************/
/***************************************************************************/
int audio_test(const struct menu_d *smp) 	 
{
int row;
int errr;
char emsg[AN_VIS_COL_MAX+1];
U16 retval;
int fail;

row = 5;
fail=0;

*(VU16*)IO_H2SND_CTL |= 5;		/* enable the sound processor and FIFO's */

txt_str(5, row++, "*** Testing Audio Hardware  ***", GRN_PAL);
row++;
prc_delay(1);

if (soundHardwareReset()) 
	{
	txt_str(5, row++, "Hardware Reset, FAILED.", RED_PAL);
	fail++;
    } 
else 
	{
    txt_str(5, row++, "Hardware Reset, passed.", WHT_PAL);
    }

prc_delay(1);

if ((errr=soundLatchTest())) 
	{
	txt_str(5, row++, "Latch test, FAILED.", RED_PAL);
	fail++;
    }
else
	{
	txt_str(5, row++, "Latch test, passed.", WHT_PAL);
    }

prc_delay(1);

if (soundGetEppromRev(&retval)) 
	{
	txt_str(5, row++, "Get epprom revision, TIMED OUT.", RED_PAL);
	fail++;
    }
else
	{
	nsprintf(emsg, sizeof(emsg)-1, "Eprom Revision: V%x.%02x",retval>>8,retval&0x00FF);
	txt_str(5, row++, emsg, WHT_PAL);
    }

prc_delay(1);

if (soundGetAsicRev(&retval)) 
	{
	txt_str(5, row++, "Get SDRC revision, TIMED OUT.", RED_PAL);
	fail++;
    }
else
	{
	nsprintf(emsg, sizeof(emsg)-1, "SDRC Revision: %d",retval);
	txt_str(5, row++, emsg, WHT_PAL);
    }

prc_delay(1);

if (soundGetPMChecksum(&retval)) 
	{
	txt_str(5, row++, "Get Checksum, TIMED OUT.", RED_PAL);
	fail++;
    }
else
	{
	nsprintf(emsg, sizeof(emsg)-1, "PM Checksum: %04x",retval);
	txt_str(5, row++, emsg, WHT_PAL);
    }

prc_delay(1);

if ((errr=soundRamTest(0))) 
	{
	fail++;
	if (errr<4 && errr>0) nsprintf(emsg, sizeof(emsg)-1, "S/RAM chip #%d failed",errr);
	else if (errr==9) nsprintf(emsg, sizeof(emsg)-1, "RAM return code mismatch");
	else if (errr<0) nsprintf(emsg, sizeof(emsg)-1, "Communication error in RAM test #%d.",errr);
	else nsprintf(emsg, sizeof(emsg)-1, "unknown error durring RAM test");

	txt_str(5, row++, emsg, RED_PAL);
    }
else
	{
	txt_str(5, row++, "SRAM test, passed.", WHT_PAL);
    }

prc_delay(1);

if ((errr=soundRamTest(2))) 
	{
	fail++;
	if (errr==9) nsprintf(emsg, sizeof(emsg)-1, "RAM return code mismatch");
	else if (errr<0) nsprintf(emsg, sizeof(emsg)-1, "Communication error in RAM test #%d.",errr);
	else if (errr==4) nsprintf(emsg, sizeof(emsg)-1, "D/RAM bank 0 failed");
	else nsprintf(emsg, sizeof(emsg)-1, "unknown error durring RAM test");
	txt_str(5, row++, emsg, RED_PAL);
    }
else
	{
	txt_str(5, row++, "DRAM bank 0 test, passed.", WHT_PAL);
    }

prc_delay(1);

if (soundEppromBong()) 
	{
	txt_str(5, row++, "Boink, failed.", RED_PAL);
	fail++;
    }
else
	{
	txt_str(5, row++, "Boink, passed.", WHT_PAL);
    }

prc_delay(1);

if ((errr=soundSoftwareReset()))
	{
	txt_str(5, row++, "Software Reset, FAILED.", RED_PAL);
	fail++;
    }
else
	{
	txt_str(5, row++, "Software Reset, passed.", WHT_PAL);
    }

prc_delay(1);

#if defined(SOUND_DCS_STREAMED_AUDIO) || defined(SA_DIAGS)
	if ((errr=soundFifoTest())) 
		{
		nsprintf(emsg, sizeof(emsg)-1, "Communication error in FIFO test #%d.",errr);
		txt_str(5, row++, emsg, RED_PAL);
		fail++;
		}
	else
		{
		txt_str(5, row++, "FIFO test, passed.", WHT_PAL);
		}

soundHardwareReset();
prc_delay(1);
#endif


row++;
if (fail==0) txt_str(5, row++, "All tests passed.", GRN_PAL);

ExitInst(INSTR_PAL);

while ( 1 ) 
	{
	U32 ctls;
	ctls = ctl_read_sw(SW_NEXT);
	if ((ctls&SW_NEXT)) break;
	prc_delay(0);
    }

ctl_read_sw(-1);			/* flush all edges */
return(0);
}

/*****************************************************************************/
#if defined(SOUND_DCS_STREAMED_AUDIO)
/*****************************************************************************/
int soundFifoTest(void) 
{
int retval;
U16 flags;
int x;
U16 test;

if (soundHardwareReset()) return(-1);
if (soundSoftwareReset()) return(-1);

x = SOUND_CONTROL_REGISTER;
x = x & (~((1<<1)+(1<<2)));	  /* hold in reset */
SOUND_CONTROL_REGISTER = x;

prc_delay(1);

x = SOUND_CONTROL_REGISTER;
x = x | (((1<<1)+(1<<2)));	  /* fifo is released and enabled */
SOUND_CONTROL_REGISTER = x;

for (x=0;x<512;x++)
	{
	soundPutWord(0x55FF);
	soundGetWord(&test);
	}

retval=0;
flags=SOUND_PORT_FLAGS& 0x0038;
if (flags!=0x0008) return(1);

SOUND_FIFO=0;

flags=SOUND_PORT_FLAGS& 0x0038;
if (flags!=0x0000) return(2);

for (x=1;x<257;x++)
	{
	SOUND_FIFO=x;
	}

flags=SOUND_PORT_FLAGS& 0x0038;
if (flags!=0x0010) return(4);

for (x=257;x<512;x++)
	{
	SOUND_FIFO=x;
	}
flags=SOUND_PORT_FLAGS& 0x0038;
if (flags!=0x0030) return(5);

for (x=0;x<512;x++)
	{
	soundPutWord(0x55FF); 
	soundGetWord(&test);
	if (x!=test)  return(6);
	}

x = SOUND_CONTROL_REGISTER;
x = x & (~((1<<1)+(1<<2)));	  /* hold in reset */
SOUND_CONTROL_REGISTER = x;
return(soundHardwareReset());
}
/*****************************************************************************/
int audio_fifo_test(const struct menu_d *smp) 
{
U32 ctls;
U16 retval;
U16 x,y;
int row,err;
int goodblks,errorblks;
char emsg[AN_VIS_COL_MAX+1];


row=5;
goodblks=0;
errorblks=0;

txt_str(5, row++, "*** Testing Audio Fifo Hardware  ***", GRN_PAL);
row++;
if (soundHardwareReset()) return(-1);
if (soundSoftwareReset()) return(-1);

x = SOUND_CONTROL_REGISTER;
x = x & (~((1<<1)+(1<<2)));	  /* hold in reset */
SOUND_CONTROL_REGISTER = x;

prc_delay(1);

x = SOUND_CONTROL_REGISTER;
x = x | (((1<<1)+(1<<2)));	  /* fifo is released and enabled */
SOUND_CONTROL_REGISTER = x;

ExitInst(INSTR_PAL);


while(1) 
	{
	for (y=0;y<32;y++)
		{
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;

		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;

		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;

		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		SOUND_FIFO=x++;
		}

	err=0;
	x=x-512;
	for (y=0;y<512;y++)
		{
		soundPutWord(0x55FF); 
/*		soundPutWord(0x6612); */
		soundGetWord(&retval);
		if (x!=retval)  err++;
		x++;
		}

	if (err) errorblks++;
	else goodblks++;

	nsprintf(emsg, sizeof(emsg)-1, "Good FIFO blocks : %d.",goodblks);
	txt_str(5,8, emsg, WHT_PAL);
	nsprintf(emsg, sizeof(emsg)-1, "Bad FIFO blocks  : %d.",errorblks);
	txt_str(5,9, emsg, WHT_PAL);

	if (err) 
		{
		for (y=0;y<512;y++)
			{
			soundPutWord(0x55FF); 
/*			soundPutWord(0x6612); */
			soundGetWord(&retval);
			}
		}
		ctls = ctl_read_sw(SW_NEXT);
		if ((ctls&SW_NEXT)) break;
		prc_delay(1);
     }

ctl_read_sw(-1); /* flush all edges */

return(soundHardwareReset());
}
#endif
/*****************************************************************************/
int audio_speaker_test(const struct menu_d *smp) 
{
int i;
int idx;
U16 temp;

/*  if (!aud_init(0)) return -1;
    if (aud_game_volume()==0) return(-1);
    if ((idx=aud_load_partition(0,phx_lr_bank))==0) return(-1);
*/
    if (!aud_init(0)) return -1;
    if ((idx=aud_load_bank(phx_lr_bank))==0) return(-1);

    temp=eer_gets(EER_AUD_VOL);
    temp=temp&0x00FF;
	if (temp==0)
		{
	    if (soundPutWord(0x55AA)) return(-1);
    	if (soundPutWord(0x40BF)) return(-1);
		}
	else if (aud_game_volume()==0) return(-1);


ExitInst(INSTR_PAL);

i=0;
while (1) 
	{
	U32 ctls;
	ctls = ctl_read_sw(SW_NEXT);

	if ((ctls&SW_NEXT)) break;

	if ((i&0x000F) == 0) txt_clr_wid(1, AN_VIS_ROW/2, AN_VIS_COL-2);

	if ((i&0x001F)==0) 
		{
	    txt_str(4, AN_VIS_ROW/2, "LEFT", WHT_PAL);
	    if (soundPutWord(idx)) return(-1);
	    if (soundPutWord(0xFF00)) return(-1);
	    if (soundPutWord(0x0000)) return(-1);
	    if (soundGetWord(&temp)) return(-1);
		}
	if ((i&0x001F)==0x0010) 
		{
	    txt_str(AN_VIS_COL-1-4-5, AN_VIS_ROW/2, "RIGHT", WHT_PAL);
	    if (soundPutWord(1+idx)) return(-1);
	    if (soundPutWord(0xFFFF)) return(-1);
	    if (soundPutWord(0x0000)) return(-1);
	    if (soundGetWord(&temp)) return(-1);
		}
	prc_delay(3);
	i++;
    }

ctl_read_sw(-1); /* flush all edges */
return(0);
}
/*****************************************************************************/
int audio_sine_test(const struct menu_d *smp) 
{
U32 ctls;
U16 retval;

if (soundHardwareReset()) return(-1);

/* 1K sin on */
soundPutWord(0x00BA);

soundGetWord(&retval);
if (retval!=0xCC05) return(-1);

soundGetWord(&retval);
if (retval!=0x000a) return(-1);

ExitInst(INSTR_PAL);

while(1) 
	{
	ctls = ctl_read_sw(SW_NEXT);
	if ((ctls&SW_NEXT)) break;
	prc_delay(5);
    }

ctl_read_sw(-1); /* flush all edges */

return(soundHardwareReset());
}
/*****************************************************************************/
#if defined(USER_REC_UREC10)
int audio_clear_stats(const struct menu_d *smp) 
{
U32 ctls;
U16 row,x;
char msg[80];
struct audio_stats *stats;
struct audio_stats2 *stats2;
struct audio_stats3 *stats3;

row=3;

nsprintf(msg, sizeof(msg)-1, "Reset all audio stats,");txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "Are you sure? (Reset to exit!)");txt_str(4, row++, msg, WHT_PAL);


ExitInst(INSTR_PAL);

while(1) 
	{
	ctls = ctl_read_sw(SW_NEXT);
	if ((ctls&SW_NEXT)) break;
	prc_delay(5);
    }

ctl_read_sw(-1); /* flush all edges */

stats=(struct audio_stats *)eer_user_rd(USER_REC_UREC10,NULL);
eer_user_free(USER_REC_UREC10);

stats->num_cmds=0;

for (x=0;x<52;x++) stats->error[x]=0;

eer_user_wrt(USER_REC_UREC10);
eer_user_free(USER_REC_UREC10);

stats2=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC11,NULL);
eer_user_free(USER_REC_UREC11);

for (x=0;x<8;x++)
	{
	stats2->cmd1[x]=0;
	stats2->cmd2[x]=0;
	stats2->cmd3[x]=0;
	stats2->cmd4[x]=0;
	}
eer_user_wrt(USER_REC_UREC11);
eer_user_free(USER_REC_UREC11);

stats2=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC12,NULL);
eer_user_free(USER_REC_UREC12);

for (x=0;x<8;x++)
	{
	stats2->cmd1[x]=0;
	stats2->cmd2[x]=0;
	stats2->cmd3[x]=0;
	stats2->cmd4[x]=0;
	}
eer_user_wrt(USER_REC_UREC12);
eer_user_free(USER_REC_UREC12);

stats3=(struct audio_stats3 *)eer_user_rd(USER_REC_UREC13,NULL);
eer_user_free(USER_REC_UREC13);

for (x=0;x<16;x++)
	{
	stats3->flags[x]=0;
	stats3->data[x]=0;
	}
eer_user_wrt(USER_REC_UREC13);
eer_user_free(USER_REC_UREC13);



nsprintf(msg, sizeof(msg)-1, "All values reset to zero");txt_str(4, row++, msg, WHT_PAL);

ExitInst(INSTR_PAL);

while(1) 
	{
	ctls = ctl_read_sw(SW_NEXT);
	if ((ctls&SW_NEXT)) break;
	prc_delay(5);
    }

ctl_read_sw(-1); /* flush all edges */

return(soundHardwareReset());
}
/*****************************************************************************/
int audio_stats(const struct menu_d *smp) 
{
U32 ctls;
U16 row;
U16 error;
char msg[80];
struct audio_stats *stats;
struct audio_stats2 *stats2;
struct audio_stats3 *stats3;
U32 total;

row=3;

stats=(struct audio_stats *)eer_user_rd(USER_REC_UREC10, 0);

nsprintf(msg, sizeof(msg)-1, "%d audio commands have been processed",stats->num_cmds);txt_str(4, row++, msg, WHT_PAL);
total=0;for (error=0;error<42; error++) total=total+stats->error[error];
if (total) {nsprintf(msg, sizeof(msg)-1, "%d errors occured",total);txt_str(4, row++, msg, RED_PAL);}

row++;
for (error=0;error<40; error=error+6)
	{
	nsprintf(msg, sizeof(msg)-1, "%02d:%03d   %02d:%03d   %02d:%03d   %02d:%03d   %02d:%03d   %02d:%03d",error,stats->error[error],error+1,stats->error[error+1],error+2,stats->error[error+2],error+3,stats->error[error+3],error+4,stats->error[error+4],error+5,stats->error[error+5]);
	txt_str(4, row++, msg, WHT_PAL);
	}
row++;

eer_user_free(USER_REC_UREC10);

stats2=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC11, 0);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd1[0],stats2->cmd1[1],stats2->cmd1[2],stats2->cmd1[3],stats2->cmd1[4],stats2->cmd1[5],stats2->cmd1[6],stats2->cmd1[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd2[0],stats2->cmd2[1],stats2->cmd2[2],stats2->cmd2[3],stats2->cmd2[4],stats2->cmd2[5],stats2->cmd2[6],stats2->cmd2[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd3[0],stats2->cmd3[1],stats2->cmd3[2],stats2->cmd3[3],stats2->cmd3[4],stats2->cmd3[5],stats2->cmd3[6],stats2->cmd3[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd4[0],stats2->cmd4[1],stats2->cmd4[2],stats2->cmd4[3],stats2->cmd4[4],stats2->cmd4[5],stats2->cmd4[6],stats2->cmd4[7]);
txt_str(4, row++, msg, WHT_PAL);
eer_user_free(USER_REC_UREC11);

stats2=(struct audio_stats2 *)eer_user_rd(USER_REC_UREC12, 0);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd1[0],stats2->cmd1[1],stats2->cmd1[2],stats2->cmd1[3],stats2->cmd1[4],stats2->cmd1[5],stats2->cmd1[6],stats2->cmd1[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd2[0],stats2->cmd2[1],stats2->cmd2[2],stats2->cmd2[3],stats2->cmd2[4],stats2->cmd2[5],stats2->cmd2[6],stats2->cmd2[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd3[0],stats2->cmd3[1],stats2->cmd3[2],stats2->cmd3[3],stats2->cmd3[4],stats2->cmd3[5],stats2->cmd3[6],stats2->cmd3[7]);
txt_str(4, row++, msg, WHT_PAL);
nsprintf(msg, sizeof(msg)-1, "%04x  %04x  %04x  %04x  %04x  %04x  %04x  %04x",stats2->cmd4[0],stats2->cmd4[1],stats2->cmd4[2],stats2->cmd4[3],stats2->cmd4[4],stats2->cmd4[5],stats2->cmd4[6],stats2->cmd4[7]);
txt_str(4, row++, msg, WHT_PAL);
eer_user_free(USER_REC_UREC12);

row++;
stats3=(struct audio_stats3 *)eer_user_rd(USER_REC_UREC13, 0);

for (error=0;error<16; error=error+4)
	{
	nsprintf(msg, sizeof(msg)-1, "%04x:%04x    %04x:%04x    %04x:%04x    %04x:%04x",stats3->flags[error],stats3->data[error],stats3->flags[error+1],stats3->data[error+1],stats3->flags[error+2],stats3->data[error+2],stats3->flags[error+3],stats3->data[error+3]);
	txt_str(4, row++, msg, WHT_PAL);
	}

eer_user_free(USER_REC_UREC13);

ExitInst(INSTR_PAL);

while(1) 
	{
	ctls = ctl_read_sw(SW_NEXT);
	if ((ctls&SW_NEXT)) break;
	prc_delay(5);
    }

ctl_read_sw(-1); /* flush all edges */

return(soundHardwareReset());
}
#endif
/********************************************************************
 *     Here are a bunch of defines used for volume control:
 *******************************************************************/

/* Display GREEN up to low volume */
#define       LOW_VOLUME    (MAX_VOLUME-((MAX_VOLUME/8)*2)) 
/* Display RED after HIGH volume */
#define       HIGH_VOLUME   (MAX_VOLUME-(MAX_VOLUME/15))

#define VBAR_TEXT_ROW        (6)
#define VBAR_GAME_ROW        (6)
#define VBAR_ATTRACT_ROW     (14)
/*#define VBAR_COL 7
#define VBAR_CHAR "*"
#define VBAR_WIDTH (AN_VIS_COL - 14)*/

#define VBAR_COL 4
#define VBAR_CHAR "*"
#define VBAR_WIDTH (56)
#define DYN_VBAR_ROW	     (AN_VIS_ROW-3-1)

#define MIN_VOLUME   (0)
#define MAX_VOLUME   (255)
#ifndef DEF_VOLUME
#define DEF_VOLUME   ((MAX_VOLUME+MIN_VOLUME)/2)
#endif

/*****************************************************************************/
void ShowVolume(int volume, int row, int bg) 
{
m_int i,j,color;
/*    char msg[5];	 */

for (i=0;i<VBAR_WIDTH;i++)
	{
	j=((MAX_VOLUME-MIN_VOLUME)*(i+1))/VBAR_WIDTH;
	if (volume<j) color = GRY_PAL | bg;
	else if (j<=LOW_VOLUME) color = GRN_PAL | bg;
	else if (j>HIGH_VOLUME) color = RED_PAL | bg;
	else color = YEL_PAL | bg;
	 
	txt_str(VBAR_COL+i,row,VBAR_CHAR,color);
	txt_str(VBAR_COL+i,row+1,VBAR_CHAR,color);
	txt_str(VBAR_COL+i,row+2,VBAR_CHAR,color);

/*	nsprintf(msg, sizeof(msg)-1, "%02d%%",((volume*100)+MAX_VOLUME-1)/MAX_VOLUME);
	txt_str(VBAR_COL-4, row+1, msg, GRY_PAL | bg);*/
	}
}

#if SW_VOLM && SW_VOLP
# define VOLUME_BUTTONS	(SW_VOLM|SW_VOLP)
# define VOLUME_UP	(SW_VOLP)
# define VOLUME_DOWN	(SW_VOLM)
#else
# define VOLUME_BUTTONS (0)
# define VOLUME_UP	(0)
# define VOLUME_DOWN	(0)
#endif

#define ATTRACT_GRANULARITY 5

/*****************************************************************************/
#if defined(SA_DIAGS)
int audio_volume_adj(const struct menu_d *smp) 
{
    U32       ctrls;
    U32       prev_ctrls;
    int      update;
    int original_game_volume;
    int original_attract_volume;
    int game_volume;
    int clr1,clr2;
    int vol;
    int bottom;
	int hold_cntr;

/* Reset the audio processor:  Full reset, load first bank for volume test */
    if (!aud_init(0)) return -1;
    if (aud_load_bank(phx_testbank)==0) return(-1);

/* default to game volume */
    if (aud_game_volume()==0) return(-1);

#if BOOT_FROM_DISK
    bottom = st_insn(AN_VIS_ROW-3,t_msg_ret_menu,t_msg_next,INSTR_PAL);
    bottom = st_insn(bottom,"To set to midrange",t_msg_action,INSTR_PAL);
#else
    bottom = st_insn(AN_VIS_ROW-3,t_msg_save_ret,t_msg_next,INSTR_PAL);
    bottom = st_insn(bottom,t_msg_restore,t_msg_actionh,INSTR_PAL);
#endif
    bottom = st_insn(bottom,"To ADJUST volume,",t_msg_control,INSTR_PAL);


    ctl_autorepeat(JOY_BITS | VOLUME_BUTTONS, 60/4, 60/30); /* Autorepeat after 1/2secs @ 1/15 */

    vol=eer_gets(EER_AUD_VOL);
    original_game_volume=vol&0x00FF;
    original_attract_volume=(vol&0xFF00)>>8;

	prev_ctrls=0;
	hold_cntr=0;

    game_volume=original_game_volume;
    update=1;
    clr1=GRY_PAL;
    clr2=GRY_PAL;

/* Start Tune */
    if (soundPutWord(1)) return(-1);
    if (soundPutWord(0xFF7F)) return(-1);
    if (soundPutWord(0x0000)) return(-1);

    while (1) 
    {			  
	prc_delay0(); 		/* is this neccesarry? */
	ctrls = ctl_read_sw(JOY_BITS | SW_NEXT | VOLUME_BUTTONS);

	if (ctrls & SW_NEXT) /* exit vol adj */
		{
	    soundPutWord(0);
	    soundPutWord(0xFF7F);
	    soundPutWord(0x0000);
	    eer_puts(EER_AUD_VOL,(original_attract_volume<<8)|game_volume);
	    break;
		}

	if ((!(prev_ctrls & SW_ACTION)) && (ctrls & SW_ACTION)) 
		{
		hold_cntr=0;
		}

	if ((prev_ctrls & SW_ACTION) && (ctrls & SW_ACTION)) 
		{
		hold_cntr++;
		if (hold_cntr==57)
			{
		    game_volume = original_game_volume;
		    update = 1;
			}
		}

	if (ctrls & (J_LEFT|VOLUME_DOWN) ) {
	    /* decreasing whichever volume */
	    	{
 			game_volume= ((((game_volume*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )-1)*MAX_VOLUME)/VBAR_WIDTH;
 			if (game_volume < MIN_VOLUME) game_volume = MIN_VOLUME;
 			}
	    update = 1;
		}

	if ( ctrls & (J_RIGHT|VOLUME_UP) ) 
		{
			{
			game_volume= ((((game_volume*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )+1)*MAX_VOLUME)/VBAR_WIDTH;
			if (game_volume > MAX_VOLUME) game_volume = MAX_VOLUME;
			}
	    update = 1;
		}

	if (update) /* display bar and text */
		{
	    update=0;

	    ShowVolume(game_volume,VBAR_GAME_ROW,0);
	    txt_str(VBAR_COL+1, VBAR_GAME_ROW-2,"Game", clr1);

	    soundPutWord(0x55AA);
		soundPutWord((game_volume<<8)|((~game_volume)&0x00FF));
		}

	prev_ctrls=ctrls;
    }
ctl_read_sw(-1);		/* flush all edges */
return(0);
}
#endif
/*****************************************************************************/
int adj_vol(const struct menu_d *smp) {
    U32       ctrls;
    U32       prev_ctrls;
    int      update;
    int original_game_volume;
    int original_attract_volume;
    int game_volume;
    int attract_volume;
    int which_volume;
    int cntr;
    int clr1,clr2;
    int vol;
    int bottom;
	int percent;
	int hold_cntr;
	char strng[24];

/* Reset the audio processor:  Full reset, load first bank for volume test */
    if (!aud_init(0)) return -1;
    if (aud_load_bank(phx_testbank)==0) return(-1);

/* default to game volume */
    if (aud_game_volume()==0) return(-1);

#if BOOT_FROM_DISK
    bottom = st_insn(AN_VIS_ROW-3,t_msg_ret_menu,t_msg_next,INSTR_PAL);
    bottom = st_insn(bottom,"To set to midrange",t_msg_action,INSTR_PAL);
#else
    bottom = st_insn(AN_VIS_ROW-3,t_msg_save_ret,t_msg_next,INSTR_PAL);
    bottom = st_insn(bottom,t_msg_restore,t_msg_actionh,INSTR_PAL);
    bottom = st_insn(bottom,"To SELECT which volume,",t_msg_action,INSTR_PAL);
#endif
    bottom = st_insn(bottom,"To ADJUST volume,",t_msg_control,INSTR_PAL);

    ctl_autorepeat(JOY_BITS | VOLUME_BUTTONS, 60/4, 60/30); /* Autorepeat after 1/2secs @ 1/15 */

    vol=eer_gets(EER_AUD_VOL);
    original_game_volume=vol&0x00FF;
    original_attract_volume=(vol>>8)&0x00FF;

	prev_ctrls=0;
	hold_cntr=0;

    game_volume=original_game_volume;
    attract_volume=original_attract_volume;
	if (game_volume) percent=(((attract_volume*100+50)/game_volume)/ATTRACT_GRANULARITY)*ATTRACT_GRANULARITY;
	else percent=50;
    update=1;
    which_volume=0;
    cntr=0;
    clr1=GRY_PAL;
    clr2=GRY_PAL;

/* Start Tune */
    if (soundPutWord(1)) return(-1);
    if (soundPutWord(0xFF7F)) return(-1);
    if (soundPutWord(0x0000)) return(-1);

    while (1) 
    {			  
	prc_delay0(); 		/* is this neccesarry? */
	ctrls = ctl_read_sw(JOY_BITS | SW_NEXT |/* SW_ACTION |*/ VOLUME_BUTTONS);

	if (ctrls & SW_NEXT) /* exit vol adj */
		{
	    soundPutWord(0);
	    soundPutWord(0xFF7F);
	    soundPutWord(0x0000);
	    eer_puts(EER_AUD_VOL,(attract_volume<<8)|game_volume);
	    break;
		}

	if ((!(prev_ctrls & SW_ACTION)) && (ctrls & SW_ACTION)) 
		{
		hold_cntr=0;
		}

	if ((prev_ctrls & SW_ACTION) && (ctrls & SW_ACTION)) 
		{
		hold_cntr++;
		if (hold_cntr==57)
			{
		    game_volume = original_game_volume;
		    attract_volume = original_attract_volume;
			if (game_volume) percent=(((attract_volume*100+50)/game_volume)/ATTRACT_GRANULARITY)*ATTRACT_GRANULARITY;
			else percent=50;
		    update = 1;
			}
		}

	if ((prev_ctrls & SW_ACTION) && (!(ctrls & SW_ACTION))) 
		{
		if (hold_cntr<57) 
			{
			which_volume=(which_volume+1)&1;
			update = 1;
			}
		}

	if (!(J_UP & VOLUME_UP))
		{
		if (ctrls & (J_DOWN))	{which_volume=1;update = 1;}
		if (ctrls & (J_UP)  )	{which_volume=0;update = 1;}
		}

	if (ctrls & (J_LEFT|VOLUME_DOWN) ) {
	    /* decreasing whichever volume */
	    if ( which_volume ) 
			{
			percent -= ATTRACT_GRANULARITY;
			if (percent<00) percent=00;
			attract_volume=(game_volume*percent)/100; 
			}
	    else
	    	{
 			game_volume= ((((game_volume*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )-1)*MAX_VOLUME)/VBAR_WIDTH;
 			if (game_volume < MIN_VOLUME) game_volume = MIN_VOLUME;
 			attract_volume=(game_volume*percent)/100;
 			}
	    update = 1;
		}

	if ( ctrls & (J_RIGHT|VOLUME_UP) ) 
		{
	    if ( which_volume ) 
			{
 			if (percent!=100)
				{				
				percent += ATTRACT_GRANULARITY;
				if (percent>100) percent=100;
				attract_volume=(game_volume*percent)/100;
				}
			}
	    else
			{
			game_volume= ((((game_volume*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )+1)*MAX_VOLUME)/VBAR_WIDTH;
			if (game_volume > MAX_VOLUME) game_volume = MAX_VOLUME;
			attract_volume=(game_volume*percent)/100;
			}
	    update = 1;
		}

	if ((cntr&0x07)==0)  /* flash active volume text */
		{
	    if (cntr&0x08) 
	    	{
			if (which_volume) {clr1=GRY_PAL;clr2=WHT_PAL;update=1;}
			else {clr1=WHT_PAL;clr2=GRY_PAL;update=1;}
	    	}
	    else 
	    	{clr1=GRY_PAL;clr2=GRY_PAL;update=1;}
		}
	cntr++;

	if (update) /* display bar and text */
		{
	    update=0;

	    ShowVolume(game_volume,VBAR_GAME_ROW,0);
	    ShowVolume(attract_volume,VBAR_ATTRACT_ROW,0);
									   
	    txt_str(VBAR_COL+1, VBAR_GAME_ROW-2,"Game", clr1);

	    nsprintf(strng, sizeof(strng)-1,"Attract (%02d%% of Game) ",percent);
	    txt_str(VBAR_COL+1, VBAR_ATTRACT_ROW-2,strng, clr2);
	    soundPutWord(0x55AA);
	    if (which_volume) soundPutWord((attract_volume<<8)|((~attract_volume)&0x00FF));
	    else soundPutWord((game_volume<<8)|((~game_volume)&0x00FF));
		}

	prev_ctrls=ctrls;
    }
ctl_read_sw(-1);		/* flush all edges */
return(0);
}

#define DYN_INIT_AUTOREP	0x01
#define DYN_INIT_DISPLAY_UP	0x02

/*****************************************************************************/
int aud_dyn_adjvol(int which_volume) {
    static int off_time, init;
    U32 ctrls;
    int att_vol, game_vol, volume,percent;
#if !defined(SOUND_DCS_STREAMED_AUDIO)
    char strng[AN_VIS_COL_MAX];
#endif

    if (!init) 
    	{
		ctl_autorepeat(VOLUME_BUTTONS, 15, 15); /* Autorepeat after 15 frames, @ 15 frames/key */
		init |= DYN_INIT_AUTOREP;	/* signal we've done this already */
    	}

    ctrls = ctl_read_sw(VOLUME_BUTTONS)&VOLUME_BUTTONS;
    if (!ctrls) 
    	{
		if (off_time > 60) return 0;		/* nothing to do */
		if (off_time == 60 && (init&DYN_INIT_DISPLAY_UP) ) 
			{
	    	txt_clr_wid(VBAR_COL,   DYN_VBAR_ROW-1, VBAR_WIDTH+1);
		    txt_clr_wid(VBAR_COL,   DYN_VBAR_ROW+0, VBAR_WIDTH+1);
		    txt_clr_wid(VBAR_COL-4, DYN_VBAR_ROW+1, VBAR_WIDTH+5);
		    txt_clr_wid(VBAR_COL,   DYN_VBAR_ROW+2, VBAR_WIDTH+1);
		    init &= ~(DYN_INIT_DISPLAY_UP);	/* signal the display is off */
			}
		++off_time;
		return 0;
    	}

    off_time = 0;
    volume = eer_gets(EER_AUD_VOL);
    att_vol = (volume>>8)&0xFF;
    game_vol = volume&0xFF;    
    if (game_vol) percent=(((att_vol*100+(game_vol-1))/game_vol)/ATTRACT_GRANULARITY)*ATTRACT_GRANULARITY;
    else percent=75;

    if ( (init&DYN_INIT_DISPLAY_UP) )	/* Adjust volume only after display is up */
		{
    	if ((ctrls & VOLUME_DOWN)) 
 		   	{
	    	/* decreasing volume */
	    	if ( which_volume ) 
				{
#if defined(SOUND_DCS_STREAMED_AUDIO)
 				att_vol = ((((att_vol *VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )-1)*MAX_VOLUME)/VBAR_WIDTH;
 				if (att_vol < MIN_VOLUME) att_vol = MIN_VOLUME;
#else
				percent -= ATTRACT_GRANULARITY;
				if (percent<00) percent=00;
				att_vol=(game_vol*percent)/100; 
#endif
				}
	    	else
	    		{
 				game_vol= ((((game_vol*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )-1)*MAX_VOLUME)/VBAR_WIDTH;
 				if (game_vol < MIN_VOLUME) game_vol = MIN_VOLUME;
#if !defined(SOUND_DCS_STREAMED_AUDIO)
 				att_vol=(game_vol*percent)/100;
#endif
 				}
	    	}
	    if ((ctrls & VOLUME_UP)) 
    		{
	    	if ( which_volume ) 
				{
#if defined(SOUND_DCS_STREAMED_AUDIO)
				att_vol = (((( att_vol*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )+1)*MAX_VOLUME)/VBAR_WIDTH;
				if (att_vol > MAX_VOLUME) att_vol = MAX_VOLUME;
#else
 				if (percent<100)
					{				
					percent += ATTRACT_GRANULARITY;
					if (percent>100) percent=100;
					att_vol=(game_vol*percent)/100;
					}
#endif
				}
		    else
				{
				game_vol= ((((game_vol*VBAR_WIDTH+(VBAR_WIDTH-1))/MAX_VOLUME )+1)*MAX_VOLUME)/VBAR_WIDTH;
				if (game_vol > MAX_VOLUME) game_vol = MAX_VOLUME;
#if !defined(SOUND_DCS_STREAMED_AUDIO)
				att_vol=(game_vol*percent)/100;
#endif
				}
			}

	    if (game_vol<MIN_VOLUME) game_vol=MIN_VOLUME; 
    	if (game_vol>MAX_VOLUME) game_vol=MAX_VOLUME; 

	    if (att_vol<MIN_VOLUME) att_vol=MIN_VOLUME; 
#if !defined(SOUND_DCS_STREAMED_AUDIO)
    	if (att_vol>game_vol)   att_vol=game_vol; 
#else
    	if (att_vol>MAX_VOLUME)   att_vol=MAX_VOLUME; 
#endif
	    volume = (att_vol << 8) | game_vol;
	    eer_puts(EER_AUD_VOL, volume);
		}
	if (which_volume) {
#if defined(SOUND_DCS_STREAMED_AUDIO)
    	txt_str(VBAR_COL, DYN_VBAR_ROW-1, "ATTRACT VOLUME", WHT_PALB);
	  	ShowVolume(att_vol, DYN_VBAR_ROW, BGBIT);
#else
	    nsprintf(strng, sizeof(strng)-1,"ATTRACT VOLUME (%02d%% of Game) ",percent);
		txt_str(VBAR_COL, DYN_VBAR_ROW-1,strng, WHT_PALB);
	 	ShowVolume(att_vol, DYN_VBAR_ROW, BGBIT);
#endif
		}
	else {
    	txt_str(VBAR_COL, DYN_VBAR_ROW-1, "GAME VOLUME", WHT_PALB);
	  	ShowVolume(game_vol, DYN_VBAR_ROW, BGBIT);
		}
    init |= DYN_INIT_DISPLAY_UP;		/* next time we can adjust volume */
    if (which_volume) 
    	{
#if defined(SOUND_QUEUED_LATCH)
		adiAttractVolume();
#else
		aud_attract_volume();
#endif
    	}
    else
    	{
#if defined(SOUND_QUEUED_LATCH)
		adiGameVolume();
#else
		aud_game_volume();
#endif
    	}
    return 0;
}

/*****************************************************************************/
int sound_cb (const struct menu_d *smp);

static const struct menu_d snd_menu[] = 
{
    {"SOUND TESTS",  sound_cb},
#if SA_DIAGS
    {"Audio Volume Adjustment", audio_volume_adj},
#endif
    {"Audio Speaker Test", audio_speaker_test},
    {"Audio Hardware test",  audio_test},
#if defined(USER_REC_UREC10)
    {"Audio Debugging stats",  audio_stats},
    {"Clear Debugging stats",  audio_clear_stats},
#endif
#if defined(SOUND_DCS_STREAMED_AUDIO)
    {"?Fifo Test",  audio_fifo_test},
#endif
    {"?LOUD 1k Hz sine test",  audio_sine_test},
    {0, 0} 
};
/*****************************************************************************/

int snd_test ( const struct menu_d *smp ) 
{
int status;
ctl_autorepeat(JOY_BITS,30,15);       /* Autorepeat after 1/2secs @ 1/4 */
status = st_menu(snd_menu,sizeof(snd_menu[0]),MNORMAL_PAL,0);
 
if (!aud_init(0)) return -1;
return status;
}

static int snd_status;

/*****************************************************************************/
int sound_cb (const struct menu_d *smp) 
{
if ( smp ) 
	{
	/* after first call, just watch for SW_NEXT, and clean up if seen */

	if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) 
		{
	    prc_delay0();
	    aud_init(0);
	    if ( snd_status == 0 ) 
	    	{
			ctl_read_sw(-1);
			return -1;
	    	}
		}
    } 
return 0;
}
/*****************************************************************************/
