/*                     
 *     snd_test.c
 *    Primary Author:   Mike Albaugh
 *    Some comments by: Chuck Peplinski
 *
 *  This file controlls the self test routines for audio.  
 *  Mainly, this is the volume adjust screen, ( adj_vol() )
 *  and the "sounds by numbers" screen.  ( snd_by_num() )
 *
 *            Copyright 1991-1995 Atari Games.
 *     Unauthorized reproduction, adaptation, distribution, performance or 
 *     display of this computer program or the associated audiovisual work
 *     is strictly prohibited.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: snd_test.c,v 1.30 1997/09/05 02:03:32 forrest Exp $";
#endif
#define BRAVE_NEW_WORLD (1)      /* part of a system wide changeover to new names */
#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <eer_defs.h>
#include <string.h>

#if !(HAS_CAGE)
#define aud_cmd(arg) ((arg)|0x8000)
#endif

#if HAS_CAGE
#define  HAS_MOS (1)
/* Following may be bogus (CAGE may not always have QUAD), but
 * it's less bogus than the former assumption that _everything_
 * supported QUAD. (4FEB97, MEA)
 */
#ifndef HAS_QUAD
#define  HAS_QUAD  (1)
#endif
#else
#ifdef   COJAG_GAME
#define  HAS_MOS (1)
#define VOLUME_UP ( (cojag_rev >= 2) && !(TEST &(1<<5)) )
#define VOLUME_DOWN ( (cojag_rev >= 2) && !(TEST &(1<<4)) )
extern int cojag_rev;
#else
#define  HAS_MOS (0)
#endif
#endif

#ifndef VOLUME_UP
#if HAS_VOLUME_BUTTONS
/* Compatibility hack for older games which always put
 * volume buttons in VBLANK "switch" location.
 */
#define VOLUME_UP ( !(VBLANK &(1<<B_VOLUP) )
#define VOLUME_DOWN ( !(VBLANK &(1<<B_VOLDN) )
#endif	/* HAS_VOLUME_BUTTONS */
#endif /* ndef'd VOLUME_UP */

#define  STATIC static      /* If we are debugging, we define STATIC as blank space */

/* 
 *    Set up some defines for audio command values:
 */
 
#ifndef  ST_S_AUDTST
#define  ST_S_AUDTST  (0x8003)
#endif

#ifndef  ST_S_STOP
#define  ST_S_STOP    (0x8000)
#endif


#if      HAS_MOS
#define  DATA           unsigned short
#define  MAX_AUD_TRIES  (30)          /* 1/2 second */
#define  S_PJCT_VER     (0x8009)
#define  S_ERRORS       (0x800A)
#define  S_LOAD_BANK    (0x8014)
#define  S_UNLOAD_BANK  (0x8015)
#define  S_REPORT_BANK  (0x8016)
#define  START_SOUND    ST_S_AUDTST
#define  START_INDEX    0x8000
#define  START_TIME     (100)
#ifndef  VERS_COL
#if (AN_VIS_COL > 40 )
#define  VERS_COL       (3)
#else
#define  VERS_COL       (1)
#endif /* Wide/Narrow */
#endif /* VERS_COL defined */

#else    /* SA series */
#define  DATA           unsigned char
#define  MAX_AUD_TRIES  (3 * 60)      /* 3 seconds */
#define  START_SOUND    ST_S_YTST
#define  START_INDEX    0
#define  START_TIME     (9*60)
#endif


/*     Try to set a reasonable default for the location and size of the
 *     Attract-mode volume ratio. This will only be used if EER_AUD_ATRVOL
 *     is not defined.
 */
#ifndef AUD_AV_MSK
#ifdef EER_GUTS_OPT
#define AUD_AV_MSK (3)
#else
#define AUD_AV_MSK (0)
#endif /* EER_GUTS_OPT defined */
#endif /* AUD_AV_MSK defined */
#ifndef AUD_AV_SHF
#define AUD_AV_SHF (0)
#endif

STATIC void VolumeSetError();
/*
 *     error strings
 */
static const char * const snderr[] =
{
#if HAS_MOS
       "SOUND ROM ERROR",
       "INTERNAL ERRORS",
       "UNDEFINED ERRORS"
#else
       "MAIN LINE ERROR",
       "MUSIC CHIP TIME OUT",
       "SOUND CPU INTERRUPT ERROR",
       "SOUND CPU RAM ERROR",
       "SOUND CPU RAM ERROR",
       "SOUND CPU ROM ERROR",
       "SOUND CPU ROM ERROR",
       "SOUND CPU ROM ERROR"
#endif
};
#define MAX_ERR_BITS (sizeof(snderr)/sizeof(snderr[0]))
/*
 *     sound strings
 */

#define       SNDCOL        2
#define       SNDROW        17
#define       SOUNDCOL      (SNDCOL + 5)
#define       SOUNDROW      SNDROW
#define       DECCOL        (SNDCOL + 5 + 6)
#define       DECROW        SNDROW
#define       HEXCOL        (SNDCOL + 5 + 6 + 5)
#define       HEXROW        SNDROW
#define       LABCOL        (SNDCOL + 5 + 6 + 5 + 4)
#define       LABROW        SNDROW

#define       INSROW        (SNDROW + 1)

/* SNDLST has strings for auto-tests (can't be called!)        */

#define       ENDSNDLST     (sizeof(sndlst)/sizeof(sndlst[0]))

static const char sndcnt[] = "NUMBER OF SOUNDS:     ";

#define       BORDERROW     (AN_VIS_ROW - 6)     /* Up six from the bot      */
#define       VCOL          ((AN_VIS_COL+1 - sizeof(sndcnt))>>1)
#define       VROW          (BORDERROW - 2)
#define       SCOL          (VCOL + 18)

/**************************************************************
 *  cage_get() might be better called "mos_get."  
 *  It sends a "query" type of a command to the MOS system
 *  and waits around for a response (or time out).
 */
#if HAS_MOS
STATIC int cage_get( int cmd )
{
    int status;
    int timer;
    timer = 42;
    while ( (status = aud_q_get()) >= 0 ) prc_delay0();
    do {
       prc_delay0();
       status = aud_put(cmd);
    } while ( status == 0 && --timer >= 0 );
    if ( status == 0 ) return -1;
    timer = 42;
    do {
       status = aud_q_get();
       if ( status >= 0 ) break;
       prc_delay0();
    } while ( status < 0 && --timer >= 0);
    return status;
}
#endif

STATIC int show_sound_status PARMS(( int  status, int count ));

/********************************************************************
 * ResetSoundProc() is effectively aud_init() with pretty wrappers.
 * The level parameter is passed directly to audi_init().
 */
STATIC int
ResetSoundProc(level)
int    level;
{
    long      status;
    char      *msg = "Resetting sounds, Please wait";

    /* Attempt to reset the sound process[or]. aud_init() should
     *  take care of the details and return a long with "number of
     *  sounds" in the top 16 bits and "status" (encoded as per old
     * 6502 status) in bottom 16 bits. A return of < 0 means a failure
     * while a return of 0 means the user got impatient and hit "NEXT".
     */
    while (1) {
       txt_str(2,4,msg,WHT_PAL);
       status = aud_init(level);

       if ( status > 0 ) 
         break;
       else if ( status == 0 ) {
           /* bailed at user request */
           aud_init(-1);    /* finished with sounds */
           return 0;
       } else {
           /* timeout or other wedge-up
            */
           txt_clr_str(2,4,msg,WHT_PAL);
           msg = "RE-TRYING SOUND RESET";
           txt_str(2,6,"SOUND PROCESSOR NOT RESPONDING",RED_PAL);
           continue;
       }
    } /* end while (1) */
    txt_clr_str(2,6,"SOUND PROCESSOR NOT RESPONDING",RED_PAL);
    txt_clr_str(2,4,msg,WHT_PAL);
    return (status);
}

/********************************************************************/
STATIC int
show_sound_status(status, count)
int    status,count;
{
    m_int     timer;
    m_int     i;

    /* we want to report in gory detail...
     */
    txt_str(VCOL,VROW+1,"SOUND CPU STATUS: ",VERSION_PAL);
    if ( status == 0 ) txt_cstr("GOOD",VERSION_PAL);
    else {  /* detailed debug report: Not used recently */
       m_int row = 7;
       txt_chexnum(status,2,RJ_ZF,VERSION_PAL);
       for ( i = 0 ; i < MAX_ERR_BITS ; ++i ) {
           if ( status & (1 << i) ) {
              /* each bit has a meaning...
               */
              txt_str(2,row++,snderr[i],RED_PAL);
           }
       } /* end for */
       txt_str(2,row,t_msg_next,WHT_PAL);
       txt_cstr(" To proceed",WHT_PAL);
       for ( timer = 42*60 ; timer > 0 ; --timer ) {
           if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) break;
           prc_delay0();
       }
       if ( timer <= 0 ) return -1;
       while ( row >= 7 ) txt_clr_wid(2,row--,AN_VIS_COL-4);
    } /* end else (bad status) */

#if HAS_MOS
    if ( (i = aud_cmd(ST_C_MOS_VER)) != 0 ) {
       txt_str(VERS_COL,VROW+2,"MOS VERSION ",MNORMAL_PAL);
       i = cage_get(i);
       txt_chexnum((i >> 8),2,LJ_NF,MNORMAL_PAL);
       txt_cstr(".",MNORMAL_PAL);
       txt_chexnum((i & 0xFF),2,RJ_ZF,MNORMAL_PAL);
       if ( ( i = aud_cmd(ST_C_PJCT_VER) ) != 0 ) {
           i = cage_get(i);
           if ( i & 0x8000 ) {
              /* Kluge to deal with "-1" returned as unsigned short,
                * which shows up as 65535.
               */
              txt_cstr(" (NO GAME)",ERROR_PAL);
           } else {
              txt_cstr(" PROJECT VERSION ",MNORMAL_PAL);
              txt_cdecnum(i,3,LJ_NF,MNORMAL_PAL); 
           }
       }  
    }
#if 0    /* print error count, if desired (not for CoJag) */
    if ( (i = aud_cmd(ST_C_ERR_CNT)) != 0
       && (status = cage_get(i)) > 0 ) {
       txt_str(VERS_COL,VROW+3,"MOS ERRORS ",RED_PAL);
       txt_cdecnum(status,4,LJ_NF,RED_PAL);
    }
#endif

#endif
    return (count);                /* > zero                   */
}


/********************************************************************
 *     Here are a bunch of defines used for volume control:
 */
#ifndef INC_VOLUME
#define INC_VOLUME   (8)
#endif
#ifndef MIN_VOLUME
#define MIN_VOLUME   (0)
#endif
#ifndef MAX_VOLUME
#define MAX_VOLUME   (32)
#endif
#ifndef DEF_VOLUME
#define DEF_VOLUME   ((MAX_VOLUME+MIN_VOLUME)/2)
#endif
       /* Display GREEN up to low volume */
#ifndef LOW_VOLUME
#define       LOW_VOLUME    (DEF_VOLUME-(MAX_VOLUME/8)) 
#endif
#ifndef HIGH_VOLUME
       /* Display RED after HIGH volume */
#define       HIGH_VOLUME   (DEF_VOLUME+(MAX_VOLUME/8))
#endif

#define VBAR_ROW      (10)
#define VBAR_AROW     (14)
#define VBAR_COL      ((AN_VIS_COL-32)>>1)
#define VBAR_NUMROW   (8)
#define VBAR_ANUMROW  (19)

/*     ShowVolume() Show volume and draw the current volume 'bar'
 */
STATIC void
ShowVolume(volume,row,nerd_row)
int volume;
int row,nerd_row;
{
    m_int i,j,color;

    if ( volume < MIN_VOLUME ) volume = MIN_VOLUME;
    if ( volume > MAX_VOLUME ) volume = MAX_VOLUME;
    if ( nerd_row >= 0 ) {
       /* display volume as a number      */
       txt_decnum(25,nerd_row,volume,3,LJ_BF,WHT_PALB + AN_BIG_SET);
    }
    color = GRN_PAL;

    for(i = 0, j=0; j <= MAX_VOLUME ; i++, j += INC_VOLUME) 
    {
       if ( j > volume ) color = GRY_PAL;
       else if ( j > HIGH_VOLUME) color = RED_PAL;
       else if ( j > LOW_VOLUME) color = YEL_PAL;
       txt_stamp(VBAR_COL+i,row,AN_VOL_BAR,color);
       txt_stamp(VBAR_COL+i,row+1,AN_VOL_BAR,color);
       txt_stamp(VBAR_COL+i,row+2,AN_VOL_BAR,color);
    }
}

static const char bar_row[2] = { VBAR_ROW, VBAR_AROW };
static const char label_row[2] = { VBAR_ROW-1, VBAR_AROW+3 };
static const char * const which_name[2] = { "GAME", "ATTRACT" };

struct vol_state {
    U32 prev_rtc;
    U16 volume;		/* Game (or only) volume */
    U16 old_volume;	/* Used in selftest screen for "get me back" */
    U16 atr_volume;	/* Attract volume, either derived or set */
    U16 old_atr_volume;
/* Following (which_volume) is more complex than it may seem
 * necessary. We have games with only a single volume, games
 * whose attract volume is a ratio of the game volume, and
 * games with a separate attract volume. In an attempt to
 * accomodate use of volume-buttons during game-play, and
 * reduce the amount of compile-time (versus run-time)
 * configuration, we keep track of the possibilities on the
 * fly. The bottom two bits of which_volume indicate
 * one of Game, Attract_ratio, Attract is being diddled,
 * while the next two indicate which of (Attract_ratio/value)
 * (if either) are allowed.
 */
    U8 which_volume;
#define WV_GAME (0)	/* setting game volume */
#define WV_RATIO (1)	/* setting attract ratio */
#define WV_ATTR (2)	/* setting attract volume */
#define WV_CURR(x) ((x)&3) /* What we are currently setting */
#define WV_CANSET(x) (((x)>>2)&3) /* What we are allowed to set */
#define WV_ALLOW(w) ((w)<<2)	/* For symbolic setting */
#define WV_SET(x,w) (((x)&0xC)|((w)&3))
    U8 atrv;		/* Attract volume ratio */
    U8 old_atrv;	/* old ratio */
    U8 idle;
#ifdef VOLUME_UP
    U8 vup_tim,vdn_tim,old_btns;
#endif
};

#define B_VUP (1)
#define B_VDN (2)
#define VOL_IDLE (240)	/* Initialize count for idle-timer, 4 seconds */
#define AR_FIRST (30)	/* First auto_repeat at 30 frames */
#define AR_SUBS (15)	/* Subsequent auto_repeat every 15 frames */

static int vol_fiddle( struct vol_state *vsp, U32 ctls )
{
    int update = 0;
    int elapsed;

    elapsed = eer_rtc - vsp->prev_rtc;
    vsp->prev_rtc = eer_rtc;

    if (ctls & SW_ACTION) {
	int which = vsp->which_volume;
	vsp->volume = vsp->old_volume;
	if ( WV_CANSET(which) == WV_RATIO ) {
	    /* restore the ratio, then update the
	     * full attract value.
	     */
	    vsp->atrv = vsp->old_atrv;
#if AUD_AV_MSK
	    vsp->atr_volume = ((vsp->volume)*(vsp->atrv))/AUD_AV_MSK;
#endif
	} else {
	    vsp->atr_volume = vsp->old_atr_volume;
	}
	vsp->which_volume = WV_SET(which,WV_GAME);
	update = 1;
    }

#ifdef VOLUME_UP
    /* We have some way of reading the Volume-control
     * Buttons. Debounce and auto-repeat them,
     * then fake LEFT/RIGHT joystick motion for
     * following code.
     */
    if ( VOLUME_UP ) {
	if ( !(vsp->old_btns & B_VUP) ) {
	    /* Leading edge, note and set autorepeat to
	     * "first repeat"
	     */
	    ctls |= J_RIGHT;
	    vsp->vup_tim = AR_FIRST;
	} else if ( vsp->vup_tim <= elapsed ) {
	    /* Timed out auto-repeat. Fake edge and
	     * re-load "subsequent repeat" time.
	     */
	    ctls |= J_RIGHT;
	    vsp->vup_tim = AR_SUBS;
	} else vsp->vup_tim -= elapsed;
	vsp->old_btns |= B_VUP;
	vsp->idle = VOL_IDLE;
    } else {
	vsp->vup_tim = 0;
	vsp->old_btns &= ~B_VUP;
    }
    if ( VOLUME_DOWN ) {
	if ( !(vsp->old_btns & B_VDN) ) {
	    /* Leading edge, note and set autorepeat to
	     * "first repeat"
	     */
	    ctls |= J_LEFT;
	    vsp->vdn_tim = AR_FIRST;
	} else if ( vsp->vdn_tim <= elapsed ) {
	    /* Timed out auto-repeat. Fake edge and
	     * re-load "subsequent repeat" time.
	     */
	    ctls |= J_LEFT;
	    vsp->vdn_tim = AR_SUBS;
	} else vsp->vdn_tim -= elapsed;
 	vsp->old_btns |= B_VDN;
	vsp->idle = VOL_IDLE;
    } else {
	vsp->vdn_tim = 0;
	vsp->old_btns &= ~B_VDN;
    }
#endif
    if ( ctls & (J_DOWN|J_UP) ) {
	/* selecting game/attract volume */
	int which = WV_GAME;
	if ( ctls & J_DOWN ) which = WV_CANSET(vsp->which_volume);
	vsp->which_volume = WV_SET(vsp->which_volume,which);
	update = 1;
    }
    else if ( ctls & (J_LEFT) ) {
	/* decreasing whichever volume */
	int which;
	which = WV_CURR(vsp->which_volume);
	switch (which) {
	    case WV_RATIO:
		/* attract ratio */
#if AUD_AV_MSK
		if ( vsp->atrv > 0 ) --vsp->atrv;
		break;
#endif
	    case WV_ATTR:
		/* Attract volume */
		if (vsp->atr_volume >= MIN_VOLUME+INC_VOLUME) {
		    vsp->atr_volume -= INC_VOLUME;
		} else {
		  vsp->atr_volume = MIN_VOLUME;
		}
		break;
	    case WV_GAME:
		/* game volume */
		if (vsp->volume >= MIN_VOLUME+INC_VOLUME) {
		    vsp->volume -= INC_VOLUME;
		} else {
		  vsp->volume = MIN_VOLUME;
		}
		if ( vsp->volume < vsp->atr_volume ) {
		    /* Limit attract volume to no higher than
		     * game volume, in case it is separately set.
		     */
		    vsp->atr_volume = vsp->volume;
		}
	}
	update = 1;
    }
    else if ( ctls & (J_RIGHT) ) {
	/* increasing whichever volume */
	int which;
	which = WV_CURR(vsp->which_volume);
	switch (which) {
	    case WV_RATIO:
		/* attract ratio */
#if AUD_AV_MSK
		if ( vsp->atrv < AUD_AV_MSK ) ++vsp->atrv;
		break;
#endif
	    case WV_ATTR:
		/* Attract volume */
		if (vsp->atr_volume <= MAX_VOLUME-INC_VOLUME) {
		    vsp->atr_volume += INC_VOLUME;
		} else {
		  vsp->atr_volume = MAX_VOLUME;
		}
		if ( vsp->volume < vsp->atr_volume ) {
		    /* Bump game volume to keep it no
		     * lower than attract volume.
		     */
		    vsp->volume = vsp->atr_volume;
		}
		break;
	    case WV_GAME:
		/* game volume */
		if (vsp->volume <= MAX_VOLUME-INC_VOLUME) {
		    vsp->volume += INC_VOLUME;
		} else {
		  vsp->volume = MAX_VOLUME;
		}
	}
	update = 1;
    }
    if ( update ) {

       /*  This is a little more complicated now. On CAGE boards
        *  which support separate game and attract volume, we need
        *  to make sure we are changing the right one, and that
        *  we are using the right one.
        */
	vsp->idle = VOL_IDLE;

#if AUD_AV_MSK
	if ( WV_CANSET(vsp->which_volume) == WV_RATIO ) {
	    /* If the attract volume is a ratio, update
	     * the "fully decoded" version to track
	     * changes in ratio or game volume.
	     */
	    vsp->atr_volume = ((vsp->volume)*(vsp->atrv))/AUD_AV_MSK;
	}
#endif
	if ( aud_cmd(ST_C_ATR_VOL) != 0
	  && WV_CURR(vsp->which_volume) != WV_GAME ) {
           /* we want to change attract volume, and CAGE
            * keeps track of it.
            */
           U16 volbuf[2];
           volbuf[0] = aud_cmd(ST_C_ATR_VOL);
           volbuf[1] = vsp->atr_volume;
           if ( aud_mput(volbuf,2) < 0 ) {
              return -1;
           }
           if ( aud_cmd(ST_C_ATR_MODE) ) aud_put(aud_cmd(ST_C_ATR_MODE));
	} else {
           /* either we _want_ to set game volume, or it's the
            * only volume we have
            */
	    int status;
	    int which;
	    int volume;

	    which = WV_CURR(vsp->which_volume);
	    volume = vsp->atr_volume;
	    if ( which == WV_GAME ) volume = vsp->volume;

           status = aud_setvol( volume );

           if ( status < 0) /* error */
           {
              return status;
           }
           if ( which != WV_GAME && aud_cmd(ST_C_ATR_MODE) ) {
              aud_put(aud_cmd(ST_C_ATR_MODE));
           }
           if ( which == WV_GAME && aud_cmd(ST_C_GAME_MODE) ) {
              aud_put(aud_cmd(ST_C_GAME_MODE));
           }
	}
    } else {
	/* Keep track of idle time */
	int tmp;
	tmp = vsp->idle - elapsed;
	if ( tmp < 0 ) tmp = 0;
	vsp->idle = tmp;
    }
    return update;
}

/********************************************************************
 * This is the main volume adjust routine!
 */
int
adj_vol(smp)
const struct menu_d *smp;
{
    U32		ctrls;
    int		i;
    m_int	volume;
    int		update;
    char	num_row[2];
    int		bottom;
    int 	which;
    struct vol_state vs;

    /* Below allows for game to over-ride the default "Self Test Music"
     * command. This allows games with disk-based music to use
     * it in volume-adjust, and allows audio composers who can't
     * be bothered to put a useful sound at the "guaranteed"
     * ST_S_VMUSIC to get away with it, with sufficient collusion
     * from their game-programmer. The start_stop function will
     * be called with its "what_for_on" parameter here, and
     * with its "what_for_off" parameter at exit. The coroutine
     * function will be called with an integer "best guess
     * microseconds since last call" during the polling loop.
     */
    const struct st_envar *mus_hack;
    const struct mus_hack_struct {
	void (*start_stop)(int);
	void (*coroutine)(int);
	int what_for_on;
	int what_for_off;
    } *mhp;

    /* Reset the audio processor:  Full reset, load first bank for volume test
     */
    i = ResetSoundProc(1);
    if ( i == 0 ) 
      return -1;       /* user gave up    */
    if (i < 0) 
      return(i);       /* Died young...   */

    vs.atrv = 0;
    update = 1;

    /* Start out assuming that we can only set game volume.
     */
    which = WV_ALLOW(WV_GAME);
    volume = aud_setvol(-1);  /* Set the default volume */
    if ( volume < 0 ) {
       VolumeSetError();
       return -1;
    }

    vs.volume = volume;
    vs.old_volume = volume;

    /* If AUD_AV_MSK is defined, we may be using a ratio
     * for attract volume.
     */
#if AUD_AV_MSK
    vs.atrv = (eer_gets(EER_GUTS_OPT) >> AUD_AV_SHF) & AUD_AV_MSK;
    vs.atr_volume =  ((vs.volume)*(vs.atrv))/AUD_AV_MSK;
    which = WV_ALLOW(WV_RATIO);
#endif

    /* If EER_AUD_ATRVOL is defined, it may exist, and if
     * so, we should use it.
     */
#ifdef EER_AUD_ATRVOL
    volume = eer_gets(EER_AUD_ATRVOL);
    if ( volume >= 0 ) {
	/* It is possible in the future that EER_AUD_ATRVOL
	 * could be a "virtual stat" that may not exist,
	 * even if it is #defined. If it _really_ exists,
	 * use "full scale" attract volume adjust.
	 */
	which = WV_ALLOW(WV_ATTR);
	vs.atr_volume = volume;
    }
#endif

    vs.old_atrv = vs.atrv;
    vs.old_atr_volume = vs.atr_volume;
    vs.which_volume = which;

    /* Write out "boilerplate", which varies between normal user
     * and "Nerd Mode".
     */
    num_row[0] = num_row[1] = -1;
    if ( debug_mode & GUTS_OPT_DEVEL ) {
       num_row[0] = VBAR_NUMROW;
       num_row[1] = VBAR_ANUMROW;
       txt_str(11,VBAR_NUMROW,"VOLUME:_",GRN_PALB|AN_BIG_SET);
    } else {
       txt_str(VBAR_COL-1,VBAR_ROW+3,
         "0  1  2  3  4  5  6  7  8  9 10 11",GRY_PAL);
    }

    bottom = st_bottom();

#if !NO_EER_WRITE
    bottom = st_insn(bottom,t_msg_save_ret,t_msg_next,INSTR_PAL);
#else
    bottom = st_insn(bottom,t_msg_ret_menu,t_msg_next,INSTR_PAL);
#endif

    bottom = st_insn(bottom,t_msg_restore,t_msg_action,INSTR_PAL);
    bottom = st_insn(bottom,"To ADJUST volume,",t_msg_control,INSTR_PAL);
    prc_delay0();

    mhp = 0;
    mus_hack = st_getenv("SELFTEST MUSIC", 0);
    if ( mus_hack ) {
	mhp = mus_hack->value;
	if ( mhp && mhp->start_stop ) mhp->start_stop(mhp->what_for_on);
    } else {
#ifndef ST_S_VMUSIC
	txt_str(-1,AN_VIS_ROW-11,"NO SOUND? define ST_S_VMUSIC",ERROR_PAL);
#else
    	aud_put(ST_S_VMUSIC);          /* Start Happy Music!              */
#endif
    }

    ctl_autorepeat(JOY_BITS,AR_FIRST,AR_SUBS);

    update = 1;

    while (1)
    {
	prc_delay0();
	if ( mhp && mhp->coroutine ) mhp->coroutine(16666);
	ctrls = ctl_read_sw(JOY_BITS | SW_NEXT | SW_ACTION);

	if (ctrls & SW_NEXT) break;

	update |= vol_fiddle( &vs, ctrls );

	if ( update < 0 ) {
	    VolumeSetError();
	    break;
	}

	which = vs.which_volume;

	if ( (eer_rtc & 0xF) == 0 && WV_CANSET(which) != WV_GAME ) {
	    int pal;
	    pal = (eer_rtc & 0x10) ? GRY_PAL : WHT_PAL;
	    which = ( WV_CURR(which) != WV_GAME);
	    txt_str(-1,label_row[1-which],which_name[1-which],GRY_PAL);
	    txt_str(-1,label_row[which],which_name[which],pal);
	}
#if (0)
	if ( vs.idle == 0 ) txt_str(2,2,"I",GRY_PAL);
	else txt_str(2,2,"A",GRY_PAL);
#endif
	if ( update == 0 ) continue;


	ShowVolume(vs.volume,bar_row[0],num_row[0]);
	which = WV_CANSET(vs.which_volume);
	switch (which) {
	    case WV_RATIO:
#if AUD_AV_MSK
		txt_clr_wid(VBAR_COL,label_row[1],6);
		if ( vs.atrv == 0 || vs.atrv == AUD_AV_MSK ) {
		    txt_str(VBAR_COL,label_row[1],
			vs.atrv ? "FULL" : "MUTE", GRY_PAL);
		} else {
		    txt_decnum(VBAR_COL,label_row[1],vs.atrv,2,LJ_NF,GRY_PAL);
		    txt_cstr("/",GRY_PAL);
		    txt_cdecnum(AUD_AV_MSK,2,LJ_NF,GRY_PAL);
		}
		ShowVolume(vs.atr_volume,bar_row[1],num_row[1]);
		break;
#endif
	    case WV_ATTR:
		txt_clr_wid(VBAR_COL,label_row[1],6);
		if ( vs.atr_volume == 0 || vs.atr_volume == vs.volume ) {
		    txt_str(VBAR_COL,label_row[1],
			vs.atr_volume ? "FULL" : "MUTE", GRY_PAL);
		}
#if (0)
		/* If you really _must_ see the attract volume expressed
		 * as a percentage of the game volume, re-enable this.
		 * But first consider that you are here because you
		 * chose _not_ to use a ratio, so the percentage will
		 * not "stay put" as you alter game volume. Better
		 * mute than misleading, I always say :-) MEA 27JAN97
		 */
		else {
		    int percent;
		    percent = (vs.atr_volume*100)/vs.volume;
		    txt_decnum(VBAR_COL,label_row[1],percent,2,LJ_NF,GRY_PAL);
		    txt_cstr("%",GRY_PAL);
		}
#endif
		ShowVolume(vs.atr_volume,bar_row[1],num_row[1]);
		break;
	}
	update = 0;
    }


    /* we can save to EEPROM regardless of changes, since non-changes are
     * innocuous.
     */
    eer_puts(EER_AUD_VOL,vs.volume);  /* save volume level to EEROM      */
#if AUD_AV_MSK
    if ( WV_CANSET(vs.which_volume) == WV_RATIO ) {
	U32 atrv = vs.atrv;
	atrv <<= AUD_AV_SHF;
	atrv |= (eer_gets(EER_GUTS_OPT) & ~(AUD_AV_MSK<<AUD_AV_SHF));
	eer_puts(EER_GUTS_OPT,atrv);
    }
#endif
#ifdef EER_AUD_ATRVOL
    eer_puts(EER_AUD_ATRVOL,vs.atr_volume);
#endif
    /* 30JAN97, MEA moves the shutdown code _after_ the save to EEPROM
     * after figuring out why the CoJag sometimes gets a _very_ loud
     * "boot-bing" on exit from the volume-adjust screen. In short,
     * Chuck does a full reset, including the "boot bing", and
     * over-writes his "current" volume values. If the volume from
     * the EEPROM was really loud before we entered, it will be
     * used. CJP didn't seem to know what "-1" meant...
     */
    if ( mhp && mhp->start_stop ) mhp->start_stop(mhp->what_for_off);
    else {
	aud_put(ST_S_STOP);                   /* kill all sounds          */
	prc_delay0();
	aud_init(-1);                  /* finished with engines    */
    }
    return 0;
}

/********************************************************************
 * This is the hook to adjust volume during game play.
 */
void
aud_adj_volume(display, attract)
void (*display)(int value, int show);
int attract;
{
    U32		ctls;
    m_int	volume;
    int		update;
    int 	which;
    static struct vol_state vs;
    int old_idle = vs.idle;

    if ( vs.prev_rtc == 0 ) {
	/* first call ever, set up. */
	vs.atrv = 0;
	update = 1;

	/* Start out assuming that we can only set game volume.
	 */
	which = WV_ALLOW(WV_GAME);
	volume = eer_gets(EER_AUD_VOL);  /* Get the default game volume */
	if ( volume < 0 ) volume = DEF_VOLUME;

	vs.volume = volume;
	vs.old_volume = volume;

	/* If AUD_AV_MSK is defined, we may be using a ratio
	 * for attract volume.
	 */
#if AUD_AV_MSK
	vs.atrv = (eer_gets(EER_GUTS_OPT) >> AUD_AV_SHF) & AUD_AV_MSK;
	vs.atr_volume =  ((vs.volume)*(vs.atrv))/AUD_AV_MSK;
	which = WV_ALLOW(WV_RATIO);
#endif

	/* If EER_AUD_ATRVOL is defined, it may exist, and if
	 * so, we should use it.
	 */
#ifdef EER_AUD_ATRVOL
	volume = eer_gets(EER_AUD_ATRVOL);
	if ( volume >= 0 ) {
	    /* It is possible in the future that EER_AUD_ATRVOL
	     * could be a "virtual stat" that may not exist,
	     * even if it is #defined. If it _really_ exists,
	     * use "full scale" attract volume adjust.
	     */
	    which = WV_ALLOW(WV_ATTR);
	    vs.atr_volume = volume;
	}
#endif

	vs.old_atrv = vs.atrv;
	vs.old_atr_volume = vs.atr_volume;
	vs.which_volume = which;
    }

    ctls = 0;
    if ( WV_CURR(vs.which_volume) == WV_GAME ) {
	if ( attract ) ctls = J_DOWN;
    } else {
	if ( !attract ) ctls = J_UP;
    }

    update = vol_fiddle( &vs, ctls );
    which = vs.which_volume;
    if ( WV_CURR(which) == WV_GAME) volume = vs.volume;
    else volume = vs.atr_volume;

    if ( vs.idle != 0 ) {
	/* Just show current volume, if display exists.
	 */
	if ( display ) display(volume,old_idle ? AAV_UPDATE : AAV_SHOW);
    } else if ( old_idle ) {
	/* Timed out, hide display and update EEPROM.
	 */
	if ( display ) display(volume,AAV_HIDE);	/* Hide display */
	eer_puts(EER_AUD_VOL,vs.volume);  /* save volume level to EEROM      */
#if AUD_AV_MSK
	if ( WV_CANSET(vs.which_volume) == WV_RATIO ) {
	    U32 atrv = vs.atrv;
	    atrv <<= AUD_AV_SHF;
	    atrv |= (eer_gets(EER_GUTS_OPT) & ~(AUD_AV_MSK<<AUD_AV_SHF));
	    eer_puts(EER_GUTS_OPT,atrv);
	}
#endif
#ifdef EER_AUD_ATRVOL
	eer_puts(EER_AUD_ATRVOL,vs.atr_volume);
#endif
    }
    return;
}

STATIC void
VolumeSetError()
{
    /* volume set error message    */
    txt_str(-1,6,"ERROR SETTING VOLUME LEVEL",RED_PALB);

    ExitInst(INSTR_PAL);

    while (1)
    {
       prc_delay0();

       if (ctl_read_sw(0) & SW_NEXT)
           break;
    }

    /* erase volume set error message     */
    txt_clr_str(-1,6,"ERROR SETTING VOLUME LEVEL",RED_PALB);
}

#if (1)
STATIC int snd_cksum PARMS(( const struct menu_d *));
STATIC int snd_by_num PARMS(( const struct menu_d *));
#if HAS_CAGE
STATIC int cage_error_log PARMS(( const struct menu_d *));
#endif
#if HAS_QUAD
int quad_tst PARMS(( const struct menu_d *));
#endif
STATIC int sound_cb PARMS(( const struct menu_d *));

static const struct menu_d snd_menu[] = {
    {"SOUND TESTS",  sound_cb},
#if HAS_QUAD
    {"SPEAKER TEST", quad_tst},
#endif
    {"AUDIO CHECKSUMS", snd_cksum},
    {"?SOUNDS BY NUMBER", snd_by_num},
#if (HAS_CAGE)
    {"?CAGE ERROR LOG", cage_error_log},
#endif
    {0, 0}
};

int snd_test ( smp )
const struct menu_d *smp;
{
    int status;
    ctl_autorepeat(JOY_VERT,30,15);       /* Autorepeat after 1/2secs @ 1/4 */
    status = st_menu(snd_menu,sizeof(snd_menu[0]),MNORMAL_PAL,0);
    return status;
}

static int snd_status;

/*            sound_cb()
 *     This is the "call-back" or coroutine for the sound menu.
 *     It is called with a parameter of 0 when the menu specifying it
 *     is first entered, and with a pointer to the current menu line
 *     every time through st_menu()'s main polling loop. If it returns
 *     non-zero, that value will be passed up to the caller of st_menu().
 */
int sound_cb ( smp )
const struct menu_d *smp;
{
    if ( smp ) {
       /* after first call, just watch for SW_NEXT, and clean up if seen */
       if ( ctl_read_sw(0) & SW_NEXT ) {
           prc_delay0();
           aud_init(-1);
           if ( snd_status == 0 ) return -1;
       }
    } else if ( snd_status == 0 ) {
       /* first time in this menu, and first time we talk to MOS. */
       /* Reset the audio processor:  
        * Full reset, load second bank for volume test */
       snd_status = ResetSoundProc(2);
       if ( snd_status < 0 ) 
         return snd_status;
       if ( snd_status == 0 ) 
         return -1;
    } else show_sound_status( 0, snd_status>>16 );
    return 0;
}

struct snd_rom_desc {
    char *name;
    U16 cksum;
    U16 pad;
};

/* this should be accessed though pConfig */
#if (SST_GAME == SST_RUSH)
static const struct snd_rom_desc srd[] = {
    {"U62", 0, 0},
    {"U61", 0, 0},
    {"U53", 0, 0},
    {"U49", 0, 0},
    {"U69", 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};
#else
static const struct snd_rom_desc srd[] = {
    {"\?\?\?", 0x9E81, 0},
    {"\?\?\?", 0xE0A5, 0},
    {"\?\?\?", 0xC89A, 0},
    {"\?\?\?", 0x1A62, 0},
    {"\?\?\?", 0x5656, 0},
    {"\?\?\?", 0x9720, 0},
    {"\?\?\?", 0x36C9, 0},
    {"\?\?\?", 0xDEA9, 0},
    {"\?\?\?", 0x8B6B, 0},
    {"\?\?\?", 0xEC1F, 0},
    {"\?\?\?", 0x304A, 0},
    {"\?\?\?", 0x4640, 0},
    {"\?\?\?", 0xCA1C, 0},
    {"\?\?\?", 0x0712, 0},
    {"\?\?\?", 0xDC58, 0},
    {"\?\?\?", 0xC297, 0}
};
#endif

const int aud_cksum_dly = 600;

STATIC int snd_cksum(smp)
const struct menu_d *smp;
{
    int status,timer;
    int cmd;
    int row,col,idx;
    int errors;

    row = 3;
    col = 1;
    errors = 0;
    cmd = aud_cmd(ST_C_DIAG);
    if ( cmd == 0 ) {
       txt_str(-1,AN_VIS_ROW/2,"SOUND CHECKSUMS NYI",RED_PALB);
       while ( (ctl_read_sw(0) & SW_NEXT) == 0 ) prc_delay0();
       return 1;
    }
    while ( aud_q_get() >= 0 ) prc_delay0();     /* flush */
    for ( timer = 42 ; --timer >= 0 ; ) {
       status = aud_put(cmd);
       if ( status ) break;
       prc_delay0();
    }
    if ( timer < 0 ) return -1;
    txt_str(-1,row++,"CKSUM CMD SENT, WAITING",WHT_PAL);
    for ( timer = aud_cksum_dly ; --timer >= 0 ; ) {
       int secs;
       status = aud_q_get();
       if ( status >= 0 ) break;
       secs = timer / 60;
       if ( timer == (secs * 60) ) {
           txt_clr_wid(-1,row,2);
           txt_decnum(-1,row,secs,2,RJ_BF,GRY_PAL);
       }
       prc_delay0();
    }
    txt_clr_wid(-1,row,2);
    if ( timer < 0 ) {
       /* OOPS, timed out */
       txt_str(-1,row,"NO RESPONSE",RED_PAL);
       prc_delay(120);
       return -1;
    }
    txt_str(-1,row++,"CKSUM CMD DONE",WHT_PAL);
    cmd = aud_cmd(ST_C_CKSUMS);
    if ( cmd == 0 ) return -1;
    for ( timer = 42 ; --timer >= 0 ; ) {
       status = aud_put(cmd);
       if ( status ) break;
       prc_delay0();
    }
    if ( timer < 0 ) return -1;
    txt_str(-1,row++,"GETTING CKSUMS",WHT_PAL);
    idx = 0;
    for ( timer = 42 ; --timer >= 0 ; ) {
       status = aud_q_get();
       if ( status >= 0 ) {
           /* Got a reply, print it */
	if ( srd[idx].name )
	{
           txt_str(3,++row,srd[idx].name,WHT_PAL);
           if ( status != srd[idx].cksum ) {
	      txt_cstr(" FAILED ", RED_PAL);
              txt_chexnum(status,4,RJ_ZF,RED_PAL);
              ++errors;
           } else {
	      txt_cstr(" PASSED", GRN_PAL);
	   }
	}
	++idx;
        timer = 10;
       }
       else prc_delay0();
    }
    if ( idx != (sizeof(srd)/sizeof(srd[0])) ) {
       /* More or fewer checksums than proper */
       char errbuf[AN_VIS_COL+1];
       strcpy(errbuf,"GOT ");
       utl_cdec(idx,errbuf+4,3,LJ_NF);
       strncat(errbuf,"CHECKSUMS, NEEDED ",AN_VIS_COL-8);
       utl_cdec((sizeof(srd)/sizeof(srd[0])),
         errbuf+strlen(errbuf),3,LJ_NF);
       txt_str(-1,++row,errbuf,RED_PAL);
       errors |= 0x100;
    }
    if ( errors == 0 ) {
       txt_str(-1,++row,"ALL CHECKSUMS OK",INSTR_PAL);
    } else txt_str(-1,++row,"NOTE ERRORS",RED_PAL); 
    while ( (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0 ) prc_delay0();
    return errors;
}

#if HAS_CAGE
#define C_E_LOG_ROW (5)
#define C_E_LOG_COL (1)
#ifdef DEBUG
extern void aud_q_dump PARMS((
int row,      /* row to start display */
int npal,     /* "normal" palette */
int ipal,     /* palette to display item at "in" idx */
int opal      /* palette to display item at "out" idx */
));
#endif

STATIC int cage_error_log(smp)
const struct menu_d *smp;
{
    int status,timer;
    int cmd;
    int row,col,pal;
    int errors;

    errors = 0;
    cmd = aud_cmd(ST_C_ERR_LOG);
    if ( cmd == 0 ) {
       txt_str(-1,AN_VIS_ROW/2,"ERROR LOG NOT AVAILABLE",RED_PALB);
       timer = 42;
       while ( (ctl_read_sw(0) & SW_NEXT) == 0 ) {
           prc_delay0();
           if ( --timer < 0 ) break;
       }
       ctl_read_sw(SW_NEXT);
       return 1;
    }
#ifdef EER_SNDSLST
    if ( (status = eer_gets(EER_SNDSLST)) >= 0 ) {
       txt_str(2,C_E_LOG_ROW-1,"LOST SNDS:",status ? RED_PAL : WHT_PAL);
       txt_cdecnum(status,3,RJ_BF,WHT_PAL);
    }
#endif
#ifdef EER_DATALST
    if ( (status = eer_gets(EER_DATALST)) >= 0 ) {
       txt_str(27,C_E_LOG_ROW-1,"LOST DATA:",status ? RED_PAL : WHT_PAL);
       txt_cdecnum(status,3,RJ_BF,WHT_PAL);
    }
#endif
#ifdef EER_AUDRESET
    if ( (status = eer_gets(EER_AUDRESET)) >= 0 ) {
       txt_str(16,C_E_LOG_ROW-1,"RESETS:",status ? RED_PAL : WHT_PAL);
       txt_cdecnum(status,3,RJ_BF,WHT_PAL);
    }
#endif

    while ( (status = aud_q_get()) >= 0 ) prc_delay0(); /* flush */
    while (1) {
#ifdef USER_REC_AUD_ERRS
       unsigned char *saved_log;
       int reclen;
#endif
       cmd = aud_cmd(ST_C_ERR_LOG);
       for ( timer = 42 ; --timer >= 0 ; ) {
           status = aud_put(cmd);
           if ( status ) break;
           prc_delay0();
       }
       if ( timer < 0 ) {
           txt_str(-1,3,"ERROR LOG CMD NOT SENT",RED_PAL);
           return -1;
       }
       errors = 0;
       for ( row = 4 ; row >= 0 ; --row ) {
           txt_clr_wid(1,row+C_E_LOG_ROW,AN_VIS_COL-2);
       }
#ifdef DEBUG
#if HAS_CAGE
       aud_q_dump(C_E_LOG_ROW+8,GRY_PAL, WHT_PALB, GRN_PAL);
#endif
#endif
       row = C_E_LOG_ROW;
       col = C_E_LOG_COL;

#ifdef USER_REC_AUD_ERRS
       /* we may have some old errors in EEPROM */
       saved_log = eer_user_rd(USER_REC_AUD_ERRS,&reclen);
       if ( saved_log ) {
           m_int idx;
           unsigned short errwd;
           for ( idx = 0 ; idx < reclen ; idx += 2 ) {
              errwd = saved_log[idx] | (saved_log[idx+1]<<8);
              if (errwd == 0xCA6E) break;
              if ( col > (AN_VIS_COL-5) ) {
                  col = C_E_LOG_COL;
                  ++row;
              }
              col += 1+txt_hexnum(col,row,errwd,4,RJ_ZF,YEL_PAL);
           }
           eer_user_free(USER_REC_AUD_ERRS);
       }
#endif
       for ( timer = 42 ; --timer >= 0 ; ) {
           status = aud_q_get();
           if ( status >= 0 ) {
              if ( status == 0xCA6E ) break;
              /* Got a reply, print it */
              if ( col > (AN_VIS_COL-5) ) {
                  col = C_E_LOG_COL;
                  ++row;
              }
              pal = WHT_PAL;
              col += 1+txt_hexnum(col,row,status,4,RJ_ZF,pal);
              ++errors;
              timer = 10;
           }
           else prc_delay0();
       }
       if ( errors & 1 ) {
           txt_str(C_E_LOG_COL,++row,"ODD # of ERRORS?",RED_PAL);
       }
       /* Check here for match with error count? */
       if ( errors == 0 ) {
           txt_str(-1,row,"NO NEW ERRORS LOGGED",GRY_PAL);
       }
       errors >>= 1;
       cmd = 0;
       if ( !smp ) return 0;       /* hack for call from elsewhere */
       /* called from menu, give the option of clearing */
       for ( timer = 20 ; timer >= 0 ; --timer ) {
           if ( (ctl_read_sw(0) & SW_ACTION) == 0 ) break; 
           prc_delay0();
       }
       if ( timer >= 0 ) {
           /* let go of SW_ACTION, see if they hold it again
            * before hitting NEXT
            */
           timer = 10;
           do {
              prc_delay0();
              status = ctl_read_sw(SW_NEXT);
              if ( (status & SW_ACTION) && --timer < 0 ) {
#ifdef USER_REC_AUD_ERRS
                  /* Clear old errors in EEPROM too */
                  saved_log = eer_user_rd(USER_REC_AUD_ERRS,&reclen);
                  if ( saved_log ) {
                     saved_log[0] = 0x6E;
                     saved_log[1] = 0xCA;
                     eer_user_wrt(USER_REC_AUD_ERRS);
                  }
#endif
                  cmd = aud_cmd(ST_C_CLR_ERR_LOG);
                  ctl_read_sw(SW_ACTION); /* kill edge */
                  break;
              } else if ( (status & SW_ACTION) == 0 && timer < 8 ) {
                  /* let go pretty soon, just re-paint */
                  break;
              }
           } while ( (status & SW_NEXT) == 0 );
       }
       if ( cmd ) aud_put(cmd);
       if ( status & SW_NEXT ) return errors;
       prc_delay0();
    } /* end while (1) */
    return errors;
}
#endif
/*            Use generic parameter entry to build up a MOS command,
 *     then send it.
 */

/*            plist[] (below) describes the parameters we wish to
 *     enter.
 */
static const struct parm plist [] = {
    /* Label         value         col/row       sigdig        curdig */
    { "Command: ",   0x8003,              4,3,   (HEX_FIELD|4),       0      },
#define PIDX_CMD (0)
    { "P0: ", 0xC0,         4,5,   (HEX_FIELD|4),       0      },
    { "P1: ", 0,            13,5,  (HEX_FIELD|4),       0      },
    { "P2: ", 0x400,        22,5,  (HEX_FIELD|4),       0      },  /* default for custom SFX */
    { "P3: ", 0,            31,5,  (HEX_FIELD|4),       0      },
    { "P4: ", 0,            4,7,   (HEX_FIELD|4),       0      },
    { "P5: ", 0,            13,7,  (HEX_FIELD|4),       0      },
    { "P6: ", 0,            22,7,  (HEX_FIELD|4),       0      },
    { "P7: ", 0,            31,7,  (HEX_FIELD|4),       0      },
    { 0,             0,            0,0,   0,            0      }};

#define NPARMS (sizeof (plist)/sizeof(plist[0]))

/*            
sbn_cb() is the "callback". That is, it is called by the
 *     parameter entry code to validate changes to the parameters and
 *     copy them to the buffer.
 */
STATIC int
sbn_cb(struct parm *pp, int idx,struct opaque *op)
{
    unsigned short *sdp = (unsigned short *)op;
    unsigned long val;

    val = pp->val;
    if ( idx > 0 ) {
       sdp[idx+1] = val &= 0x7FFF;
    } else {
       sdp[idx+1] = val |= 0x8000;
    }
    pp->val = val;
    if ( idx > *sdp ) {
       struct parm *opp;
       opp = pp  - idx;
       opp += *sdp;
       txt_clr_wid(opp->col,opp->row+1,1);
       txt_str(pp->col,pp->row+1,"*",GRY_PAL);
       *sdp = idx;
    }
    return 1;
}

/* This function is a jag specific debugging function */
extern void synPrintJVcb();

/* these defines make us more screen size- independant */
#define REPLY_TOP ((AN_VIS_ROW/2)+1)
#define REPLY_BOTTOM (REPLY_TOP+4)

STATIC
int snd_by_num( parm_mp )
const struct menu_d *parm_mp;
{
    struct parm work[NPARMS],*wp;
    const struct parm *cwp;
    unsigned short buff[NPARMS+1];
    int idx,status,row;

    /* ensure that bank 1 is loaded */
    unsigned short cmd[2];
    cmd[0] = S_LOAD_BANK;
    cmd[1] = 1;
    aud_mput(cmd, 2);
    
    
    wp = work;
    cwp = plist;
   
    do {
       *wp++ = *cwp++;
    } while (wp[-1].label != 0);

    for ( idx = 0 ; idx < NPARMS ; ++idx ) {
       if ( work[idx].label == 0 ) break;
       sbn_cb(work+idx,idx,(struct opaque *)buff);
    }

    show_sound_status(0,0);
    buff[0] = 0;
    while (1) {

#if (JERRY_BASE)
       synPrintJVcb(8);		/* print debug info on CoJag */
#endif
       status = utl_enter(work,sbn_cb,(struct opaque *)buff);
       if ( status & SW_NEXT ) return 0;
       if ( status & SW_ACTION ) {
           /* Send command */
           int count = buff[0]+1;
           status = aud_mput(buff+1, count);
           if ( status < count ) {
              /* no luck sending */
              txt_str(-1,AN_VIS_ROW/2,"FAILED TO SEND",RED_PAL);
              prc_delay(42);
              return -1;
           }
           idx = 0;
           for ( row = REPLY_BOTTOM ; --row >= REPLY_TOP ; ) {
              txt_clr_wid(2,row,AN_VIS_COL-4);
           }
           ++row;
           count = 5;
           while ( count || (status = ctl_read_sw(0)) & SW_ACTION ) {
              status = aud_q_get();
              if ( status >= 0 ) {
                  idx += 5;
                  if ( idx > (AN_VIS_COL-6) ) {
                     idx = 5;
                     if ( ++row > REPLY_BOTTOM) break;
                  }
                  txt_hexnum(idx,row,status,4,RJ_ZF,WHT_PAL);
              } else {
                  /* no reply, run count and try again */
                  prc_delay0();
                  if ( count ) --count;
              }
           }
       }
    }
    aud_put(ST_S_STOP);  /* stop all sounds */
    return 0;
}

#if HAS_QUAD
#ifndef FWD_LEFT
#define FWD_LEFT   0
#endif
#ifndef FWD_RIGHT
#define FWD_RIGHT  0
#endif
#ifndef BACK_LEFT
#define BACK_LEFT  0
#endif
#ifndef BACK_RIGHT
#define BACK_RIGHT 0
#endif
static const unsigned dirphrase[] = {
       FWD_LEFT,     0,     FWD_RIGHT,
       0,            0,     0,
       BACK_LEFT,    0,     BACK_RIGHT
};

#ifdef RUMP_THUMP
#define S_THUMP (RUMP_THUMP)
#else
#define S_THUMP (0x80B7)
#endif

int quad_tst ( smp )
const struct menu_d *smp;
{
    unsigned long ctls;
    int dir,old_dir;
    int volume,dec_vol,vpal;

#if FWD_LEFT && FWD_RIGHT && BACK_LEFT && BACK_RIGHT
    txt_str(-1,AN_VIS_ROW/2,"To select speaker",INSTR_PAL);
    txt_str(-1,(AN_VIS_ROW/2)-1,t_msg_control,INSTR_PAL);
#endif
#ifdef S_THUMP
#if (0)
    txt_str(-1,(AN_VIS_ROW-6),"FOR A GOOD TIME",INSTR_PAL);
#else
    txt_str(-1,(AN_VIS_ROW-6),"To test seat speaker",INSTR_PAL);
#endif
    txt_str(-1,(AN_VIS_ROW-5),t_msg_action,INSTR_PAL);
#endif
    old_dir = -1;
    vpal = GRY_PAL;
    volume = aud_setvol(-1);
    if ( volume <= 0 ) {
       vpal = RED_PAL;
       volume = 0;
    }
    txt_str(-1,(AN_VIS_ROW/2)+2,"VOLUME: ",vpal);
    /* compute "decimal" volume in manner consistent with volume adjust screen
     */
    dec_vol = (volume*1000)/2338;
    txt_cdecnum((dec_vol+5)/10,2,LJ_BF,vpal);
    /* optionally also show "NERD MODE" volume */
#ifdef GUTS_OPT_DEVEL
    if ( debug_mode & GUTS_OPT_DEVEL )
#endif
    {
       txt_cstr(" (",vpal);
        txt_cdecnum(volume,3,LJ_BF,vpal);
       txt_cstr(")",vpal);
    }
    /* start with the rotating announcement */
    if ( (dir = aud_cmd(ST_C_SPKR)) != 0 ) aud_put(dir);

    while ( ((ctls = ctl_read_sw(SW_NEXT)) & SW_NEXT) == 0 ) {
       prc_delay0();
#ifdef S_THUMP
       if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) aud_put(S_THUMP);
#endif
#if FWD_LEFT && FWD_RIGHT && BACK_LEFT && BACK_RIGHT
       st_joy_disp(AN_VIS_COL/2,(AN_VIS_ROW/2)-4,ctls);
       if ( (ctls & (J_UP|J_DOWN|J_LEFT|J_RIGHT)) == 0 ) continue;
       dir = 4;
       if ( ctls & J_UP ) dir -= 3;
       if ( ctls & J_DOWN ) dir += 3;
       if ( ctls & J_LEFT ) dir -= 1;
       if ( ctls & J_RIGHT ) dir += 1;
       if ( old_dir < 0 ) {
           /* First move off center, kill rotating announcement */
           aud_put(ST_S_STOP);
       }
       if ( (old_dir != dir) && dirphrase[dir] ) aud_put(dirphrase[dir]);
       old_dir = dir;
#endif
    }
    aud_put(ST_S_STOP);
    return 0;
}
#endif
#endif
