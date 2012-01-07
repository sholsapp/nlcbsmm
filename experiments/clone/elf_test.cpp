#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <elf.h>
#include <libelf.h>
#include <fcntl.h>
static void failure(void);

int main(int argc, char ** argv) {
    Elf32_Shdr *    shdr;
    Elf32_Ehdr *    ehdr;
    Elf *        elf;
    Elf_Scn *    scn;
    Elf_Data *    data;
    int        fd;
    unsigned int    cnt;

    printf("Starting Elf test\n");

    /* Open the input file */
    if ((fd = open(argv[1], O_RDONLY)) == -1){
        printf("Couldnt find: %s\n", argv[1]);
        exit(1);
    }

    /* Obtain the ELF descriptor */
    (void) elf_version(EV_CURRENT);
    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
        failure();

    /* Obtain the .shstrtab data buffer */
    if (((ehdr = elf32_getehdr(elf)) == NULL) || 
	((scn = elf_getscn(elf, ehdr->e_shstrndx)) == NULL) ||
        ((data = elf_getdata(scn, NULL)) == NULL))
        failure();

    ehdr = elf32_getehdr(elf);

    /* Traverse input filename, printing each section */
    for (cnt = 1, scn = NULL; scn = elf_nextscn(elf, scn); cnt++) {
         if ((shdr = elf32_getshdr(scn)) == NULL) 
		 failure();
	 (void) printf("[%d]    %s\n", cnt,(char *)data->d_buf + shdr->sh_name);
    }
    printf("Ending Elf test\n");
    return 0;
}        /* end main */

static void failure()
{
        (void) fprintf(stderr, "%s\n", elf_errmsg(elf_errno()));
        exit(1);
}

