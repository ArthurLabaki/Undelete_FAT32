#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "fat32disk.h"

bool isDirectory(DirEntry* dirEntry){ // True se for um diretorio
    return dirEntry->DIR_Attr == 0x10;
}

char* getDisk(unsigned int fd){ // retorna o disco mapeado dado o arquivo
    struct stat fs; // struct para arquivos
    if(fstat(fd, &fs) == -1) 
    {
        perror("Erro ao ler o stat");
    }
    return mmap(NULL , fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // Retorna o disco do arquivo mapeado
}

DirEntry* getclusterPtr(char* file_content, BootEntry* disk, unsigned int cluster){  // cria a struct para o arquivo e cluster no disco
    unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (cluster- 2)*disk->BPB_SecPerClus;
    unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;

    DirEntry* dirEntry = (DirEntry*)(file_content + rootClusterOffset);
    if (dirEntry==NULL){
        perror("Erro no mmap");
        exit(1);
    }
    return dirEntry;
}

void showRootDirectory(DirEntry* dirEntry){ // imprime o nome do diretorio
    if (isDirectory(dirEntry)){   
        int idx = 0;
        while(dirEntry->DIR_Name[idx]!=' '){ // nome do diretorio menor que 8 carateres
            printf("%c",dirEntry->DIR_Name[idx]);
            idx++;
        }
        printf("/");
    }
    else{       // nome do diretorio maior que 8
        for(int i=0;i<8;i++){
            if(dirEntry->DIR_Name[i]==' ') 
                break;
            printf("%c",dirEntry->DIR_Name[i]);
        }
        if(dirEntry->DIR_Name[8]!=' '){
            printf(".");
            for(int i=8;i<12;i++){
                    if(dirEntry->DIR_Name[i]==' ')
                    break;
                printf("%c",dirEntry->DIR_Name[i]);
            }
        }
    }
}

void getRootDirectoryEntries(int fd, BootEntry* disk){ // imprime todo o conteudo da fat

    unsigned int nEntries = 0;
    unsigned int currCluster = disk->BPB_RootClus;
    unsigned int totalPossibleEntry = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);

    unsigned char *file_content = getDisk(fd);

    do{
        DirEntry* dirEntry = getclusterPtr(file_content,disk,currCluster);  //adiciona o arquivo na struct
        for(int m=0;m<totalPossibleEntry;m++){
            if (dirEntry->DIR_Attr == 0x00){        // não tem mais diretorios para entrar
                break;
            }

            if(dirEntry->DIR_Name[0] != 0xe5){      // arquivos que comecam com 0xe5 são arquivos deletados (não mostrar)
                showRootDirectory(dirEntry); // imprime
                int startCluster = dirEntry->DIR_FstClusHI << 16 | dirEntry->DIR_FstClusLO;
                printf(" (Tamanho = %d, Primeiro cluster = %d)\n",dirEntry->DIR_FileSize, startCluster);
                nEntries++;
            }
            dirEntry++;
        }
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*currCluster); // proximo
        if(*fat >= 0x0ffffff8 || *fat==0x00){ // fim da fat
            break;
        }
        currCluster=*fat; // atualiza
    } while(1);
    printf("Número total componentes = %d\n",nEntries);
}
