#include <stdio.h>
#include <config.h>
#include <os_proto.h>
#include <stdlib.h>
#include <string.h>
#include <qio.h>
#include <fsys.h>
#include <tdfx_diags.h>

#define n_elts(x) (sizeof(x)/sizeof(x[0]))

extern int fx_tests(int argc, char *argv[]);
extern int tmu_memtest(int argc, char *argv[]);
extern int fbi_memtest(int argc, char *argv[]);
extern void tdfx_init(void);

static int fix_args(char *buf, int max, char *argv[]) {
    int argc = 0;
    if (!buf) {
	if (argv) *argv = 0;
	return 0;
    }
    while (max > 0 && *buf) {
	while (*buf == ' ' || *buf == '\t') ++buf;
	if (*buf) {
	    *argv++ = buf;
	    ++argc;
	    --max;
	    while (*buf && *buf != ' ' && *buf != '\t' && *buf != '\r' && *buf != '\n') ++buf;
	    if (!*buf || *buf == '\r' || *buf == '\n') {
		*buf = 0;
		break;
	    }
	    *buf++ = 0;
	}
    }	
    return argc;
}

#if INTERACTIVE_MODE
extern char *diag_get_line(const char *prompt);
#ifndef DIAGS_PROMPT
# define DIAGS_PROMPT "Diags>"
#endif

static char * const help_msg[] = {
    "Standalone diags (sort of). Version " __DATE__ " " __TIME__,
"All commands can be abbreviated to one or more characters and are insensitive
to case (they are listed in uppercase for clarity). In the event of ambiguity,
the command listed first takes precedence. The 3dfx command has options unique
to it. Type \"3dfx -?\" for details on those options. Other commands may
accept or require a \"range\" as a parameter. The syntax for \"range\" is
either: \"start\" or \"start LENGTH n\" or \"start TO end\". The keywords
LENGTH and TO may also be abbreviated. The 'n' after LENGTH specifies the
number of bytes, the 'end' after TO specifies the (inclusive) end address.
Except for the 3dfx command, all numbers appearing as arguments to a command
are expected to be hexidecimal.",
    "",
    "The disk has to be working to run the 3dfx tests.",
    "You have to press reset to abort scope loops.",
    "",
    "The following commands are available:",
    "HELP (or ?) - Displays this message",
    "3DFX [options] - Run 3dfx tests. Type 3dfx -? for list of options.",
    "TMU [options] - Run 3dfx TMU memory test. Type TMU -? for options.",
    "FBI [options] - Run 3dfx FBI memory test. Type FBI -? for options.",
    "READ range - Read from memory (scope loop)",
    "WRITE range [data0 [data1...]] - Write data to memory (scope loop).",
    "RW range [data0 [data1...]] - Write data then read (scope loop)",
    "DUMP range - Display memory (32 bits at a time only)",
    "SET range data0 [data1 ...] - Write data (32 bit) to memory (once).",
    0
};

static int help(int argc, char *argv[]) {
    char * const *s = help_msg;
    while (*s) {
	fputs(*s++, stderr);
	fputs("\r\n", stderr);
    }
    return 0;
}

static const struct cmds {
    char *name;
    int (*func)(int c, char *v[]);
    int param;
} commands[] = {
#if 1 || HOST_BOARD != PHOENIX_AD
    { "3dfx", fx_tests, 0 },	/*!!!! WARNING: THIS ONE MUST BE FIRST  !!!!*/
#endif
    { "tmu", tmu_memtest, 0 },
    { "fbi", fbi_memtest, 0 },
    { "help", help, 0 },
    { "?", help, 0 },
    { 0, 0}
};
#endif

#include <setjmp.h>

static jmp_buf beginning;
static int set_beginning;
extern void testCleanUp(void);

void exit(int arg) {
    fprintf(stderr, "Task 'exit'ed with status of %d\r\n", arg);
#if 1 || HOST_BOARD != PHOENIX_AD
    testCleanUp();
#endif
    if (set_beginning) longjmp(beginning, 1);
    fprintf(stderr, "sejmp not set, cannot continue...");
    while (1) __asm__("break");
}

#if INTERATIVE_MODE
# define PREFIX "Cmds: "
#else
# define PREFIX
#endif

#if 1 || HOST_BOARD != PHOENIX_AD

static int hd_inited;

static int init_drive(void) {
    int sts;
    extern int ide_init(void);
    fprintf(stderr, PREFIX "Setting up code to use the disk drive...");
    if ((sts=ide_init())) {
	fprintf(stderr, "\r\n" PREFIX "Disk drive is not responding\r\n");
    } else {
	fprintf(stderr, "\r\n" PREFIX "Mounting the filesystem...");
	sts = fsys_mountw("/rd0", "/d0");
	fprintf(stderr, "\r\n");
	if (QIO_ERR_CODE(sts)) {
	    char msg[AN_VIS_COL_MAX];
	    qio_errmsg(sts, msg, sizeof(msg));
	    fprintf(stderr, PREFIX "%s\r\n", msg);
	} else {
	    hd_inited = 1;
	    sts = 0;
	}
    }
    return sts;
}
#endif

int done_it_once;

#if !INTERACTIVE_MODE
extern int testYesNo(void);
static int loop_count, error_counts;
#endif

int tdfx_cmd_loop(void) {
    int argc;
    char *argv[128];

    tdfx_init();		/* init the shim file system and the uart */

    if (!done_it_once) fprintf(stderr, "\r\n");	/* first time through, clear line */

#if !INTERACTIVE_MODE
    if (setjmp(beginning)) {
	fprintf(stderr, "\r\n" PREFIX "Fatal errors prevent continuation. Press reset to start over...");
	while (1) { ; }
    }
    while (1) {
	int sts, errs;
	errs = 0;
	argc = fix_args("fbi", n_elts(argv), argv);
	sts = fbi_memtest(argc, argv);
	if (!DONE_ONCE) {	/* the first time we check to see whether we should continue */
	    if (sts) {		/* test failed, ask if other tests should be tried */
		fprintf(stderr, "Because FBI memory test failed, all other 3dfx tests will very likely\r\n");
		fprintf(stderr, "fail also. Do you want to try to run them anyway? (Y/[N]): ");
		if (testYesNo()) {
		    done_it_once |= 2<<DONE_TMU_V;
		} else {
		    done_it_once |= 3<<DONE_TMU_V;
		}
		fputs("\r\n", stderr);
	    } else {
		done_it_once |= 2<<DONE_TMU_V;
	    }
	}
	if (sts) ++errs;
	if (DO_TMU_TEST) {
#if COBBLE_TMU_TEST
	    if (!DONE_ONCE) {
		printf("Note: This cobbled TMU test does not check TMU 1 with random\n");
		printf("   data patterns since that test seems to fail every board.\n");
	    }
#endif
	    argc = fix_args("tmu", n_elts(argv), argv);
	    sts = tmu_memtest(argc, argv);
	    if (!DONE_ONCE) {	/* the first time we check to see whether we should continue */
		if (sts) {	/* the test failed, ask if other tests should be tried */
		    fprintf(stderr, "Because TMU memory test failed, other 3dfx tests will very likely\r\n");
		    fprintf(stderr, "fail also. Do you want to try to run them anyway? (Y/[N]): ");
		    if (testYesNo()) {
			done_it_once |= 2<<DONE_3DF_V;
		    } else {
			done_it_once |= 3<<DONE_3DF_V;
		    }
		    fputs("\r\n", stderr);
		} else {
		    done_it_once |= 2<<DONE_3DF_V;
		}
	    }		    
	    if (sts) ++errs;
#if 1 || HOST_BOARD != PHOENIX_AD
	    if (DO_3DF_TEST) {
		if (!DONE_ONCE) {
		    sts = init_drive();
		    if (sts) {
			fprintf(stderr, "Because hard disk not working, other 3dfx tests cannot be run\r\n");
			done_it_once |= 3<<DONE_3DF_V;	/* disable 3dfx tests */
		    }
		}
		if (DO_3DF_TEST) {
		    argc = fix_args("3dfx", n_elts(argv), argv);
		    sts = fx_tests(argc, argv);
		    if (sts) ++errs;
		}
	    }
#endif
	}
#if 1 || HOST_BOARD != PHOENIX_AD
	if (!DONE_ONCE) {
	    extern int getch(void);
	    fprintf(stderr, "Press any key to continously repeat tests: ");
	    getch();
	    fprintf(stderr, "\r                                               \r");
	}
#endif
	if (errs) ++error_counts;
	if (DONE_ONCE) {
	    fprintf(stderr, "Total passes: %d, Total passes with errors: %d\r\n", loop_count+1, error_counts);
	}
	done_it_once |= 1<<DONE_ONCE_V;
	++loop_count;
    }
    return 0;
#else
    if (!setjmp(beginning)) {
	set_beginning = 1;	/* been through here */
    }

    while (1) {
	char *str;
	int ii;

	str = diag_get_line(DIAGS_PROMPT);
	fputs("\r\n", stderr);
	argc = fix_args(str, n_elts(argv), argv);
	if (!argc) continue;
#if 0
	{
	    char **strp;
	    fprintf(stderr, "Found %d strings:\r\n", argc);
	    strp = argv;	
	    for (ii=0; ii < argc; ++ii) {
		fprintf(stderr, "%2d: %s\r\n", ii, *strp++);
	    }
	}
#endif
	for (ii=0; ii < n_elts(commands); ++ii) {
	    int jj;
	    if (!commands[ii].name) {
		fprintf(stderr, "Unknown command: %s\r\n", argv[0]);
		break;
	    }
	    jj = strlen(argv[0]);
	    if (strncmp(commands[ii].name, argv[0], jj) == 0) {
		if (!ii && !hd_inited) {
		    if (init_drive()) {
			fprintf(stderr, "Sorry, disk drive required to run 3dfx diags.\r\n");
			break;
		    }
		}
		ii = commands[ii].func(argc, argv);
# if 0
		fprintf(stderr, "Command returned status of %d\r\n", ii);
		{
		    extern int junk_free_size;
		    fprintf(stderr, "junk_free_size = %d\r\n", junk_free_size);
		}
# endif
		break;
	    }
	}
    }
#endif
}
