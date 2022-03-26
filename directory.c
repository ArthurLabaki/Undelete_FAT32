#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "fat32disk.h"

bool isDirectory(DirEntry* dirEntry){                   // True se for um diretorio
    return dirEntry->DIR_Attr == 0x10;                  // Atributo do arquivo (0x10 = subdiretorio)
}

char* getDisk(unsigned int fd){                         // retorna o disco mapeado dado o arquivo
    struct stat fs;                                     // struct para arquivos
    if(fstat(fd, &fs) == -1)                            // erro ao ler o arquivo na stat
    {
        perror("Erro ao ler o stat");
    }
    return mmap(NULL , fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // Retorna o disco do arquivo mapeado
}

//PS: OffSet é a distancia entre o primeiro elemento e o desejado, no caso do root, é todo o espaco utilizado

DirEntry* getclusterPtr(char* file_content, BootEntry* disk, unsigned int cluster){  // cria a struct para o arquivo e cluster no disco
    unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (cluster- 2)*disk->BPB_SecPerClus;
    // calcula o setor root com (tamanho em setores da área reservada + numero de fats *  Tamanho de 32 bits em setores de uma FAT) +
    // o (total de cluster -2 )* setor por cluster /// serve para encontrar o primeiro setor dos clusters
    unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec; // calcula o offset com o numero de setores * bytes por setor
    DirEntry* dirEntry = (DirEntry*)(file_content + rootClusterOffset); // coloca na struct passando o mmap e o offset do root
    if (dirEntry==NULL){                                // erro ao passar o mmap
        perror("Erro no mmap");
        exit(1);
    }
    return dirEntry;                                    // retorna a struct criada
}

void showRootDirectory(DirEntry* dirEntry){             // imprime o nome do diretorio
    if (isDirectory(dirEntry)){                         // se for diretorio vai imprimir o nome dele com uma / depois
        int idx = 0;
        while(dirEntry->DIR_Name[idx]!=' '){ // nome do diretorio menor que 8 carateres
            printf("%c",dirEntry->DIR_Name[idx]);
            idx++;
        }
        printf("/");
    }
    else{                                               // se não for firetorio vai imprimir so o nome dele
        for(int i=0;i<8;i++){
            if(dirEntry->DIR_Name[i]==' ') 
                break;
            printf("%c",dirEntry->DIR_Name[i]);
        }                                               // imprime sua extenção, se existir
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

void getRootDirectoryEntries(int fd, BootEntry* disk){  // imprime todo o conteudo da fat a partir do diretorio raiz

    unsigned int nEntries = 0;
    unsigned int currCluster = disk->BPB_RootClus;  //Cluster onde o diretório raiz pode ser encontrado
    unsigned int totalPossibleEntry = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);
    // tem o total de possibilidade dividindo o byte por por cluster pelo tamanho da tabela de diretorios (numero total de entradas na tabela)
    unsigned char *file_content = getDisk(fd);     // file_content é um ponteiro para o disco mapeado 

    do{
        DirEntry* dirEntry = getclusterPtr(file_content,disk,currCluster);  //adiciona o mapeamento do diretorio atual na struct
        for(int m=0;m<totalPossibleEntry;m++){      // verifica o numerototal de entradas possiveis nesse cluster
            if (dirEntry->DIR_Attr == 0x00){        // não tem mais diretorios para entrar
                break;
            }

            if(dirEntry->DIR_Name[0] != 0xe5){      // não mostrar arquivos excluidos (0xe5)
                showRootDirectory(dirEntry);        // imprime o arquivo 
                int startCluster = dirEntry->DIR_FstClusHI << 16 | dirEntry->DIR_FstClusLO; 
                // encontra o primeiro cluster com o cluster HIGH ou cluster LOW
                printf(" (Tamanho = %d, Primeiro cluster = %d)\n",dirEntry->DIR_FileSize, startCluster);
                nEntries++;
            }
            dirEntry++;                             // proximo arquivo/diretorio
        }
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*currCluster); // proximo
        //Tamanho em setores da área reservada*bytes por setor + 4*cluster atual -> calcula o proximo cluster na FAT
        if(*fat >= 0x0ffffff8 || *fat==0x00){       // não tem mais cluster para serem lidos ou parte não utilizada
            break;
        }
        currCluster=*fat;                           // atualiza / entra no proximo cluster
    } while(1);
    printf("Número total componentes = %d\n",nEntries); // imprime o numero de entradas lidas
}
