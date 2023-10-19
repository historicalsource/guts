PROCESSOR = r4kl

RCS_PATH = RCS
SPATH = .	# point this to the source tree(s)
HPATH = .:$(R4KL_INCLUDE_PATH):$(R4KL_INCLUDE_PATH_HW)
GUTS_RCS = RCS

DEST = /usr/local/gnu/IRIX/mips-97q1/testlib

# We need the definition below to defeat the brain-dead csh built-in
ECHO = /bin/echo		# than all the others

BELL	= $(ECHO) "\007" > /dev/tty
.SILENT:

CINCS 	= -I$(subst :, -I,$(HPATH))
COPT	= -O
DEFS	= -DGET_LOCAL_DEFINES=1
CDEBUG	= -g
CC	= gcc$(PROCESSOR)
CFLAGS  = $(CINCS) $(COPT) $(CDEBUG) $(DEFS)
CFLAGS += -mips3 -mgp32 -mfp64 -G 0 -Wa,-32 -fshort-double -Wall -Wno-trigraphs -ansi -pedantic 

AR = ar$(PROCESSOR)

include Makefile.dms

default : libcledice.a libcled.a

ifeq ("$(FROM_RCS)","")
Makefile.dms : $(RCS_PATH)/Makefile.dms,v
	$(ECHO) "\tChecking out $@..."
	co -q $@
else
Makefile.dms : $(RCS_PATH)/Makefile.dms,v
	$(FROM_RCS)
endif

H_FILES =  cled_proto.h cled_defines.h cled_io.h cled_ioctl.h
H_FILES += cled_termios.h 
IH_FILES = iio_proto.h icelesspkt.h

O_FILES = cled_edit.o cled_defaults.o icecled.o

libcledice.a : $(O_FILES)
	@$(ECHO) "\tBuilding $@ ..."
	-/bin/rm -f $@
	-$(AR) rusv $@ $^

libcled.a : cled_edit.o cled_defaults.o
	@$(ECHO) "\tBuilding $@ ..."
	-/bin/rm -f $@
	-$(AR) rusv $@ $^

ALLDEPENDS =	$(patsubst %.o,%.c,$(O_FILES))

depends : $(ALLDEPENDS)
	$(DEPENDS_RULE)

cled_edit.o: cled_edit.c $(H_FILES) $(IH_FILES)

cled_defaults.o: cled_defaults.c $(H_FILES) $(IH_FILES)

icecled.o: icecled.c $(H_FILES) $(IH_FILES)

install : libcled.a libcledice.a makefile 
	cp $(filter %.a,$^) $(DEST)

clean:
	rm -f *.o *.a *.err
