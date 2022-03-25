#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "fat32disk.h"

unsigned int getFileDirectory(const char *diskName){  // Acessa o diretorio a partir do disco
    int fd = open(diskName, O_RDWR); // leitura e gravação - file descriptor
    if (fd<0){
        perror("Arquivo de disco não consegue abrir");
        exit(1);
    }
    return fd;
}

BootEntry* readFileSystem(int fd){  // mapeia o disco para a struct com base no arquivo

    BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ, MAP_PRIVATE, fd, 0);
    if (disk==NULL){
        perror("Erro no mmap");
        exit(1);
    }
    return disk;
}

void showDiskInformation(BootEntry* disk){ // mostra informação do disco
    printf("Número de FATs = %d\n"
            "Número de bytes por setor = %d\n"
            "Número de setores por cluster = %d\n"
            "Número de setores reservados = %d\n",
            disk->BPB_NumFATs, disk->BPB_BytsPerSec, disk->BPB_SecPerClus, disk->BPB_RsvdSecCnt
    );
    fflush(stdout);
}

void showUsage()
{ // mostra operaçoes pro usuario
    printf(
        "Para usar: ./file fat32 <opções>\n"
        "  -i                     Mostra informações do sistema de arquivos.\n"
        "  -l                     Lista o diretorio raiz.\n"
        "  -r arquivo [-s sha1]   Recupera o arquivo.\n"
    );
    fflush(stdout);
    exit(1);
}
