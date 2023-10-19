#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libelf.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>

#define NAMES_SHSTRTAB	".shstrtab"
#define NAMES_TEXT	".text"
#define NAMES_SYMTAB	".symtab"
#define SUFFIX_COMP	"_comp_size"		/* size of data compressed */
#define SUFFIX_DECOMP	"_decomp_size"		/* size of data uncompressed */

static const char *help_msg[] = {
    "Usage: elf2zo [-nsym_name] [-i] [-z] input output\n",
    "Where:\n",
    "   -n syn_name = ASCII string to become label of data\n",
    "        (name defaults to same as output filename if this\n",
    "        parameter is not specified)\n",
    "   -i = input file is raw binary (image) instead of .elf\n",
    "   -z = DON\'T compress the data\n",
    "    input = input filename\n",
    "    output = output filename\n",
    0
};

static int say_help(void) {
    int ii;
    for (ii=0; help_msg[ii]; ++ii) {
	printf(help_msg[ii]);
    }
    return 1;
}

int main(int argc, char *argv[]) {
    Elf *elf, *arf;
    Elf32_Ehdr *ehdr;
    Elf_Scn *scn;
    Elf32_Shdr *shdr;
    Elf32_Phdr *phdr;
    Elf32_Sym *sym;
    Elf_Data *data;
    int filedes, image=0, no_compress=0;
    char *u_sym_name = 0, *strings;
    int str_size;
    int sym_name_off, sym_comp_off, sym_decomp_off;
    char *s;
    unsigned char *prog=0;
    int prog_len=0;
    int len, cmd, sts, verbose=0;
    uLong comprLen=0;
    unsigned char *compr=0;

    elf_version(EV_CURRENT);		/* required by the elf library functions */
    --argc;
    ++argv;
    for (;argc > 0;--argc, ++argv) {	/* process command args */
	s = *argv;
	if (*s++ != '-') break;
	switch (*s++) {
	    case 'v':
		verbose = 1;
		continue;
	    case 'n':
		if (*s == 0) {
		    return say_help();
		}
		u_sym_name = s;
		continue;
	    case 'i':
		image = 1;
		continue;
	    case 'z':
		no_compress = 1;
		continue;
	    default:
		return say_help();
	}
    }
    if (argc < 2) {
	return say_help();
    }

    filedes = open(*argv, O_RDONLY, 0);	/* open input file */
    if (filedes < 0) {
	perror("Unable to open input");
	return 2;
    }

    if (!image) {
	if ((arf = elf_begin(filedes, ELF_C_READ, (Elf *)0)) == 0) { /* prepare to decode */
	    perror("elf_begin on input file failed");
	    return 4;
	}

	cmd = ELF_C_READ;
	while ((elf = elf_begin(filedes, cmd, arf)) != 0) {
	    ehdr = elf32_getehdr(elf);
	    if (ehdr != 0 && (phdr = elf32_getphdr(elf)) != 0) {	/* get program header */
		int lcldes, ii;
		for (ii=0; ii < ehdr->e_phnum; ++ii, ++phdr) {
		    if (phdr->p_filesz) break;
		}
		if (ii < ehdr->e_phnum && phdr->p_filesz) {
#if 0
		    printf("Phdr: type=%ld, off=%ld, vaddr=%08lX, paddr=%08lX\n",
			phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr);
		    printf("   filsiz=%ld, memsiz=%ld, flags=%ld, align=%ld\n",
			phdr->p_filesz, phdr->p_memsz, phdr->p_flags, phdr->p_align);
#endif
		    prog_len = phdr->p_filesz;			/* data length is file length */
		    prog = malloc(prog_len);			/* place to put uncompressed data */
		    lcldes = dup(filedes);			/* this is not to confuse the elf lib */
		    if (lseek(lcldes, phdr->p_offset, SEEK_SET) < 0) { /* position to data in file */
			perror("Error seeking input");
			return 5;
		    }
		    if (read(lcldes, prog, prog_len) != prog_len) {	/* get uncompressed data */
			perror("Premature EOF on input");
			return 6;
		    }
		    close(lcldes);				/* done with this */
		}
	    } else {
		printf("No PHDR\n");
	    }
	    cmd = elf_next(elf);
	    elf_end(elf);
	    break;
	}
	elf_end(arf);
    } else {
	struct stat st;
	sts = fstat(filedes, &st);
	if (sts < 0) {
	    perror("Error fstat'ing input file");
	    return 2;
	}
	prog_len = st.st_size;
	if (prog_len) {
	    prog = malloc(prog_len);
	    if (!prog) {
		fprintf(stderr, "Unable to malloc %d bytes\n", prog_len);
		return 3;
	    }
	    sts = read(filedes, prog, prog_len);
	    if (sts != prog_len) {
		perror("Read error on input");
		return 4;
	    }
	}
    }
    close(filedes);				/* done with this */

    if (!prog_len) {				/* if no program data */
	printf("Input file empty\n");
	return 7;
    }
    if (!no_compress) {
	comprLen = (prog_len*11)/10;		/* make buffer for compressed data 110% */
	compr = malloc(comprLen);
	sts = compress(compr, &comprLen, prog, prog_len); /* compress input data */
	if (sts != Z_OK) {
	    printf("Error compressing. Return code %d\n", sts);
	    return 7;
	}
	if (verbose) printf("Compressed %s from %d to %ld. Compression ratio %4.2f:1\n", 
	*argv, prog_len, comprLen, (float)prog_len/(float)comprLen);
    }
    ++argv;					/* advance to output filename */
    if (!u_sym_name) {				/* if no symbol name provided */
	char *beg, *end;
	s = *argv;				/* point to output filename */
	beg = strrchr(s, '/');			/* remove leading '/'s */
	if (!beg) {
	    beg = s;
	} else {
	    ++beg;
	}
	end = strchr(beg, '.');			/* remove trailing '.'s */
	if (!end) end = beg + strlen(beg);
	u_sym_name = malloc(end - beg + 1);	/* get new space for string */
	strncpy(u_sym_name, beg, end-beg);	/* copy stripped string */
	u_sym_name[end-beg] = 0;		/* null terminated */
    }

/* Get buffer to hold all strings */
    len = strlen(u_sym_name);
    str_size = 1+sizeof(NAMES_SHSTRTAB)+sizeof(NAMES_TEXT)+sizeof(NAMES_SYMTAB)+
    			    3*(len+1)+sizeof(SUFFIX_COMP)+sizeof(SUFFIX_DECOMP);
    strings = malloc(str_size);
    strings[0] = 0;
/* Copy in all string names */
    memcpy(strings+1, NAMES_SHSTRTAB, sizeof(NAMES_SHSTRTAB));
    memcpy(strings+1+sizeof(NAMES_SHSTRTAB), NAMES_TEXT, sizeof(NAMES_TEXT));
    memcpy(strings+1+sizeof(NAMES_SHSTRTAB)+sizeof(NAMES_TEXT), NAMES_SYMTAB, sizeof(NAMES_SYMTAB));
    sym_name_off = 1 + sizeof(NAMES_SHSTRTAB) + sizeof(NAMES_TEXT) + sizeof(NAMES_SYMTAB);
/* construct the _comp_size and _decomp_size strings */
    s = strings + sym_name_off;
    memcpy(s, u_sym_name, len+1);
    sym_comp_off = sym_name_off + len+1;
    s = strings + sym_comp_off;
    strcpy(s, u_sym_name);
    strcat(s, SUFFIX_COMP);
    sym_decomp_off = sym_comp_off + len + sizeof(SUFFIX_COMP);
    s = strings + sym_decomp_off;
    strcpy(s, u_sym_name);
    strcat(s, SUFFIX_DECOMP);

    filedes = open(*argv, O_RDWR|O_TRUNC|O_CREAT, 0664);	/* open the output */
    if (filedes < 0) {
	perror("Unable to open output");
	return 3;
    }
    if ((elf = elf_begin(filedes, ELF_C_WRITE, (Elf *)0)) == 0) { /* prepare to write ELF file */
	perror("elf_begin failed");
	return 4;
    }
    ehdr = elf32_newehdr(elf);		/* construct the elf header */
    ehdr->e_type = ET_REL;
    ehdr->e_machine = EM_MIPS;
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_shstrndx = 1;
    ehdr->e_flags |= 0x20000000;

/* Construct the sections. */

    scn = elf_newscn(elf);		/* section 1 is the string table */
    shdr = elf32_getshdr(scn);
    shdr->sh_name = 1;			/* first name in the string table */
    shdr->sh_type = SHT_STRTAB;
    data = elf_newdata(scn);
    data->d_buf = strings;
    data->d_size = str_size;
    data->d_type = ELF_T_BYTE;

    scn = elf_newscn(elf);		/* section 2 is the .text section */
    shdr = elf32_getshdr(scn);
    shdr->sh_name = 1+sizeof(NAMES_SHSTRTAB); /* second name in the string table */
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC;
    data = elf_newdata(scn);
    if (!no_compress) {
	data->d_buf = compr;
	data->d_size = comprLen;
    } else {
	data->d_buf = prog;
	data->d_size = prog_len;
    }
    data->d_type = ELF_T_BYTE;

    scn = elf_newscn(elf);		/* section 3 is the .symtab section */
    shdr = elf32_getshdr(scn);
    shdr->sh_name = 1+sizeof(NAMES_SHSTRTAB)+sizeof(NAMES_TEXT); /* third name in string table */
    shdr->sh_type = SHT_SYMTAB;
    shdr->sh_flags = 0;
    shdr->sh_link = 1;			/* string table section index */
    shdr->sh_info = 2;			/* last local symbol index + 1 */
    data = elf_newdata(scn);
    data->d_buf = calloc(5, sizeof(Elf32_Sym));
    data->d_size = 5*sizeof(Elf32_Sym);
    data->d_type = ELF_T_SYM;
    sym = (Elf32_Sym *)data->d_buf;

/* Construct the symbol table */
    ++sym;				/* skip the 0th entry */
    sym->st_name = 0;			/* First entry is dummy local pointing to .text */
    sym->st_value = 0;			/* offset into section */
    sym->st_size = 0;			/* no size */
    sym->st_info = ELF32_ST_INFO(STB_LOCAL, STT_SECTION);
    sym->st_shndx = 2;			/* relative to section 2 */

    ++sym;				/* Second entry is plain symbol */
    sym->st_name = sym_name_off;	/* offset to name string */
    sym->st_value = 0;			/* offset into section */
    sym->st_size = 0;			/* no size */
    sym->st_info = ELF32_ST_INFO(STB_GLOBAL, STT_SECTION);
    sym->st_shndx = 2;			/* relative to section 2 (.text) */

    ++sym;				/* Third entry is to 'symb'_comp_size; */
    sym->st_name = sym_comp_off;	/* offset to name string */
    sym->st_value = comprLen;		/* absolute value */
    sym->st_size = 0;			/* no size */
    sym->st_info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    sym->st_shndx = SHN_ABS;		/* ABS */

    ++sym;				/* Fourth entry is to 'symb'_decomp_size */
    sym->st_name = sym_decomp_off;	/* offset to name string */
    sym->st_value = prog_len;		/* absolute value */
    sym->st_size = 0;			/* no size */
    sym->st_info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    sym->st_shndx = SHN_ABS;		/* ABS */

    elf_update(elf, ELF_C_WRITE);
    elf_end(elf);
    close(filedes);
    free(prog);
    return 0;
}
