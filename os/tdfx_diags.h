#ifndef _TDFX_DIAGS_H_
#define _TDFX_DIAGS_H_

extern int done_it_once;
#define DONE_ONCE_V (0)
#define DONE_FBI_V  (DONE_ONCE_V+1)
#define DONE_FBI_M  (3)
#define DONE_TMU_V  (DONE_FBI_V+2)
#define DONE_TMU_M  (3)
#define DONE_3DF_V  (DONE_TMU_V+2)
#define DONE_3DF_M  (3)
#define DONE_ONCE (done_it_once&(1<<DONE_ONCE_V))
#define DO_FBI_TEST  (((done_it_once>>DONE_FBI_V)&DONE_FBI_M) == 2)
#define DO_TMU_TEST  (((done_it_once>>DONE_TMU_V)&DONE_TMU_M) == 2)
#define DO_3DF_TEST  (((done_it_once>>DONE_3DF_V)&DONE_FBI_M) == 2)

extern int getch(void);
extern int kbhit(void);
extern int testYesNo(void);
extern int scope_loop_q;

#endif _TDFX_DIAGS_H_
