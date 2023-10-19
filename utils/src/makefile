MODE =
#DEFINES = -DBIG_ENDIAN -Dsun
DEFINES = -DEXTERNAL_PACKED_STRUCTS -DALIGNMENT=0
ZLIB = /usr/local/GUTS/zlib/sgi
INCS =  -I${ZLIB}

#
# For non-SGI systems:
CFLAGS = $(INCS) $(DEFINES) $(MODE) -g -Wall -pedantic -ansi
CC = gcc 

# For SGI machines
#CFLAGS = $(DEFINES) $(MODE) -g # -Wall -pedantic 
#CC = cc 

%.o : %.c
	$(CC) $(CFLAGS) -c $<

% :
	@echo "Hmmmm...don't know how to make $@"
	exit 1

#.SILENT:

elf2zo: elf2zo.o
	$(CC) $(CFLAGS) -o $@ $< -L$(ZLIB) -lz -lelf
	@echo "Done"

elf2zo.o : elf2zo.c

clean:
	rm -f *.o
