
/*
 *	net_test.c -- Forrest Miller -- November 1996
 *
 *	Functions for network testing.
 *
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <phx_proto.h>
#include <wms_proto.h>
#include <nsprintf.h>
#include <string.h>
#include <net_smc.h>
#include <net_link.h>
#include <smc_link.h>
#include <smc_test.h>
#include <smc_regs.h>
#if USE_QIO_ETH
# include <qio.h>
# include <eth.h>
#endif

#if SST_GAME == SST_RUSH
# if NO_EER_WRITE == 0
extern const unsigned char **GUTS_finds_car_color( void );
# endif
#endif

#define	STATIC	static

/*
 * Ethernet Controller Definitions
 */
#define LOGICAL_LOOPBACK  ( SMC_TX_EPHLOOP )
#define PHYSICAL_LOOPBACK ( SMC_TX_LOOP )

/*
 * Net Test Definitions
 */
#define OP_LOGICAL_LOOPBACK	LOGICAL_LOOPBACK
#define OP_PHYSICAL_LOOPBACK	PHYSICAL_LOOPBACK
#define OP_NETWORK_LOOPBACK	( LOGICAL_LOOPBACK | PHYSICAL_LOOPBACK )
#define OP_COMM_INIT		0x10000
#define OP_COMM_LINK		0x20000
#define OP_COMM_STOP		0x30000

#define NET_ERR_OPEN		(-1)
#define NET_ERR_BUFF		(-2)
#define NET_ERR_SEND		(-3)
#define NET_ERR_LOOP		(-4)
#define NET_ERR_DATA		(-5)
#define NET_ERR_NODE		(-6)
#define NET_ERR_INIT		(-7)
#define NET_ERR_RESET		(-8)

#define NET_BUFFER_MEMORY	( NET_BUFFER_TOTAL * sizeof( Net_Buffer ) )


STATIC Net_Buffer *fnb;
STATIC Net_Buffer *lnb;

STATIC void net_que( Net_Buffer *qnb )
{
 if ( lnb )
 {
  lnb->next = qnb;
  lnb = qnb;
 }
 else
 {
  fnb = lnb = qnb;
 }
 lnb->next = (Net_Buffer *)0;
}


STATIC Net_Buffer *net_deq( void )
{
 Net_Buffer *dnb = fnb;

 if ( dnb )
 {
  int old_ipl = prc_set_ipl(INTS_OFF);
  fnb = dnb->next;
  if ( fnb == (Net_Buffer *)0 ) lnb = (Net_Buffer *)0;
  prc_set_ipl(old_ipl);
  dnb->next = (Net_Buffer *)0;
 } 

 return dnb;
}


STATIC int win_l, win_u, win_r, win_d, win_x, win_y;

STATIC void clrwin( void )
{
 int row;
 for ( row = win_u; row <= win_d; ++row )
 {
  txt_clr_wid(win_l, row, win_r);
 }
}


#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b?a:b)

STATIC void gotoxy(int x, int y)
{
 win_x = win_l + max(x, 0);
 win_x = min(win_x, win_r);
 win_y = win_u + max(y, 0);
 win_y = min(win_y, win_d);
}


STATIC void window(int x1, int y1, int x2, int y2)
{
 win_l = min(x1, x2);		/* left */
 win_l = max(win_l, 0);
 win_u = min(y1, y2);		/* up */
 win_u = max(win_u, 0);
 win_r = max(x1, x2);		/* right */
 win_r = max(win_r, 0);
 win_d = max(x1, y2);		/* down */
 win_d = max(win_d, 0);
 gotoxy(0, 0);
 clrwin();
}


STATIC void _smc_putstr( const char *str )
{
 int i = 0;
 char c, txt[AN_VIS_COL+1];

 while ( ( c = *str++ ) )
 {
  if ( ( c == '\n' ) || ( c == '\r' ) )
  {
   txt[i] = '\0';
   txt_str(win_x, win_y, txt, WHT_PALB);
   i = 0;
   win_x = win_l;
   if ( c == '\n' )
   {
    win_y = ( win_y == win_d ) ? win_u : ++win_y;
    txt_clr_wid(win_l, win_y, win_r - win_l);
   }
  }
  else txt[ i++ ] = c;

  if ( i == ( win_r - win_x) )
  {
   txt[i] = '\0';
   txt_str(win_x, win_y, txt, WHT_PALB);
   i = 0;
   win_x = win_l;
   win_y = ( win_y == win_d ) ? win_u : ++win_y;
   txt_clr_wid(win_l, win_y, win_r - win_l);
  }
 }

 if ( i )
 {
  txt[i] = '\0';
  txt_str(win_x, win_y, txt, WHT_PALB);
  win_x = win_l + i;
 }

}


void (*_smc_putstr_vec)(const char *);
extern void smc_putstr(const char *);

int net_init( void )
{
#if USE_QIO_ETH
 qio_eth_init();
#endif
 _smc_putstr_vec = _smc_putstr;
 return 0;
}


STATIC U32 net_loopback;

#define LOOPBACK_SIG OP_NETWORK_LOOPBACK

STATIC net_callback_fn net_callback;	/* Prototype to force parameter consistency  */
STATIC Net_CB_Result net_callback(	/* Compiler will balk if there is a mismatch */
        Net_CB_Func func,
        network_descriptor nd,
        packet_number pkt_no,
        U32 len,
        U32 param1,
        U32 param2,
        U32 param3
)
{
 int res = 0;
 U16 sig;
 switch (func)
 {
  case CB_Filter :
   sig = ((U8 *)param2)[6] | ( ((U8 *)param2)[7] << 8 );
   if ( net_loopback )
   {
    if ( ( memcmp( (char *)param2, &smc_data[0].my_nid, sizeof(netadd) ) == 0 )
      && ( sig == LOOPBACK_SIG ) )
     res = CB_Copy_Packet;
    else
     res = CB_Drop_Packet;
   }
   else
   {
    if ( sig == WHOSTHERE || sig == IMHERE || sig == TOITSELF )
     res = CB_Copy_Packet;
    else
     res = CB_Drop_Packet;
   }
   if ( res == CB_Copy_Packet )
   {
    if ( Net_Get_Buffer( (Net_Buffer **)param3, NET_BUFFER_SIZE, NETOPT_NONBLOCK ) != NETOK )
    {
     res = CB_Drop_Packet;
    }
   }
   break;

  case CB_Receive :
   net_que( (Net_Buffer *)param1 );
   res = CB_OK;
   break;

  case CB_TXResult:
   break;

  case CB_BfrRlse:
  case CB_BfrAquire:
  default:
   res = CB_FAILED;
   break;
 } /* endswitch */
 return res;
}

STATIC seq_num;
STATIC const netadd brdcast = {0xff,0xff,0xff,0xff,0xff,0xff};

#define LOOP_CB_CAPS ( CB_Will_Filter | CB_Will_Receive | NET_SEND_RAW )
#define LOOP_RETRIES 2

STATIC int net_loopback_test( int which )
{
 int i, res;
 Net_Buffer *nb, *rnb;
 network_descriptor pnd;
 U16 tx_control;

 i = Net_Initialize( prc_extend_bss(0), NET_BUFFER_MEMORY, (void(*))0, (void *)0 );
 if (i) return NET_ERR_INIT;

 net_loopback = TRUE;

 pnd = Net_Open_Interface("SMC0", net_callback, LOOP_CB_CAPS, 8, 6);
 if ( pnd == 0 ) return NET_ERR_OPEN;

 tx_control = smc_data[0].control.TX_control;

 if ( Net_Get_Buffer(&nb, NET_BUFFER_SIZE, NETOPT_NONBLOCK) != NETOK )
 {
  Net_Close_Interface(pnd);
  return NET_ERR_BUFF;
 }

 if ( ( which == OP_LOGICAL_LOOPBACK ) || ( which == OP_PHYSICAL_LOOPBACK ) )
 {
  smc_data[0].control.TX_control |= which;
  smc_data[0].status = SMC_Setup(&smc_data[0]);
 }

 nb->data[0] = LOOPBACK_SIG & 0xff;
 nb->data[1] = LOOPBACK_SIG >> 8;

 for ( i = 2; i < 238; ++i ) nb->data[i] = i;
 nb->len = 238;

 for ( i = 0; i < 10; ++i, ++seq_num )
 {
  nb->data[2] = (U8)seq_num;
  nb->data[3] = (U8)( seq_num >> 8 );
  res = Net_Send_Packet(pnd, smc_data[0].my_nid, nb, NETOPT_NONBLOCK);

  if ( res & NETOK_MASK )
  {
   int j = LOOP_RETRIES;

   while ( ( rnb = net_deq() ) == (Net_Buffer *)0 )
   {
    if ( --j ) prc_delay(0);
    else
    {
     Net_Close_Interface(pnd);
     Net_Free_Buffer(nb, NETOPT_NONBLOCK);
     return NET_ERR_LOOP;
    }
   }
   for ( j = 16; j < 250; j++)
   {
    if ( rnb->data[j] != ( j - 12 ) )
    {
     Net_Close_Interface(pnd);
     Net_Free_Buffer(rnb, NETOPT_NONBLOCK);
     Net_Free_Buffer(nb, NETOPT_NONBLOCK);
     return NET_ERR_DATA;
    }
   }
   Net_Free_Buffer(rnb, NETOPT_NONBLOCK);
  }
  else
  {
   Net_Close_Interface(pnd);
   Net_Free_Buffer(nb, NETOPT_NONBLOCK);
   return NET_ERR_SEND;
  }
 }

 Net_Close_Interface(pnd);
 Net_Free_Buffer(nb, NETOPT_NONBLOCK);

 if ( ( which == OP_LOGICAL_LOOPBACK ) || ( which == OP_PHYSICAL_LOOPBACK ) )
 {
  smc_data[0].control.TX_control = tx_control;
  if ( which == OP_LOGICAL_LOOPBACK )
  {
   extern int SMC_Reset_Chip(SMC_Data *smc);
   if (SMC_Reset_Chip(&smc_data[0])) return NET_ERR_RESET;
  }
  smc_data[0].status = SMC_Setup(&smc_data[0]);
 }

 net_loopback = FALSE;
 return 0;
}

#define N_RECS 8
#define N_CHAR 16
#define N_DATA 50

STATIC struct t_rec
{
 U32    sent;
 U32    recv;
 netadd node;
 U8     used;
 U8     pad_;
 U8	serial[N_CHAR];
 U8	g_text[N_CHAR];
 U8	g_item[N_CHAR];
} *test_summary;

STATIC struct t_rec *net_get_trec( netadd *nid )
{
 struct t_rec *tp;

 for ( tp = test_summary; tp < &test_summary[ N_RECS ]; ++tp )
 {
  if ( memcmp( &tp->node, nid, sizeof(netadd) ) == 0 ) return( tp );
  if ( tp->used == 0 )
  {
   tp->used = 1;
   memcpy( &tp->node, nid, sizeof(netadd) );
   return( tp );
  }
 }
 return ( (struct t_rec *)0 );
}


#define COMM_CB_CAPS ( CB_Will_Filter | CB_Will_Receive | CB_Will_TXResult | NET_SEND_RAW )

STATIC int net_comm_test( int which )
{
 static network_descriptor pnd;
 static Net_Buffer *nb;
 Net_Buffer *rnb;
 struct t_rec *tp;
 int res = 0, ierr;

 if ( which == OP_COMM_INIT )
 {
  ierr = Net_Initialize( prc_extend_bss(0), NET_BUFFER_MEMORY, (void(*))0, (void *)0 );
  if (ierr) return NET_ERR_INIT;
  pnd = Net_Open_Interface("SMC0", net_callback, COMM_CB_CAPS, 8, 6);
  if ( pnd == 0 ) return NET_ERR_OPEN;

  if ( Net_Get_Buffer(&nb, NET_BUFFER_SIZE, NETOPT_NONBLOCK) != NETOK )
  {
   Net_Close_Interface(pnd);
   return NET_ERR_BUFF;
  }

  memset( nb->data, 0, nb->len = N_DATA );

  nb->data[0] = IMHERE & 0xff;
  nb->data[1] = IMHERE >> 8;

  nsprintf( (char *)&nb->data[2], N_CHAR, "%s%s", GetMFG()[MFG_TYPE], GetMFG()[MFG_SERIAL] );
#if SST_GAME == SST_RUSH
# if NO_EER_WRITE == 0
  nsprintf( (char *)&nb->data[18], N_CHAR, "%s", GUTS_finds_car_color()[0] );
  nsprintf( (char *)&nb->data[34], N_CHAR, "%s", GUTS_finds_car_color()[1] );
# endif
#endif

  memset( test_summary, 0, N_RECS * sizeof(struct t_rec) );
  tp = net_get_trec( &smc_data[0].my_nid );
 }

 if ( which == OP_COMM_STOP )
 {
  Net_Free_Buffer(nb, NETOPT_NONBLOCK);
  Net_Close_Interface(pnd);
  prc_delay(2);
  while ( ( rnb = net_deq() ) != (Net_Buffer *)0 )
  {
   Net_Free_Buffer(rnb, NETOPT_NONBLOCK);
  }
  return 0;
 }

 if ( ( Net_Send_Packet(pnd, brdcast, nb, NETOPT_NONBLOCK) & NETOK_MASK ) == 0 )
 {
#if 0
  Net_Close_Interface(pnd);
  Net_Free_Buffer(nb, NETOPT_NONBLOCK);
#endif
  return NET_ERR_SEND;
 }

 tp = net_get_trec( &smc_data[0].my_nid );
 ++tp->sent;

 while ( ( rnb = net_deq() ) != (Net_Buffer *)0 )
 {
  netadd node;
  U16 sig = rnb->data[12] | ( rnb->data[13] << 8 );

  if ( sig == (U16)IMHERE )
  {
   memcpy( node, &rnb->data[6], sizeof(netadd) );
   tp = net_get_trec( &node );
   if ( tp )
   {
    ++tp->recv;
    strncpy( (char *)tp->serial, (char *)&rnb->data[14], N_CHAR );
    strncpy( (char *)tp->g_text, (char *)&rnb->data[30], N_CHAR );
    strncpy( (char *)tp->g_item, (char *)&rnb->data[46], N_CHAR );
   }
   else
    res = NET_ERR_NODE;
  }
  Net_Free_Buffer(rnb, NETOPT_NONBLOCK);
 }

 if ( res == 0 )
 {
  int i;
  for ( i = 0; i < N_RECS; ++i )
   if ( test_summary[ i ].used == 1 ) res = i;
 }

 return res;
}


STATIC void net_print_t_rec( struct t_rec *tp )
{
 char buff[ AN_VIS_COL ];

 nsprintf( buff, sizeof(buff), "SERIAL:  %s -- %s  %s\n", tp->serial, tp->g_text, tp->g_item ); 
 smc_putstr( buff );

 nsprintf( buff, sizeof(buff), "TRANSMITTED:  %10.10d\nRECEIVED:     %10.10d\n\n", tp->sent, tp->recv ); 
 smc_putstr( buff );
}


STATIC void net_print_error( int which )
{
 char *cp;
 switch ( which )
 {
  case NET_ERR_OPEN:  cp = " OPEN";  break;
  case NET_ERR_BUFF:  cp = " ALLOC";  break;
  case NET_ERR_SEND:  cp = " SEND";  break;
  case NET_ERR_LOOP:  cp = " LOOP";  break;
  case NET_ERR_DATA:  cp = " DATA";  break;
  case NET_ERR_NODE:  cp = " NODE";  break;
  case NET_ERR_INIT:  cp = " INIT";  break;
  case NET_ERR_RESET: cp = " RESET";  break;
  default:  cp = " UNKNOWN";
 }
 txt_cstr( cp, RED_PAL );
}


int net_test( smp )
const struct menu_d *smp;       /* Selected Menu Pointer */
{
 int res;
 int bottom = AN_VIS_ROW-2;

 bottom = st_insn(bottom, t_msg_ret_menu, t_msg_next, INSTR_PAL);

 test_summary = (struct t_rec *)(((int)prc_extend_bss(0) + NET_BUFFER_MEMORY + 15) & 0xFFFFFFFC);

 txt_str( 3, 3, "Testing local:", WHT_PAL );
 while ( ( ( res = net_loopback_test( OP_LOGICAL_LOOPBACK ) ) < 0 ) ||
         ( ( res = net_loopback_test( OP_PHYSICAL_LOOPBACK ) ) < 0 ) ||
         ( ( res = net_loopback_test( OP_NETWORK_LOOPBACK ) ) < 0 ) )
 {
  txt_str( 19, 3, "TRYING", YEL_PAL );
  net_print_error( res );
  prc_delay(0);
  if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) return 0;
 }
 txt_clr_str( 19, 3, "TRYING", YEL_PAL );
 txt_str( 19, 3, "PASSED", GRN_PAL );
 
#if (HOST_BOARD == FLAGSTAFF) || (HOST_BOARD == SEATTLE)
 txt_str( 35, 3, "Testing cable:", WHT_PAL );
 if ( smc_data[0].control.last_EPHSR & SMC_LINK_OK )
 {
  txt_str( 51, 3, "OK", GRN_PAL );
 }
 else
 {
  txt_str( 51, 3, "??", RED_PAL );
 }
#endif

 txt_str( 3, 5, "Testing link:", WHT_PAL );
 if ( ( res = net_comm_test( OP_COMM_INIT ) ) < 0 )
 {
  txt_str( 18, 5, "FAILED", RED_PAL );
  net_print_error( res );
  while ( ( ctl_read_sw(SW_NEXT) & SW_NEXT ) == 0 ) prc_delay(0);
  return 0;
 }

 while ( ( ctl_read_sw(SW_NEXT) & SW_NEXT ) == 0 )
 {
  struct t_rec *tp;
  int i;
#if (HOST_BOARD == FLAGSTAFF) || (HOST_BOARD == SEATTLE)
  if ( smc_data[0].control.last_EPHSR & SMC_LINK_OK )
  {
   txt_str( 51, 3, "OK", GRN_PAL );
  }
  else
  {
   txt_str( 51, 3, "??", RED_PAL );
  }
#endif
  if ( ( res = net_comm_test( OP_COMM_LINK ) ) < 0 )
  {
   net_comm_test( OP_COMM_STOP );
   if ( res == NET_ERR_NODE )
   {
    txt_str( 18, 5, "FOUND TOO MANY GAMES", RED_PAL );
   }
   else
   {
    txt_str( 18, 5, "ERROR", RED_PAL );
    net_print_error( res );
   }
   while ( ( ctl_read_sw(SW_NEXT) & SW_NEXT ) == 0 ) prc_delay(0);
   return 0;
  }
  else if ( res )
  {
   txt_clr_str( 18, 5, "WAITING", YEL_PAL );
   txt_str( 18, 5, "FOUND", GRN_PAL );
   txt_chexnum( res, 2, RJ_BF, GRN_PAL);
   txt_str( 26, 5, "GAME", GRN_PAL );
   if ( res > 1 ) txt_cstr( "S", GRN_PAL );
  }
  else
  {
   txt_str( 18, 5, "WAITING", YEL_PAL );
  }
  window(3, 7, AN_VIS_COL-8, bottom);
  tp = test_summary;
  for ( i = 0; i < N_RECS && tp->used; ++i, ++tp )
  {
   net_print_t_rec( tp );
  }
  prc_delay(0);
 }

 net_comm_test( OP_COMM_STOP );
 return 0;
}
