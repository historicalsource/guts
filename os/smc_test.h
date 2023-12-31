

#define BANK0   0x0000
#define BANK1   0x0001
#define BANK2   0x0002
#define BANK3   0x0003

#define BANKSEL   (0x0e)
#define TCR       (0x00)       /* bank 0 registers */
#define STAT      (0x02)
#define RCR       (0x04)
#define COUNTER   (0x06)
#define MIR       (0x08)
#define MCR       (0x0a)
#define SMCTEST   (0x0c)
#define CONFIG    (0x00)       /* bank 1 registers */
#define BASE      (0x02)
#define IA10      (0x04)
#define IA32      (0x06)
#define IA54      (0x08)
#define GENERAL   (0x0a)
#define CONTROL   (0x0c)
#define MMUCOM    (0x00)       /* bank 2 registers */
#define ARRPNR    (0x02)
#define FIFOPORTS (0x04)
#define POINTER   (0x06)
#define DATA1     (0x08)
#define DATA2     (0x0a)
#define INTERRUPT (0x0c)
#define MT10      (0x00)       /* bank 3 registers */
#define MT32      (0x02)
#define MT54      (0x04)
#define MT76      (0x06)

#define EPHLOOP    0x2000       /* tcr register bits */
#define STPSQET    0x1000
#define MONCSN     0x0400
#define FDUPLX     0x0800
#define LOOP       0x0002
#define TXENA      0x0001
#define NOCRC      0x0100
#define TXUNRN     0x8000       /* eph status bits */
#define LOSTCAR    0x0400
#define LATCOL     0x0200
#define SQET       0x0020
#define SIXTEENCOL 0x0010
#define TXSUC      0x0001
#define EPHRST     0x8000       /* rcr register bits */
#define STRIPCRC   0x0200
#define RXEN       0x0100
#define ALMUL      0x0004
#define PRMS       0x0002
#define PLLGAIN    0x0c00
#define FILTCAR    0x4000
#define AUISELECT  0x0100       /* configuration register bits */
#define SIXTEENBIT 0x0080
#define DISLINK    0x0040
#define INTSEL     0x0006
#define NOWAITST   0x1000
#define FULLSTEP   0x0400
#define SETSQLCH   0x0200
#define PWRDWN     0x2000       /* control register bits */
#define RCVBAD     0x4000
#define AUTORELEASE 0x0800
#define EEPROMSEL  0x0004
#define RELOAD     0x0002
#define STORE      0x0001
#define BUSY       0x0001       /* mmu command register bits */
#define ALLOCTX    0x0020 
#define RESETMMU   0x0040
#define REMOVERX   0x0060
#define RELERX     0x0080
#define RELEPNR    0x00a0
#define QUEUETX    0x00c0
#define FAILALL    0x8000      /* pnrarr alloc fail bit   */
#define REMPTY     0x8000      /* fifoports register bits */
#define TEMPTY     0x0080
#define RXRD       0xe000      /* pointer register bits */
#define RXWR       0xc000  
#define TXRD       0x6000
#define TXWR       0x4000
#define EPHINT     0x0020      /* interrupt status register bits */
#define TXINT      0x0002
#define RXINT      0x0001
#define ALLOCINT   0x0008
#define RXOVRN     0x0010
#define ALGNERR    0x8000
#define BADCRC     0x2000
#define EPHINTMASK 0x2000
#define TXINTMASK  0x0200
#define RXINTMASK  0x0100

#define EPHERR       0x8630
#define TCRMSK       0x3d87
#define RCRMSK       0x4fff
/*
#define MCRMSK       0x00ff
*/
#define MCRMSK       0x001f
/*
#define CONFIGMSK    0x1068
*/
#define CONFIGMSK    0x1040
#define BASEMSK      0x00fe
#define IA10MSK      0xffff
#define IA32MSK      0xffff
#define IA54MSK      0xffff
#define GENERALMSK   0xffff
/*
#define CONTROLMSK   0x49e0
*/
#define CONTROLMSK   0x48e0
#define ARRPNRMSK    0x001f
/*
#define POINTERMSK   0xe700
*/
#define POINTERMSK   0xe7fe
#define DATA1MSK     0xffff
#define DATA2MSK     0xffff
#define INTERRUPTMSK 0x3f00
#define MT10MSK      0xffff
#define MT32MSK      0xffff
#define MT54MSK      0xffff
#define MT76MSK      0xffff

#define REGX1   1
#define REGY1   1
#define REGX2   80
#define REGY2   10
#define TXX1    1
#define TXY1    11
#define TXX2    80
#define TXY2    13
#define RCVX1   1
#define RCVY1   14
#define RCVX2   80
#define RCVY2   16
#define STATX1  1
#define STATY1  17
#define STATX2  80
#define STATY2  21

#define ERRX1   1
#define ERRY1   20
#define ERRX2   80
#define ERRY2   21
#define RWX1    1
#define RWX2    80
#define RWY1    11
#define RWY2    21
#define PROMX1   4
#define PROMX2  75
#define PROMY1   7
#define PROMY2  23
#define TASKX1   1
#define TASKY1  20
#define TASKX2  80
#define TASKY2  25
 
#define F1      187
#define SFTF1   212
#define F2      188
#define F3      189
#define ALTF3   234
#define CTLF3   224
#define SFTF3   214
#define F4      190
#define SFTF4   215
#define F5      191
#define SFTF5   216
#define CTLF5   226
#define ALTF5   236
#define F6      192
#define SFTF6   217
#define CTLF6   227
#define ALTF6   237
#define F7      193
#define SFTF7   218
#define ALTF7   238
#define F8      194
#define SFTF8   219
#define ALTF8   239
#define F9      195
#define F10     196
#define UPARROW 200
#define DNARROW 208
#define LFARROW 203
#define RTARROW 205
#define ESC     27
#define CR      13
#define END     207
#define VERT    '|'
#define HORZ    '-'
#define UPLEFT  '+'
#define DNLEFT  '+'
#define UPRIGHT '+'
#define DNRIGHT '+'

#define CLEAR      0x0000
#define ON         1
#define OFF        0
#define PASS       1
#define FAILED     0
#define FAILREG    0x10
#define FAILRAM    0x20
#define FAILLOPF   0x30
#define FAILPONGAUI 0x40
#define FAILPONGBT 0x50
#define FAILPROM   0x60
#define FORHALFSEC 1
#define FOREVER    0
#define NORMAL     1
#define RXINTED    1
#define TXINTED    1
#define HILITE     1
#define LOLITE     0
#define TOITSELF   0xaaaa
#define COLLIDE    0xb9c7
#define WHOSTHERE  0xb9c8
#define IMHERE     0xb9c9
#define MEDIAAUI   0xb9ca
#define MEDIABT    0xb9cb
#define READ       0
#define WRITE      1
#define INTR0      0x71
#define INTR1      0x0b
#define INTR2      0x72
#define INTR3      0x73

