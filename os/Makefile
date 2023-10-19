#	This makefile will only work with GNU make.

# Point to the various source directories

GUTS_RCS  = RCS

GLIDE_LIBRARY_PATH = /usr/local/GUTS/3dfx/glide-2.1.1noisy
#GLIDE_LIBRARY_PATH = /usr/local/GUTS/3dfx/glide-2.0b3noisy2.7
GLIDE_INCLUDE_PATH = ${GLIDE_LIBRARY_PATH}/include

STD_INCLUDE = ${R4KL_INCLUDE_PATH}/
STD_STDDEF  = ${R4KL_INCLUDE_PATH_HW}/

# Point to files used by the default rules

DEF_PP	  = def_pp.mac
DATE	  = sst_date.c

# Tell make where to look for sources

RCSPATH = $(GUTS_RCS)		# point this to the RCS tree(s)
SPATH = .					# point this to the source tree(s)
HPATH = .:$(STD_INCLUDE):$(GLIDE_INCLUDE_PATH)/
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

# Misc utilitiy flags

COFLAGS 	= -q

# Assembler and MACPP flags

PPFLAGS		= $(PPINCS) #-lis	# set to -lis if you also want listings with MACPP'd sources
ASM_PPFLAGS	= $(PPINCS) -line #-lis	# ditto, but leave the -line if you want asxx to report the correct
ASFLAGS		= -mips3 -32
POUT		= -out=

# Compiler flags

DEFS		 = -DBUILTIN_MEMCPY -DDEBUG -DMAKE_REFERENCES #-DTDFX_NTMUS=2 
DEFS		+= #-DNO_ENFORCE_NUMTMUS
DBG		 = -g3
OPT		 = -O2 $(DBG)
TDFX_CFLAGS	 = $(OPT) $(CINCS) $(DEFS) -Wall -G 0 
TDFX_CFLAGS	+= -fshort-double -mips3 -mgp32 -mfp64 -Wa,-32
CFLAGS		 = $(TDFX_CFLAGS) -ansi -pedantic 

# Linker flags

LDFLAGS		= 
LIBDIRS		= -L${R4KL_LIBRARY_PATH} -L${GLIDE_LIBRARY_PATH}
LIBS		= -lcled -lglide -lnsprintff -lmf -lcf -lgcc

# All rules

include Makefile.dms

define TDFX_RULE
	@$(ECHO) "\tCompiling (non-ansi) $<..."
	@$(CC) $(TDFX_CFLAGS) -c $< > $(basename $(@F)).err 2>&1;\
	if [ ! -s $(basename $(@F)).err ];\
	then\
	  rm $(basename $(@F)).err;\
	else\
	  $(ECHO) 'Errors in "$<"';\
	fi
endef

#	@ before line does not echo command line
#	.SILENT: makes 'make' not echo command lines through out file
.SILENT:

O		= .o

OS_FILES = phx_time$O dispatch$O selftest$O eedrive$O dummy_pbase$O 
UTL_FILES = cdec_hex$O random$O mallocr$O memcpyrxk$O
AUD_FILES = phx_audio$O phx_audio_selftest$O phx_audio_queue_interface$O phx_audio_stream$O phx_audpgm$O 
CTL_FILES = qio_uart$O pots$O phx_rtc$O
DRIVE_FILES = nsc_415$O nsc_pcishim$O qio.o fsys.o qio_errs.o phx_ide.o phx_hdshims.o 
PHX_FILES = phx_stubs$O 
GAME_FILES = gamedefs$O 
SST_FILES = sst_vid$O sst_text$O sst_an$O
DIAG_FILES = ram_test$O mem_test$O scope$O entry$O menus$O swt_test$O wb_test$O #snd_test$O 
NET_FILES = net_test$O smc_putstr$O smc_main$O net_link$O smc_rx$O smc_tx$O smc_intv$O smc_dma$O
TDFX_FILES = tdfx_main.o tdfx_util.o tdfx_tests.o tdfx_pass.o tdfx_cledi.o tdfx_shims.o
TDFX_FILES += tdfx_cmds.o tdfx_tmutest.o tdfx_fbitest.o
UX_CFILES = ux_kbd$0
MINI_FILES = mini_test$O cit_text$O null_vid$O cdec_hex$O 
ICELESS_CFILES = cit_text$O 
SA_STUBS = matherr$O 
WMS_FILES = wms_gt64010$O wms_pciio$O picfuncs$O #wms_ioasic$O 

PGUTS_FILES  = $(OS_FILES) $(UTL_FILES) $(AUD_FILES) $(CTL_FILES) $(GAME_FILES)
PGUTS_FILES += $(SST_FILES) $(PHX_FILES) $(DIAG_FILES) $(ICELESS_CFILES)
PGUTS_FILES += $(DRIVE_FILES) $(WMS_FILES) $(NET_FILES) $(TDFX_FILES)

CONFIG_FILES =  config.mac constants.mac pptypes.mac 
CONFIG_FILES += seattle.mac galileo.mac generic_switches.mac sst.mac 
CONFIG_FILES += idt.mac idt_vectors.mac ioasic.mac nss_wb.mac vmdvr.mac

OS_ROOT_FILE = phx_root

OS_ROOT = $(OS_ROOT_FILE)$O phx_shims$O idtc4000$O #fexcp$O 

OBJ_FILES = $(OS_ROOT) $(PGUTS_FILES)

default : libos.a os.elf #os_vec.asm
	@$(ECHO) "	Done..."

install : hdboot.dio
	@echo "FTP'ing to volta..."
	@echo "cd /Projects/SubSys/phxad/hockey" > /tmp/$$$$; \
	 echo "bin" >> /tmp/$$$$; \
	 echo "hash" >> /tmp/$$$$; \
	 echo "put hdboot.dio" >> /tmp/$$$$ ; \
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

os.elf : os.lnk phx_root.o libos.a #boot_hd.a 

hdboot.dio : os.elf os.lnk
	echo "\tMixiting to $@..."
	mixit -q os.mix
	dio_cksum os.dio hdboot.dio 0x7fffc 0x01020304
	dd if=hdboot.dio of=maceboot.bin bs=16 skip=1 count=32768  

libos.a : Makefile $(OBJ_FILES) $(DATE)
	@rm -f os.err $(basename $@).err $$$$.err;\
	touch $$$$.err;\
	ls *.err | grep -v $$$$.err > /dev/null || true;\
	rm -f $$$$.err;\
	if [ ! -s *.err ];\
	then\
	  echo "\tMaking $@...";\
	  $(CC) $(CFLAGS) -c -o sst_date.o $(DATE);\
	  rm -f $@;\
	  ar${PROCESSOR} -rus $@ $(filter-out Makefile, $(filter-out $(DATE),$(filter-out phx_root.o,$^))) sst_date.o;\
	else\
	  echo "$@ not made due to .err files";\
	  ls *.err;\
	  exit 1;\
	fi

ALLD0	=	$(patsubst %.o,%.c,$(OBJ_FILES))
ALLD1	=	$(patsubst phx_root.c,,$(ALLD0))
ALLD2	=	$(patsubst sst_an.c,,$(ALLD1))
ALLD3	=	$(patsubst idtc4000.c,,$(ALLD2))
ALLD4	=	$(patsubst memcpyrxk.c,,$(ALLD3))
ALLD5	=	$(patsubst phx_audpgm.c,,$(ALLD4))
ALLDEPENDS=	$(sort $(ALLD5))

depends : $(ALLDEPENDS)
	$(DEPENDS_RULE)

#
#	CONFIG dependencies
#
config		: config.h 

config.h	: $(CONFIG_FILES)

#
# Special case code generations
#

intvecs.h : phx_vecs.mac config.mac $(DEF_PP)
	@rm -f $(basename $<).lis $(basename $<).pasm dispatch.c $@
	@$(ECHO) "\tMACPP'ing $< to dispatch.c and $@ ..."
	$(PP) $(PPINCS) -lis=$(basename $<).lis -out=dispatch.c -out=$@ -out=/dev/null \
		-assem="OUTPUT_LANG == 5 " \
		$(DEF_PP) $< > $(basename $<).err 2>&1; \
	if [ -s $(basename $<).err ];\
	then\
	  $(ECHO) 'MACPP errors listed in $(basename $<).err';\
	else \
	  $(RM) -f $(basename $<).err; \
	  chmod 444 dispatch.c $@; \
	fi
		
dispatch.c	: phx_vecs.mac config.mac $(DEF_PP)
	@rm -f $(basename $<).lis $(basename $<).pasm $@ intvecs.h
	@$(ECHO) "\tMACPP'ing $< to $@ and intvecs.h ..."
	$(PP) $(PPINCS) -lis=$(basename $<).lis -out=$@ -out=intvecs.h -out=/dev/null \
		-assem="OUTPUT_LANG == 5 " \
		$(DEF_PP) $< > $(basename $<).err 2>&1; \
	if [ -s $(basename $<).err ];\
	then\
	  $(ECHO) 'MACPP errors listed in $(basename $<).err';\
	else \
	  $(RM) -f $(basename $<).err; \
	  chmod 444 $@ intvecs.h; \
	fi
		
#
#	EEPROM
#
stat_map	: stat_map.h stat_defs.h eer_defs.h eer_defs.i
	@$(ECHO) "\tdone with stat_map"

stat_map.h	: stat_map.mac stat_pp.mac 
	@$(ECHO) "\tMaking $@"
	@rm -f $(basename $(@F)).err
	@$(PP) $(POUT)$@ $(PPFLAGS) \
			-assem="MAKE_STAT_MAP == 1" \
			$(filter %stat_pp.mac,$^) \
			$<  > $(basename $(@F)).err 2>&1; \
		if [ -s $(basename $(@F)).err ];\
		then \
			$(ECHO) "Errors in $(basename $(@F)).err";\
		else\
			rm -f $(basename $(@F)).err; \
		fi

stat_defs.h	: stat_map.mac stat_pp.mac 
	@$(ECHO) "\tMaking $@"
	@rm -f $(basename $(@F)).err
	@$(PP) $(POUT)$@ $(PPFLAGS) \
			-assem="MAKE_STAT_DEFS == 1" \
			$(filter %stat_pp.mac,$^) \
			$< > $(basename $(@F)).err 2>&1; \
		if [ -s $(basename $(@F)).err ];\
		then \
			$(ECHO) "Errors in $(basename $(@F)).err";\
		else\
			rm -f $(basename $(@F)).err; \
		fi

eer_defs.h : stat_map.mac stat_pp.mac 
	@$(ECHO) "\tMaking $@"
	@rm -f $(basename $(@F)).err
	@$(PP) $(POUT)$@ $(PPFLAGS) \
			-assem="MAKE_EER_DEFS == 1" \
			$(filter %stat_pp.mac,$^) \
			$< > $(basename $(@F)).err 2>&1; \
		if [ -s $(basename $(@F)).err ];\
		then \
			$(ECHO) "Errors in $(basename $(@F)).err";\
		else\
			rm -f $(basename $(@F)).err; \
		fi

eer_defs.i : stat_map.mac stat_pp.mac 
	@$(ECHO) "\tMaking $(notdir $@)"
	@rm -f $(basename $(@F)).err
	@$(PP) $(POUT)$@ $(PPFLAGS) \
			-assem="MAKE_EER_DEFS == 1" \
			-assem="OUTPUT_LANG==$(OUTPUT_LANG)" \
			$(filter %stat_pp.mac,$^) \
			$< > $(basename $(@F)).err 2>&1; \
		if [ -s $(basename $(@F)).err ];\
		then \
			$(ECHO) "Errors in $(basename $(@F)).err";\
		else\
			rm -f $(basename $(@F)).err; \
		fi
# >>->
#	Cleanup
#
clean	:
	$(DEL) *.upd depends
	$(DEL) *.o *.ol *.olt *.ot
	$(DEL) os.elf os_vec.asm os.mix libos.a
	$(DEL) *.s *.pasm
	$(DEL) *.lis *.od *.L *.err *.map *.E
	$(DEL) *.i os.lnk
	$(DEL) *.i
	$(DEL) config.h stat_defs.h stat_map.h intvecs.h eer_defs.h
	$(DEL) dispatch.c

testem:
	@$(ECHO) "RR_SIZE   = \"$(strip $(RR_SIZE))\""
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
#	ASM dependencies
#

phx_root$O	: phx_root.mac $(CONFIG_FILES)
idtc4000$O	: idtc4000.mac $(CONFIG_FILES)
sst_an$O	: sst_an.mac $(CONFIG_FILES)
memcpyrxk$O	: memcpyrxk.mac $(CONFIG_FILES)
phx_audpgm.o	: phx_audpgm.mac

#
#	C dependancies (note this list was created using the output of the
#	"depends" rule above but edited and massaged a bit to remove the
#	references to absolute paths).
#

cdec_hex.o: cdec_hex.c config.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 os_proto.h
cit_text.o: cit_text.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h
dispatch.o: dispatch.c config.h os_proto.h intvecs.h
dummy_pbase.o: dummy_pbase.c
eedrive.o: eedrive.c config.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 os_proto.h stat_defs.h stat_map.h
entry.o: entry.c config.h os_proto.h st_proto.h
fsys.o: fsys.c \
 $(STD_INCLUDE)sys/types.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)machine/types.h \
 config.h os_proto.h st_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)limits.h \
 $(STD_INCLUDE)machine/limits.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h qio.h fsys.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h intvecs.h \
 eer_defs.h iio_proto.h icelesspkt.h
gamedefs.o: gamedefs.c config.h eer_defs.h os_proto.h st_proto.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 wms_proto.h $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 nsc_idereg.h \
 $(STD_INCLUDE)string.h pic_defs.h
mallocr.o: mallocr.c config.h os_proto.h st_proto.h qio.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)errno.h
mem_test.o: mem_test.c config.h os_proto.h
menus.o: menus.c config.h eer_defs.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 os_proto.h st_proto.h
net_link.o: net_link.c net_link.h config.h net_data.h net_smc.h \
 smc_link.h smc_data.h smc_dma.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 os_proto.h
net_test.o: net_test.c config.h os_proto.h st_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 wms_proto.h nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)string.h net_smc.h \
 net_link.h smc_link.h smc_data.h net_data.h smc_test.h smc_regs.h
nsc_415.o: nsc_415.c config.h os_proto.h st_proto.h nsc_defines.h \
 nsc_pcicfig.h nsc_415.h nsc_drvdisp.h nsc_gt64010.h
nsc_pcishim.o: nsc_pcishim.c config.h wms_proto.h nsc_defines.h \
 nsc_pcicfig.h nsc_gt64010.h
phx_audio.o: phx_audio.c config.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 os_proto.h eer_defs.h phx_audio_proto.h phx_audio_internal.h
phx_audio_queue_interface.o: phx_audio_queue_interface.c config.h \
 os_proto.h eer_defs.h intvecs.h phx_audio_internal.h \
 phx_audio_proto.h
phx_audio_selftest.o: phx_audio_selftest.c config.h os_proto.h \
 st_proto.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 eer_defs.h nsprintf.h phx_audio_internal.h phx_audio_proto.h
phx_audio_stream.o: phx_audio_stream.c config.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 os_proto.h intvecs.h phx_audio_proto.h phx_audio_internal.h
phx_hdshims.o: phx_hdshims.c config.h os_proto.h \
 $(STD_INCLUDE)ctype.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)string.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h qio.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)stdlib.h nsprintf.h \
 fsys.h
phx_ide.o: phx_ide.c config.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h os_proto.h \
 st_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 wms_proto.h intvecs.h qio.h \
 $(STD_INCLUDE)stdlib.h fsys.h \
 nsprintf.h eer_defs.h nsc_defines.h nsc_pcicfig.h nsc_415.h \
 nsc_drvdisp.h nsc_gt64010.h nsc_idereg.h
phx_rtc.o: phx_rtc.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 wms_proto.h pic_defs.h
phx_shims.o: phx_shims.c config.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 icelesspkt.h intvecs.h wms_proto.h st_proto.h ioa_uart.h eer_defs.h \
 os_proto.h
phx_stubs.o: phx_stubs.c config.h os_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)math.h wms_proto.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)errno.h qio.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h fsys.h
phx_time.o: phx_time.c config.h os_proto.h st_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h
picfuncs.o: picfuncs.c config.h os_proto.h wms_proto.h pic_defs.h \
 nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h
pots.o: pots.c config.h os_proto.h st_proto.h eer_defs.h pot_desc.h
qio.o: qio.c config.h os_proto.h phx_proto.h \
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
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)errno.h qio.h
qio_errs.o: qio_errs.c config.h os_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 qio.h $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 nsprintf.h
qio_uart.o: qio_uart.c config.h os_proto.h st_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 intvecs.h ioa_uart.h uart.h qio.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)string.h
ram_test.o: ram_test.c config.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 os_proto.h st_proto.h ram_desc.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h
random.o: random.c
scope.o: scope.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h
selftest.o: selftest.c config.h os_proto.h st_proto.h eer_defs.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h
smc_dma.o: smc_dma.c smc_regs.h smc_data.h config.h net_data.h \
 net_link.h net_smc.h smc_dma.h smc_link.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 os_proto.h intvecs.h
smc_intv.o: smc_intv.c config.h os_proto.h smc_regs.h smc_data.h \
 net_data.h net_link.h net_smc.h smc_link.h smc_dma.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h
smc_main.o: smc_main.c config.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 wms_proto.h smc_regs.h net_smc.h smc_data.h net_data.h net_link.h \
 smc_link.h os_proto.h eer_defs.h intvecs.h \
 $(STD_INCLUDE)string.h
smc_putstr.o: smc_putstr.c config.h os_proto.h st_proto.h
smc_rx.o: smc_rx.c smc_regs.h smc_data.h config.h net_data.h \
 net_link.h net_smc.h smc_link.h smc_dma.h os_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h
smc_tx.o: smc_tx.c smc_regs.h smc_data.h config.h net_data.h \
 net_link.h net_smc.h smc_link.h smc_dma.h os_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h
sst_text.o: sst_text.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h wms_proto.h \
 txt_tab.h
sst_vid.o: sst_vid.c config.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)limits.h \
 $(STD_INCLUDE)machine/limits.h \
 os_proto.h st_proto.h intvecs.h eer_defs.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 wms_proto.h $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(STD_INCLUDE)math.h sst_tdfx.h \
 nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h
swt_test.o: swt_test.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 eer_defs.h
tdfx_cledi.o: tdfx_cledi.c config.h cled_io.h cled_ioctl.h \
 cled_defines.h cled_termios.h cled_proto.h iio_proto.h icelesspkt.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h uart.h
	$(TDFX_RULE)
tdfx_cmds.o: tdfx_cmds.c \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 config.h os_proto.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)string.h qio.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h fsys.h \
 tdfx_diags.h \
 $(STD_INCLUDE)setjmp.h \
 $(STD_INCLUDE)machine/setjmp.h
	$(TDFX_RULE)
tdfx_fbitest.o: tdfx_fbitest.c config.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)math.h \
 $(STD_INCLUDE)string.h \
 $(GLIDE_INCLUDE_PATH)/sst.h \
 $(GLIDE_INCLUDE_PATH)/ncc.h \
 $(GLIDE_INCLUDE_PATH)/fx64.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h tdfx_diags.h
	$(TDFX_RULE)
tdfx_main.o: tdfx_main.c config.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)time.h \
 $(GLIDE_INCLUDE_PATH)/sst1test.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)string.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst.h \
 $(GLIDE_INCLUDE_PATH)/ncc.h \
 $(GLIDE_INCLUDE_PATH)/fx64.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/fxos.h
	$(TDFX_RULE)
tdfx_pass.o: tdfx_pass.c \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdlib.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h
	$(TDFX_RULE)
tdfx_shims.o: tdfx_shims.c config.h os_proto.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)string.h uart.h \
 nsprintf.h qio.h \
 $(STD_INCLUDE)sys/types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_INCLUDE)sys/stat.h \
 $(STD_INCLUDE)time.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h fsys.h \
 $(STD_INCLUDE)fcntl.h \
 $(STD_INCLUDE)sys/fcntl.h \
 $(STD_INCLUDE)errno.h phx_proto.h \
 wms_proto.h intvecs.h
	$(TDFX_RULE)
tdfx_tests.o: tdfx_tests.c config.h os_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 nsprintf.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(GLIDE_INCLUDE_PATH)/sst1test.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)string.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst.h \
 $(GLIDE_INCLUDE_PATH)/ncc.h \
 $(GLIDE_INCLUDE_PATH)/fx64.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/fxos.h sst_tdfx.h
	$(TDFX_RULE)
tdfx_tmutest.o: tdfx_tmutest.c config.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)string.h nsprintf.h \
 phx_proto.h $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(GLIDE_INCLUDE_PATH)/sst.h \
 $(GLIDE_INCLUDE_PATH)/ncc.h \
 $(GLIDE_INCLUDE_PATH)/fx64.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h \
 $(GLIDE_INCLUDE_PATH)/sst1test.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/fxos.h sst_tdfx.h \
 tdfx_diags.h
	$(TDFX_RULE)
tdfx_util.o: tdfx_util.c config.h \
 $(GLIDE_INCLUDE_PATH)/sst1test.h \
 $(STD_INCLUDE)stdlib.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)stdio.h \
 $(STD_STDDEF)stdarg.h \
 $(STD_STDDEF)va-mips.h \
 $(STD_INCLUDE)string.h \
 $(GLIDE_INCLUDE_PATH)/3dfx.h \
 $(GLIDE_INCLUDE_PATH)/sst.h \
 $(GLIDE_INCLUDE_PATH)/ncc.h \
 $(GLIDE_INCLUDE_PATH)/fx64.h \
 $(GLIDE_INCLUDE_PATH)/fxpci.h \
 $(GLIDE_INCLUDE_PATH)/glide.h \
 $(GLIDE_INCLUDE_PATH)/sst1vid.h \
 $(GLIDE_INCLUDE_PATH)/glidesys.h \
 $(GLIDE_INCLUDE_PATH)/glideutl.h \
 $(GLIDE_INCLUDE_PATH)/sst1init.h \
 $(GLIDE_INCLUDE_PATH)/fxos.h nsprintf.h
	$(TDFX_RULE)
wb_test.o: wb_test.c config.h os_proto.h st_proto.h \
 $(STD_INCLUDE)string.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_STDDEF)stddef.h
wms_gt64010.o: wms_gt64010.c config.h os_proto.h phx_proto.h \
 $(STD_INCLUDE)reent.h \
 $(STD_INCLUDE)sys/reent.h \
 $(STD_INCLUDE)_ansi.h \
 $(STD_INCLUDE)sys/config.h \
 $(STD_INCLUDE)sys/signal.h \
 $(STD_INCLUDE)sys/_types.h \
 $(STD_INCLUDE)machine/types.h \
 $(STD_STDDEF)stddef.h \
 nsc_gt64010.h
wms_pciio.o: wms_pciio.c config.h wms_proto.h

