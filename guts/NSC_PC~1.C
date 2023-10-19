/* 	$Id: nsc_pcishim.c,v 1.3 1997/05/17 01:54:40 shepperd Exp $	 */

#if 0 && !defined(lint)
static const char vcid[] = "$Id: nsc_pcishim.c,v 1.3 1997/05/17 01:54:40 shepperd Exp $";
#endif /* lint */

#include <config.h>
#include <wms_proto.h>
#include <nsc_defines.h>
#include <nsc_pcicfig.h>
#include <nsc_gt64010.h>

#if !defined(PRINTF)
# define PRINTF(x)
#endif

/* a non-existent port to do a dummy store in PCI I/O space */
#define NOWHERE 0x80

/* functions to adapt Williams' PCI read functions to the NSC 415 driver */
/* and to the linux ide.c driver routines				*/
/* these routines mostly suck as they silently truncate addresses,	*/
/* assume endianness and do other nasty stuff				*/

int PCI_ReadConfigByte(DEVHANDLE devhand, BYTE regaddr)
{
  int i;

  i=get_pci_config_reg(devhand, (regaddr >> 2));
  i = i >> (8 * (regaddr & 0x03));
  return i & 0xff;
}

int PCI_ReadConfigWord(DEVHANDLE devhand, BYTE regaddr)
{
  int i;

  i = get_pci_config_reg(devhand, (regaddr >> 2));
  return ((regaddr&2) ? i>>16 : i) & 0xffff;
}

unsigned long PCI_ReadConfigDWord(DEVHANDLE devhand, BYTE regaddr)
{
  return get_pci_config_reg(devhand, (regaddr >> 2));
}

void PCI_WriteConfigByte(DEVHANDLE devhand, BYTE regaddr, BYTE regval)
{
  int old, new, mask, huh;

  old = get_pci_config_reg(devhand, (regaddr >> 2));
  huh = 8 * (regaddr&3);
  new = regval << huh;
  mask = 0xFF << huh;
  old &= ~mask;
  new &= mask;

  put_pci_config_reg(devhand, (regaddr>>2), old | new);
}

void PCI_WriteConfigWord(DEVHANDLE devhand, BYTE regaddr, WORD regval)
{
  U32 old, new, mask, huh;

  old = get_pci_config_reg(devhand, (regaddr >> 2));
  huh = (regaddr & 2) ? 16 : 0;
  new = regval << huh;
  mask = 0xFFFF << huh;
  new &= mask;
  old &= ~mask;

  put_pci_config_reg(devhand, (regaddr >> 2), old|new);
  
}

void PCI_WriteConfigDword(DEVHANDLE devhand, BYTE regaddr, DWORD regval)
{
  put_pci_config_reg(devhand, regaddr>>2, regval);
}

#if HOST_BOARD != CHAMELEON
int get_timer(int channel)
{
  int *timer_count = (int *)GALILEO_TIMER0;
  timer_count += channel;
  return (*timer_count & 0x00ffffff);
}

void set_timer(int channel, int value)
{
  int *timer_count = (int *)GALILEO_TIMER0;
  int i;

  /* mask the interrupt */
  *(int *)GALILEO_CPU_I_ENA &= ~(0x00000100 << channel);

  /* disable the right channel */
  *(int *)GALILEO_TIMER_CTL &= ~(0x03 << channel);

  /* set the value */
  timer_count += channel;
  *timer_count = value & 0x00ffffff;

  /* enable the right channel */
  i = (*(int *)GALILEO_TIMER_CTL) & ~(0x03); 
  i |= (0x01 << channel);
  (*(int *)GALILEO_TIMER_CTL) = i;
}

void delay_10ms()
{
  /* magic value: */
  /* 10 ms = 10000000 ns */
  /* 20 ns / clock tick  */
  /* 10000000 ns / 20 (ns/tick) = 500000 ticks */
  set_timer(0, 500000);

  /* loop until value is zero */
  while (get_timer(0))
    ;

  /* disable the channel */
  *(int *)GALILEO_TIMER_CTL &= ~(0x03);
  return;
}
#endif

void insw(unsigned short port, unsigned short *addr, unsigned long count)
{
  unsigned short *p = (unsigned short *)(PCI_IO_BASE + port);
  while( count-- > 0) *addr++ = *p;
}

void outsw(unsigned short port, const unsigned short * addr, unsigned long count)
{
  unsigned short *p = (unsigned short *)(PCI_IO_BASE + port);
  while ( count-- > 0) *p = *addr++;
}

/* io byte output with pause */
void outb_p (BYTE val, unsigned short port)
{
  *((BYTE *)(PCI_IO_BASE + port)) = val;
  /* delay some */
  *((BYTE *)(PCI_IO_BASE + NOWHERE)) = 0x00;
}

/* io input byte with pause */
BYTE inb_p (unsigned short port)
{
  BYTE tmp;

  tmp = *((BYTE *)(PCI_IO_BASE + port));

  /* delay some */
  *((BYTE *)(PCI_IO_BASE + NOWHERE)) = 0x00;

  return(tmp);
}

/* io byte output with no pause */
void outb (BYTE val, unsigned short port)
{
#if HOST_BOARD != CHAMELEON
  T_gt64010_pci_config_reg1 foo;

  *((BYTE *)(PCI_IO_BASE + port)) = val;

  foo.data = get_pci_config_reg(GT64010_DEVICE_NUMBER, 0x04);
  if (foo.d.det_par_err \
      | foo.d.tar_abort \
      | foo.d.mas_abort \
      | foo.d.sys_err) {
    PRINTF(("PCI IO byte write error (address=%4x, data=%2x, status=%8x)\n", \
	   port, val, foo));
    *((BYTE *)(PCI_IO_BASE + port)) = val;
  }
#else
  *((BYTE *)(PCI_IO_BASE + port)) = val;
#endif
}

/* io input byte with no pause */
BYTE inb (unsigned short port)
{
#if HOST_BOARD != CHAMELEON
  BYTE tmp;
  T_gt64010_pci_config_reg1 foo;

  tmp = *((BYTE *)(PCI_IO_BASE + port));

  foo.data = get_pci_config_reg(GT64010_DEVICE_NUMBER, 0x04);
  if (foo.d.det_par_err \
      | foo.d.tar_abort \
      | foo.d.mas_abort \
      | foo.d.sys_err) {
    PRINTF(("PCI IO byte read error (address=%4x, data=%2x, status=%8x)\n", \
	   port, tmp, foo));
  }
  return(tmp);
#else
  return *((BYTE *)(PCI_IO_BASE + port));
#endif
}
