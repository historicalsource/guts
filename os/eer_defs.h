#define EER_CC0 (0)	/* Coin mech 1 */
#define EER_CC1 (1)	/* Coin mech 2 */
#define EER_CC2 (2)	/* Coin mech 3 */
#define EER_CC3 (3)	/* Coin mech 4 */
#define EER_CCB (4)	/* Bill acceptor */
#define EER_CCX (5)	/* Service credit switch */
#define EER_0PTIME (6)	/* idle time */
/* 	^^^^^^----- NOTE: bucket moves every 256 of these */
#define EER_NEWCOINS (7)	/* new games */
#define EER_CONTCOINS (8)	/* cont games */
#define EER_ERRORS (9)	/* count of EEPROM errors */
#define EER_CNOPT (10)	/* coin options */
#define EER_CNVAL (11)	/* Coin price options */
#define EER_GMOPT (12)	/* game options */
#define EER_GUTS_OPT (13)	/* GUTS options, incl. attract mute */
#define EER_RESET (14)	/* PROC non-powerup RESETS */
#define EER_WR_RO (15)	/* PROC write to read only mem */
#define EER_RD_NEM (16)	/* PROC read from non-existant mem */
#define EER_WR_NEM (17)	/* PROC write to non-existant mem */
#define EER_ADRERR_R (18)	/* PROC address alignment error on read */
#define EER_ADRERR_W (19)	/* PROC address alignment error on write */
#define EER_BUSERR_I (20)	/* PROC bus error on I fetch */
#define EER_BUSERR_D (21)	/* PROC bus error on D fetch */
#define EER_SYSCALL (22)	/* PROC SYSCALLS */
#define EER_BREAKP (23)	/* PROC Breakpoints */
#define EER_ILGINS (24)	/* PROC reserved instructions */
#define EER_COPROC (25)	/* PROC coprocessor unusables */
#define EER_ARITH (26)	/* PROC arithmetic overflows */
#define EER_TRAP (27)	/* PROC trap exceptions */
#define EER_RESERV (28)	/* PROC reserved exceptions */
#define EER_FLOAT (29)	/* PROC floating point exceptions */
#define EER_UNDEF (30)	/* PROC undefined exceptions */
#define EER_OVERFL (31)	/* PROC overflow checks */
#define EER_DVDBY0 (32)	/* PROC divide by 0 */
#define EER_RANGE (33)	/* PROC range checks */
#define EER_UHINT (34)	/* PROC unhandled interrupt */
#define EER_MOVERFL (35)	/* PROC multiply overflow */
#define EER_DATALST (36)	/* SOFT sound data lost */
#define EER_AUDRESET (37)	/* SOFT sound proc resets */
#define EER_LINKFAIL (38)	/* SOFT LINK failures */
#define EER_DSK_ERR (39)	/* DISK any disk error */
#define EER_DSK_AMNF (40)	/* DISK address not found */
#define EER_DSK_TK0NF (41)	/* DISK trk 0 not found */
#define EER_DSK_ABORT (42)	/* DISK command abort */
#define EER_DSK_IDNF (43)	/* DISK ID not found */
#define EER_DSK_UNCDTA (44)	/* DISK uncorr data error */
#define EER_DSK_TIMOUT (45)	/* DISK device timeout */
#define EER_DSK_WERR (46)	/* DISK write fault */
#define EER_DSK_CORR (47)	/* DISK correctable data err */
#define EER_FSYS_USEALT (48)	/* FSYS Used alternate file */
#define EER_AUD_VOL (49)	/* audio volume level */
#define EER_POT0L (50)	/* Pot 0 - low limit */
#define EER_POT1L (51)	/* Pot 1 - low limit */
#define EER_POT2L (52)	/* Pot 2 - low limit */
#define EER_POT3L (53)	/* Pot 3 - low limit */
#define EER_POT4L (54)	/* Pot 4 - low limit */
#define EER_POT5L (55)	/* Pot 5 - low limit */
#define EER_POT6L (56)	/* Pot 6 - low limit */
#define EER_POT7L (57)	/* Pot 7 - low limit */
#define EER_POT0H (58)	/* Pot 0 - high limit */
#define EER_POT1H (59)	/* Pot 1 - high limit */
#define EER_POT2H (60)	/* Pot 2 - high limit */
#define EER_POT3H (61)	/* Pot 3 - high limit */
#define EER_POT4H (62)	/* Pot 4 - high limit */
#define EER_POT5H (63)	/* Pot 5 - high limit */
#define EER_POT6H (64)	/* Pot 6 - high limit */
#define EER_POT7H (65)	/* Pot 7 - high limit */
#define N_TRIES (4)	/* number of retries on write */
#define EEPROM_SIZE (2040)	/* in bytes */
#define STRIDE (2)	/* shift amount eeprom addressing */
#define MAX_STAT (65)	/* Highest legit stat number */
extern int eer_gets(/* unsigned int stat_num */); /* ret: val or -1 = error*/
extern int eer_puts(/* unsigned int stat_num,val*/);/* ret: 0 = ok, -1 = error*/
extern int eer_incs(/* unsigned int stat_num,val*/);/* ret: val or -1 = error */
extern int eer_init();	/* returns total errors, if EER_ERROR defined */
extern int eer_busy();	/* returns !0 if some write still in progress */
/* You have one "orphan" nybble */
#define LAST_BUCKET (131)
