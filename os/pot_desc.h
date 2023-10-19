#ifndef POT_CODE

/* Pot Flags Used in pot_desc below */

#define PF_VERT 0x80	/* Pot Flag Vertical  */
#define PF_PARA 0x40	/* Pot Flag Parabolic */
#define PF_DBUG 0x20	/* Pot Flag Debug     */
#define PF_AMSK 0x0F	/* Pot Flag Age Mask  */

static const struct pot_config {
    int range;
    int scale;
    unsigned long low_swt;
    unsigned long high_swt;
    const char *labels;         /* String for display function to use */
    unsigned char pot,x,y,flags;
} pot_desc[POT_CNT] = {		/* One per pot, in "potBank order" */
     { 250, 256, 0, 0, "Pot 0",  8, AN_VIS_COL_MAX*1/5-5, AN_VIS_ROW_MAX/2-10, 1 }
    ,{ 250, 256, 0, 0, "Pot 1",  9, AN_VIS_COL_MAX*2/5-5, AN_VIS_ROW_MAX/2-10, 1 }
    ,{ 250, 256, 0, 0, "Pot 2", 10, AN_VIS_COL_MAX*3/5-5, AN_VIS_ROW_MAX/2-10, 1 }
    ,{ 250, 256, 0, 0, "Pot 3", 11, AN_VIS_COL_MAX*4/5-5, AN_VIS_ROW_MAX/2-10, 1 }
    ,{ 250, 256, 0, 0, "Pot 4", 12, AN_VIS_COL_MAX*1/5-5, AN_VIS_ROW_MAX/2-2,  1 }
    ,{ 250, 256, 0, 0, "Pot 5", 13, AN_VIS_COL_MAX*2/5-5, AN_VIS_ROW_MAX/2-2,  1 }
    ,{ 250, 256, 0, 0, "Pot 6", 14, AN_VIS_COL_MAX*3/5-5, AN_VIS_ROW_MAX/2-2,  1 }
    ,{ 250, 256, 0, 0, "Pot 7", 15, AN_VIS_COL_MAX*4/5-5, AN_VIS_ROW_MAX/2-2,  1 }
};

#else	/* POT_CODE defined */


#define GAME_VALUE RAW_AD[potNum]

#define plot_pot(vals,pcp,color) do {;} while (0)


int ReadPot(int pot)
{
 int val;

 val = *( (VU32 *)AD_PORTS_ADDR );
 *( (VU32 *)AD_PORTS_ADDR ) = pot;
 return ( val & 0xff );
}

#define POT_TIME ( 1000 )

PRIVATE struct tq tqtmr;
PRIVATE int pre_scaler;

void pot_timer( void *which )
{
    int i = ( int )which;
    int next = ( i == ( POT_CNT - 1 ) ) ? 0 : i + 1;

    tqtmr.delta = POT_TIME;		/* delay until conversion valid */
    tqtmr.func = pot_timer;
    tqtmr.vars = (void *)next;		/* next pot */
    tq_ins( &tqtmr );

    if ( pots_being_read > 0 )
    {
	RAW_AD[ i ] = ReadPot( pot_desc[ next ].pot );

	if ( ( ++pre_scaler % 16 ) == 0 ) --pots_being_read;

	Filter(&potBank[i],RAW_AD[i]);
	Limit(&potBank[i]);			/* handle hi/lo limits */
    }
}


extern void SetForce(U8);

#define NEEDS_CAL_GAS	( 1 << POT_GAS )
#define NEEDS_CAL_BRAKE	( 1 << POT_BRAKE )
#define NEEDS_CAL_WHEEL	( 1 << POT_WHEEL )

#define GAS_ROW		 9
#define BRAKE_ROW	 12
#define WHEEL_ROW	 17

#if NO_EER_WRITE == 0

#define POST_TIME	 180

PRIVATE int pot_check( void )
{
 int i, result;

 for ( i = 0, result = 0; i < POT_CNT; ++i )
 {
  result |= ( ( potBank[ i ].hi.lim - potBank[ i ].lo.lim ) < pot_desc[ i ].range ) << i;
 }
 return ( result & ( NEEDS_CAL_GAS | NEEDS_CAL_BRAKE | NEEDS_CAL_WHEEL ) );
}

PRIVATE void pot_ask_cal( int result )
{
 if ( result & NEEDS_CAL_GAS )
 {
  txt_str( -1, GAS_ROW, "PRESS GAS TO CALIBRATE", WHT_PAL|AN_BIG_SET);
 }
 else
 {
  txt_clr_str( -1, GAS_ROW, "PRESS GAS TO CALIBRATE", WHT_PAL|AN_BIG_SET);
  pot_save();
 }
 if ( result & NEEDS_CAL_BRAKE )
 {
  txt_str( -1, BRAKE_ROW, "PRESS BRAKE TO CALIBRATE", WHT_PAL|AN_BIG_SET);
 }
 else
 {
  txt_clr_str( -1, BRAKE_ROW, "PRESS BRAKE TO CALIBRATE", WHT_PAL|AN_BIG_SET);
  pot_save();
 }
}

PRIVATE void pot_POST( int level )
{
 if ( ( pot_check() & ( NEEDS_CAL_GAS | NEEDS_CAL_BRAKE | NEEDS_CAL_WHEEL ) )
   && ( !level && !go_to_self_test && DIAG&1 ) )
 {
  int i, result, wait_abort;
  U16 hi_val, lo_val;
  S16 prev[POT_CNT];

  ctl_read_sw(SW_EXTRA);
  prev[0] = -32767;
  txt_str( -1, 3, "PRESS START TO SKIP TESTS", WHT_PAL|AN_BIG_SET);
  txt_str( -1, WHEEL_ROW, "TESTING STEERING WHEEL", WHT_PAL|AN_BIG_SET);
  for ( i = 0; i < POST_TIME; ++i )
  {
   if ( ctl_read_sw(SW_EXTRA) & SW_EXTRA ) return;
   SetForce( 0x18 );
   pot_ask_cal( pot_check() );
   pot_display(prev);
   prc_delay(0);
   hi_val = pot_raw( POT_WHEEL );
  }
  for ( i = 0; i < POST_TIME; ++i )
  {
   if ( ctl_read_sw(SW_EXTRA) & SW_EXTRA ) return;
   SetForce( 0xE8 );
   pot_ask_cal( pot_check() );
   pot_display(prev);
   prc_delay(0);
   lo_val = pot_raw( POT_WHEEL );
  }
  if ( ( hi_val - lo_val ) < pot_desc[ POT_WHEEL ].range )
  {
   txt_clr_str( -1, 3, "PRESS START TO SKIP TESTS", WHT_PAL|AN_BIG_SET);
   txt_clr_str( -1, WHEEL_ROW, "TESTING STEERING WHEEL", WHT_PAL|AN_BIG_SET);
   txt_str( -1, WHEEL_ROW, "STEERING WHEEL NOT RESPONDING", RED_PAL|AN_BIG_SET);
   txt_str( -1, WHEEL_ROW+3, "PRESS START TO CONTINUE", RED_PAL|AN_BIG_SET);
   wait_abort = 1;
  }
  else
  {
   pot_save();
   wait_abort = 0;
  }
  i = 1;
  while ( ( result = pot_check() ) || --i )
  {
   if ( ctl_read_sw(SW_EXTRA) & SW_EXTRA ) return;
   if ( result ) i = 120;
   pot_ask_cal( result );
   pot_display(prev);
   prc_delay(0);
  }
  if ( wait_abort )
  {
   while ( ( ctl_read_sw(SW_EXTRA) & SW_EXTRA ) == 0 ) prc_delay(0);
  }
 }
}
#else
PRIVATE void pot_POST( int level )
{
}
#endif	/* NO_EER_WRITE */

#endif	/* POT_CODE */
