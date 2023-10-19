#define WRAP_VAL (6)	/* bucket moves every 256 of these */
#define EER_ERRORS (9)	/* count of EEPROM errors */
#define N_TRIES (4)	/* number of retries on write */
#define EEPROM_SIZE (2040)	/* in bytes */
#define STRIDE (2)	/* shift amount eeprom addressing */
#define SIGNATURE (0x294EE1D4)	/* "unique" signature for this game */
#define MAX_STAT (65)	/* Highest legit stat number */
extern int eer_gets(/* unsigned int stat_num */); /* ret: val or -1 = error*/
extern int eer_puts(/* unsigned int stat_num,val*/);/* ret: 0 = ok, -1 = error*/
extern int eer_incs(/* unsigned int stat_num,val*/);/* ret: val or -1 = error */
extern int eer_init();	/* returns total errors, if EER_ERROR defined */
extern int eer_busy();	/* returns !0 if some write still in progress */
/* You have one "orphan" nybble */
#define BUCKET_NUM (82)
#define MAX_RUN (8)	/* in bytes */
#define RUN_BYTES (9)
#define RUN_LEN (14)
#define RUN_REC (0)
#define SIGNAT_POSN (83)
#define MAX_HOME (173)	/* in NYBBLES */
#define HOME_BYTES (87)
#define MAX_DATA (88)	/* Largest record, in bytes */
#define ALT_OFFS (95)
#define HOME_REC (1)
#define ALT_REC (2)
#define SIZ_MASK (7)
#define RUN_MASK (15)
#define HOME_MASK (255)
#define RUN_POSN (0)
#define HOME_POSN (4)
#define SIZ_POSN (12)
#define INC_ABLE (32768)
#define MAP_TYPE unsigned short
#define DEF_CTR(HSZ,HP,RP,LABEL) (INC_ABLE|(HSZ<<SIZ_POSN)|(HP<<HOME_POSN)|(RP)),
#define DEF_OPT(HSIZE,HPOS,RPOS,LABEL) ((HSIZE<<SIZ_POSN)|(HPOS<<HOME_POSN)|(RPOS)),
#define FIRST_BUCKET (190)	/* start of "running" area */
#define MAX_CHECK (8)
#define LAST_BUCKET (131)
