
/*
 *	phx_rtc.c -- Forrest Miller -- September 1996
 *
 *	Functions for using the timekeeper chip on Phoenix-class hardware.
 *
 *
 *		Copyright 1996 Time Warner Interactive
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>
#include <nsprintf.h>
#include <wms_proto.h>
#include <pic_defs.h>

#define	STATIC	static

STATIC char *const error  = "Failed to read clock!";
STATIC char *const days   = "Mon\0Tue\0Wed\0Thu\0Fri\0Sat\0Sun";
STATIC char *const months = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";

#define RTC_BUF_LEN 80

#define binval(x) ( (x) - ( (x) >> 4 ) * 6 )

void rtc_display(int col, int row, int pal)
{
 U8 DateTimeBuffer[7];
 char rtc_buf[RTC_BUF_LEN];
 int dow, month, year;

 if ( GetPICDateTime (DateTimeBuffer) == FAILURE )
  txt_str(col, row, error, ERROR_PAL);
 else
 {
  dow = ( binval(DateTimeBuffer[DAY_OF_WEEK]) - 1 ) * 4;
  month = ( binval(DateTimeBuffer[MONTH]) - 1 ) * 4;
  year = binval(DateTimeBuffer[YEAR]);

  nsprintf(rtc_buf, RTC_BUF_LEN, "%s %s %02X, %02X %02X:%02X:%02X", &days[dow],
   &months[month], DateTimeBuffer[DAY_OF_MONTH],
   DateTimeBuffer[YEAR] + ( (year > 95) ? 0x1900 : 0x2000),
   DateTimeBuffer[HOURS], DateTimeBuffer[MINUTES], DateTimeBuffer[SECONDS]);
  txt_str(col, row, rtc_buf, pal);
 }
}

/*
 *	plist[] (below) describes the parameters we wish to enter.
 */
STATIC const struct parm plist [] = {
    /* Label		value		col/row	sigdig		curdig	*/
    { "Year:    ",	0,		4,4,	(BCD_FIELD|2),	0 	},
#define PIDX_YEAR  (0)
    { "Month:   ",	0,		4,5,	(BCD_FIELD|2),	0 	},
#define PIDX_MONTH (1)
    { "Date:    ",	0,		4,6,	(BCD_FIELD|2),	0 	},
#define PIDX_DATE  (2)
    { "Day:      ",	0,		4,7,	(BCD_FIELD|1),	0 	},
#define PIDX_DAY   (3)
    { "Hour:    ",	0,		4,8,	(BCD_FIELD|2),	0 	},
#define PIDX_HOUR  (4)
    { "Minute:  ",	0,		4,9,	(BCD_FIELD|2),	0 	},
#define PIDX_MIN   (5)
    { "Second:  ",	0,		4,10,	(BCD_FIELD|2),	0 	},
#define PIDX_SEC   (6)
    { 0,		0,		0,0,	0,		0 	}};
#define NPARMS (sizeof (plist)/sizeof(plist[0]))

STATIC int rtc_cb(struct parm *pp, int idx, struct opaque *op)
{
 unsigned long val;

 val = pp->val & 0xff;
 switch (idx)
 {
  case PIDX_YEAR:
   break;
  case PIDX_MONTH:
   if ( (val == 0) || (val > 100) ) val = 0x12;
   else if (val > 0x12) val = 1;
   break;
  case PIDX_DATE:
   if ( (val == 0) || (val > 100) ) val = 0x31;
   else if (val > 0x31) val = 1;
   break;
  case PIDX_DAY:
   if (val == 8) val = 1;
   if (val == 0) val = 7;
   break;
  case PIDX_HOUR:
   if (val > 100) val = 0x23;
   else if (val > 0x23) val = 0;
   break;
  case PIDX_MIN:
   if (val > 110) val = 0x59;
   else if (val > 0x59) val = 0;
   break;
  case PIDX_SEC:
   if (val > 110) val = 0x59;
   else if (val > 0x59) val = 0;
 } /* end switch (idx) */
 pp->val = val & 0xff;
 return 1;
}

STATIC int rtc_get_clock( struct parm *pp )
{
 U8 DateTimeBuffer[7];
 int result = GetPICDateTime (DateTimeBuffer);

 if ( result == SUCCESS )
 {
  pp[PIDX_YEAR].val  = DateTimeBuffer[YEAR];
  pp[PIDX_MONTH].val = DateTimeBuffer[MONTH];
  pp[PIDX_DATE].val  = DateTimeBuffer[DAY_OF_MONTH];
  pp[PIDX_DAY].val   = DateTimeBuffer[DAY_OF_WEEK];
  pp[PIDX_HOUR].val  = DateTimeBuffer[HOURS];
  pp[PIDX_MIN].val   = DateTimeBuffer[MINUTES];
  pp[PIDX_SEC].val   = DateTimeBuffer[SECONDS];
 }

 return result;
}

STATIC int rtc_put_clock( struct parm *pp )
{
 U8 DateTimeBuffer[7];

 DateTimeBuffer[YEAR] = pp[PIDX_YEAR].val;	/* set the time to now */
 DateTimeBuffer[MONTH] = pp[PIDX_MONTH].val;
 DateTimeBuffer[DAY_OF_MONTH] = pp[PIDX_DATE].val;
 DateTimeBuffer[DAY_OF_WEEK] = pp[PIDX_DAY].val;
 DateTimeBuffer[HOURS] = pp[PIDX_HOUR].val;
 DateTimeBuffer[MINUTES] = pp[PIDX_MIN].val;
 DateTimeBuffer[SECONDS] = pp[PIDX_SEC].val;

 return SetPICDateTime (DateTimeBuffer);
}

int rtc_set_clock( smp )
const struct menu_d *smp;	/* Selected Menu Pointer */
{
 struct parm work[NPARMS], *wp;
 const struct parm *cwp;
 int bottom = AN_VIS_ROW-2;

 bottom = st_insn(bottom,t_msg_save_ret, t_msg_next, INSTR_PAL);
 bottom = st_insn(bottom,t_msg_restore, t_msg_action, INSTR_PAL);

 wp = work;
 cwp = plist;

 do
 {
  *wp++ = *cwp++;
 } while (wp[-1].label != 0);

 if ( rtc_get_clock(work) == FAILURE )
 {
  txt_str(-1, AN_VIS_ROW/2, error, ERROR_PAL);
  while ( ( ctl_read_sw(0) & SW_NEXT ) != SW_NEXT ) prc_delay(0);
  return 0;
 }

 if ( utl_enter(work, rtc_cb, (struct opaque *)&work[0]) == SW_NEXT )
    rtc_put_clock(work);

 return 0;
}
