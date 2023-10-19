#	$Id: Makefile,v 1.52 1997/10/16 01:51:59 shepperd Exp $
#
#	This makefile will only work with GNU make.

GUTS = ../os

#
# This makes a decompression boot image. Set the next two variables to
# the places to find the eprom selftest and game elf files respectively.
#
CBOOT_SRC = ../os/os.elf
#VMUNIX_SRC = /home/wiz/usr5/rogers/Mace/disk/CODE/vmunix
#VMUNIX_SRC = /home/wiz/usr5/rogers/code/Obj/mace.elf
#VMUNIX_SRC = /home/meat/usr2/Mace/disk/CODE/vmunix
#VMUNIX_SRC = /home/wiz/usr2/people/roberto/code/Obj/mace.elf
#VMUNIX_SRC = /home/earthpig/usr/people/roberto/code/Obj/mace.elf

# Point to the various source directories
GUTS_RCS  = RCS

STD_INCLUDE = ${R4KL_INCLUDE_PATH}/
STD_STDDEF  = ${R4KL_INCLUDE_PATH_HW}/

# Point to files used by the default rules

DEF_PP	  = def_pp.mac
DATE	  = #sst_date.c

# Tell make where to look for sources

RCSPATH = $(GUTS_RCS)		# point this to the RCS tree(s)
SPATH = .					# point this to the source tree(s)
HPATH = .:$(GUTS):$(STD_INCLUDE):$(GLIDE_INCLUDE_PATH)
MPATH = .

# We need the definition below to defeat the brain-dead csh built-in
ifeq ($(shell uname),"SunOS")
	ECHO = /usr/5bin/echo		# Sun's are different
else
	ECHO = /bin/echo		# than all the others
endif

BELL	= $(ECHO) "\007" > /dev/tty

#
# Build a list of include paths from those defined in HPATH for CPP
# and for MACPP
#
CINCS	= -I$(subst :, -I,$(HPATH))
PPINCS	= -include=$(subst :, -include=,$(MPATH))

# Define what and where to find the tools

SHELL		= /bin/sh
DEL		= rm -f
PP		= macpp
LLF		= llf
PROCESSOR	= r4kl
CC		= gccr4kl
LD 		= ldr4kl
ASM		= asr4kl
AS		= asr4kl
OUTPUT_LANG 	= LANG_ASRxk
CO		= co
MIXIT		= mixit #-q
ELF2ZO		= elf2zo

# Misc utilitiy flags

COFLAGS 	= -q

# Assembler and MACPP flags

PPFLAGS		= $(PPINCS) #-lis	# set to -lis if you also want listings with MACPP'd sources
ASM_PPFLAGS	= $(PPINCS) -line #-lis	# ditto, but leave the -line if you want asxx to report the correct
ASFLAGS		= -mips3 -G 0 -32
POUT		= -out=

# Compiler flags

DEFS		= -DBUILTIN_MEMCPY -DDEBUG -DZ_PREFIX #-DJUST_TEST
DBG		= -g3
OPT		= -O2 $(DBG)
CFLAGS		 = $(OPT) $(CINCS) $(DEFS) -Wall -ansi -pedantic -G 0
CFLAGS		+= -fshort-double -mips3 -mgp32 -mfp64 -Wa,-32

# Linker flags

LDFLAGS		= 
LIBDIRS		= -L${R4KL_LIBRARY_PATH} -L${GLIDE_LIBRARY_PATH}
LIBS		= -lz -lmf -lcf -lgcc

# All rules

include Makefile.dms

define ZO_RULE
	$(ECHO) "	Making $@ ..."
	$(ELF2ZO) EXTRAS $< $@
endef

#	@ before line does not echo command line
#	.SILENT: makes 'make' not echo command lines through out file
.SILENT:

O		= .o

BASE_H_FILES = config.h os_proto.h st_proto.h
CONFIG_FILES =  config.mac constants.mac pptypes.mac
CONFIG_FILES += seattle.mac galileo.mac 
CONFIG_FILES += idt.mac idt_vectors.mac ioasic.mac

OS_ROOT_FILE = phx_root

OBJ_FILES = $(OS_ROOT_FILE)$O idtc4000$O decompboot$O mallocr$O 

default : install #os.elf 
	@$(ECHO) "	Done..."

decompbt.dio : os.elf
	@echo "\tMixiting to $@..."
	@mixit -q os.mix
	@dio_cksum os.dio $@ 0x7fffc 0x01020304
	@dd if=$@ of=$(basename $(@F)).bin bs=16 skip=1 count=65536

install : decompbt.dio
	@echo "FTP'ing to volta..."
	@echo "cd /Projects/SubSys/seattle/sadiags" > /tmp/$$$$; \
	 echo "bin" >> /tmp/$$$$; \
	 echo "hash" >> /tmp/$$$$; \
	 echo "put $<" >> /tmp/$$$$ ; \
	 echo "put $(basename $<).bin" >> /tmp/$$$$ ; \
	 echo "bye" >> /tmp/$$$$; \
	 ftp volta < /tmp/$$$$; \
	 rm -f $$$$

Makefile.dms : $(GUTS_RCS)/Makefile.dms,v
	co $(COFLAGS) $<

# Set CACHE to either 8 or A to make the program run cached or uncached
# respectively. The "strip" macro after it, ensures all spaces are whacked
# off the definition before use

CACHE=8
RR_SIZE=2

SCACHE=$(strip $(CACHE))

os.lnk : makelnk Makefile 
	@echo "\tMaking $@..."
	PGM_BASE=0x${SCACHE}0000000; export PGM_BASE;\
	 DIO_END=0x8007FFFF; export DIO_END;\
	 PGM_LEN=0x007F0000; export PGM_LEN;\
	 RR_SIZE=${RR_SIZE}; export RR_SIZE;\
	 ROOT=${OS_ROOT_FILE}.o; export ROOT;\
	 INIT_SP=0x${SCACHE}07FFFFC; export INIT_SP;\
	sh $< > $@

os.elf : os.lnk $(OBJ_FILES) epromst.zo #vmunix.zo 

epromst.zo : $(CBOOT_SRC)
	$(patsubst EXTRAS,,$(ZO_RULE))

vmunix.zo : $(VMUNIX_SRC)
	$(patsubst EXTRAS,,$(ZO_RULE))
	
ALLD0	=	$(patsubst %.o,%.c,$(OBJ_FILES))
ALLD1	=	$(patsubst phx_root.c,,$(ALLD0))
ALLD2	=	$(patsubst sst_an.c,,$(ALLD1))
ALLD3	=	$(patsubst idtc4000.c,,$(ALLD2))
ALLDEPENDS=	$(sort $(ALLD3))

depends : $(ALLDEPENDS)
	$(DEPENDS_RULE)

#
#	CONFIG dependencies
#
config		: config.h 

config.h	: $(CONFIG_FILES)

#
#	ASM dependancies
#

idtc4000$O	: idtc4000.mac $(CONFIG_FILES)
phx_root$O	: phx_root.mac $(CONFIG_FILES)

# >>->
#	Cleanup
#
clean	:
	$(DEL) *.upd
	$(DEL) *.o *.ol *.olt *.ot
	$(DEL) os.elf os_vec.asm libos.a
	$(DEL) *.s *.pasm *.dio
	$(DEL) *.lis *.od *.L *.err *.map *.E
	$(DEL) *.i os.lnk os.mix
	$(DEL) *.i *.h
	$(DEL) dispatch.c

testem:
	@$(ECHO) "RR_SIZE   = \"$(strip $(RR_SIZE))\""
	@$(ECHO) "ISMACE    = \"$(strip $(ISMACE))\""
	@$(ECHO) "TOOLS     = \"$(strip $(TOOLS))\""
	@$(ECHO) "TOOLCYG   = \"$(strip $(TOOL_CYGR4K))\""
	@$(ECHO) "Processor = $(PROCESSOR)"
	@$(ECHO) "CFLAGS    = $(CFLAGS)"
	@$(ECHO) "LIBDIRS   = $(LIBDIRS)"
	@$(ECHO) "LIBS      = $(LIBS)"
	@$(ECHO) "OUTPUT_LANG = $(OUTPUT_LANG)"
	@$(ECHO) "PPINCS	= \"$(PPINCS)\""
	@$(ECHO) "CFILES   = \"$(CFILES)\""
	@$(ECHO) "ASMF1    = \"$(ASMF1)\""
	@$(ECHO) "CFLAGS   = $(CFLAGS)"
	@$(ECHO) "CC       = $(CC)"
	@$(ECHO) "DBG	   = $(DBG)"
	@$(ECHO) "OPT	   = $(OPT)"
	@$(ECHO) "CPPFLAGS = $(CPPFLAGS)"
	@$(ECHO) "STD_INCLUDE = $(STD_INCLUDE)"
	@$(ECHO) "STD_STDDEF = $(STD_STDDEF)"

#
#	C dependancies (note this list was created using the output of the
#	"depends" rule above but edited and massaged a bit to remove the
#	references to absolute paths).
#

decompboot.o: decompboot.c config.h os_proto.h zlib.h zconf.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)errno.h wms_proto.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h
mallocr.o: mallocr.c config.h os_proto.h ../os/st_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)errno.h

