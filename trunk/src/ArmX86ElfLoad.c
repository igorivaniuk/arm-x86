#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ArmX86Debug.h"
#include "ArmX86ElfLoad.h"

#define EI_NIDENT 	    16
#define ET_EXEC             2
#define EM_ARM              40

typedef struct elfHeader_s{
  unsigned           char e_ident[EI_NIDENT];
  uint16_t           e_type;
  uint16_t           e_machine;
  uint32_t           e_version;
  uint32_t           e_entry;
  uint32_t           e_phoff;
  uint32_t           e_shoff;
  uint32_t           e_flags;
  uint16_t           e_ehsize;
  uint16_t           e_phentsize;
  uint16_t           e_phnum;
  uint16_t           e_shentsize;
  uint16_t           e_shnum;
  uint16_t           e_shstrndx;
}elfHeader_t;

typedef struct programHeader_s{
  uint32_t           p_type;
  uint32_t           p_offset;
  uint32_t           p_vaddr;
  uint32_t           p_paddr;
  uint32_t           p_filesz;
  uint32_t           p_memsz;
  uint32_t           p_flags;
  uint32_t           p_align;
}programHeader_t;

FILE *elf, *bin;

#define READ_ELF32_HALF(elem,elf) { int numBytes; 			\
                                    numBytes = fread((elem),1,2,elf);	\
                                    if(numBytes < 2){			\
                                      DP("End of File reached\n");	\
                                    }					\
                                  }					\

#define READ_ELF32_WORD(elem,elf) { int numBytes;                       \
                                    numBytes = fread((elem),1,4,elf);	\
                                    if(numBytes < 4){			\
                                      DP("End of File reached\n");	\
                                    }					\
                                  }					\

void parseElfHeader(FILE *elf, elfHeader_t *elfHeader){
  int numBytesRead;

  numBytesRead = fread(elfHeader->e_ident, 1, EI_NIDENT, elf);
  READ_ELF32_HALF(&elfHeader->e_type, elf)
  READ_ELF32_HALF(&elfHeader->e_machine, elf)
  READ_ELF32_WORD(&elfHeader->e_version, elf)
  READ_ELF32_WORD(&elfHeader->e_entry, elf)
  READ_ELF32_WORD(&elfHeader->e_phoff, elf)
  READ_ELF32_WORD(&elfHeader->e_shoff, elf)
  READ_ELF32_WORD(&elfHeader->e_flags, elf)
  READ_ELF32_HALF(&elfHeader->e_ehsize, elf)
  READ_ELF32_HALF(&elfHeader->e_phentsize, elf)
  READ_ELF32_HALF(&elfHeader->e_phnum, elf)
  READ_ELF32_HALF(&elfHeader->e_shentsize, elf)
  READ_ELF32_HALF(&elfHeader->e_shnum, elf)
  READ_ELF32_HALF(&elfHeader->e_shstrndx, elf)

  DP1("Elf Header Identification: %s\n",elfHeader->e_ident);
  DP1("ELF Type: %d\n",elfHeader->e_type);
  DP1("ELF Machine: %d\n",elfHeader->e_machine);
  DP1("ELF Version: %d\n",elfHeader->e_version);
  DP1("ELF Entry: %d\n",elfHeader->e_entry);
  DP1("ELF Program Header Offset: 0x%x\n",elfHeader->e_phoff);
  DP1("ELF Section Header Offset: 0x%x\n",elfHeader->e_shoff);
  DP1("ELF Flags: 0x%x\n",elfHeader->e_flags);
  DP1("ELF Header Size: %d\n",elfHeader->e_ehsize);
  DP1("ELF Program Header Ent Size: %d\n",elfHeader->e_phentsize);
  DP1("ELF Program Header Num: %d\n",elfHeader->e_phnum);
  DP1("ELF Section Header Ent Size: %d\n",elfHeader->e_shentsize);
  DP1("ELF Section Header Num: %d\n",elfHeader->e_shnum);
  DP1("ELF Section Header Str Index: %d\n",elfHeader->e_shstrndx);
}

void parseProgramHeader(FILE *elf, programHeader_t *programHeader){

  READ_ELF32_WORD(&programHeader->p_type, elf)
  READ_ELF32_WORD(&programHeader->p_offset, elf)
  READ_ELF32_WORD(&programHeader->p_vaddr, elf)
  READ_ELF32_WORD(&programHeader->p_paddr, elf)
  READ_ELF32_WORD(&programHeader->p_filesz, elf)
  READ_ELF32_WORD(&programHeader->p_memsz, elf)
  READ_ELF32_WORD(&programHeader->p_flags, elf)
  READ_ELF32_WORD(&programHeader->p_align, elf)

  DP1("Program Header Type: %d\n",programHeader->p_type);
  DP1("Program Segment Offset: %d\n",programHeader->p_offset);
  DP1("Program Virtual Address: 0x%x\n",programHeader->p_vaddr);
  DP1("Program Physical Address: 0x%x\n",programHeader->p_paddr);
  DP1("Program Segment File Image Size: %d\n",programHeader->p_filesz);
  DP1("Program Segment Memory Image Size: %d\n",programHeader->p_memsz);
  DP1("Program Header Flags: 0x%x\n",programHeader->p_flags);
  DP1("Program Header Alignment: 0x%x\n",programHeader->p_align);
}

uint32_t* armX86ElfLoad(char *elfFile){
  elfHeader_t elfHeader;
  uint32_t i, *instr;
  int elfFd;
  struct stat sBuf;

  DP_HI;

  if((elf = fopen(elfFile,"rb")) == NULL){
    DP1("Could not open %s\n", elfFile);
    return NULL;
  }

  char *outputFileName = (char *)malloc(strlen(elfFile) + 4);
  char *ptr;
  if((ptr = strrchr(elfFile, '.')) != NULL) *ptr = '\0';
  sprintf(outputFileName,"%s.img",elfFile);
  if((bin = fopen(outputFileName,"wb")) == NULL){
    DP("Could not open file for writing\n");
    return NULL;
  }

  parseElfHeader(elf,&elfHeader);

  if(elfHeader.e_type != ET_EXEC){
    DP("Invalid ELF: Non-executable image\n");
    return NULL;
  }

  if(elfHeader.e_machine != EM_ARM){
    DP("Invalid ELF: Not ARM executable\n");
    return NULL;;
  }

  if(elfHeader.e_phoff == 0){
    DP("Invalid ELF: Process image cannot be created\n");
    return NULL;
  }

  //
  // FIXME:
  // When loading a program, check that it will fit in memory
  // Determine correctly exactly when this loop will end
  //
  for(i=0; i<elfHeader.e_phnum; i++){
    programHeader_t *programHeader = 
     (programHeader_t *)malloc(sizeof(programHeader_t));
    parseProgramHeader(elf, programHeader);

    uint32_t inst;
    fseek(elf, programHeader->p_paddr, SEEK_SET);
    for(i=0; i<programHeader->p_filesz; i++){
      fread(&inst,1,4,elf);
      fwrite(&inst,1,4,bin);
    }
  }

  fclose(elf);
  fclose(bin);

  if((elfFd = open(outputFileName,O_RDONLY)) == -1){
    DP1("Unable to open image for loading: %x\n",errno);
    return NULL;
  }

  if((stat(outputFileName, &sBuf)) == -1){
    DP1("Unable to stat image file: %x\n",errno);
    return NULL;
  }

  instr = (uint32_t *)mmap(
    (caddr_t)0,
    sBuf.st_size,
    PROT_READ,
    MAP_SHARED,
    elfFd,
    0
  );

  if(instr == (uint32_t *)-1){
    DP1("Could not load image file to memory: %x\n",errno);
    return NULL;
  }

  DP1("Loaded image file at 0x%p\n",instr);

  if(close(elfFd) == -1){
    DP("Failed to close image file post-loading\n");
  }
  free(outputFileName);  

  DP_BYE;
  return instr;
}
