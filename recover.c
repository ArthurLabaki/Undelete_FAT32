#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#include "fat32disk.h"
#include "directory.h"
#define SHA_DIGEST_LENGTH 20

int nOfContiguousCluster(BootEntry *disk, DirEntry *dirEntry){  //conta o numero de clusters utilizados pelo arquivo
    int fileSize = dirEntry->DIR_FileSize;              // tamanho do arquivo
    int nBytesPerCluster = disk->BPB_SecPerClus * disk->BPB_BytsPerSec; //numero de bytes por cluster (setor/cluster * bytes por setor)
    int n_Clusters = 0;

    if (fileSize % nBytesPerCluster)                    // se existir resto
        n_Clusters = fileSize/nBytesPerCluster + 1;     // o n de clusters é o tam do file / o n de byts de cada cluster + 1 pq resto
    else
        n_Clusters = fileSize/nBytesPerCluster;         // se n é so o tamanho do arquivo / n de bytes de cada cluster
    return n_Clusters;
}

bool checkSHA(unsigned char *shaFile, char *shaUser){   // compara dois SHA para ver se é o mesmo
    char shaToChar[SHA_DIGEST_LENGTH*2];                // vai guardar o SHA

    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(shaToChar+(i*2), "%02x", shaFile[i]);   // tranforma o SHA em char e guarda no shaToChar
    if(strcmp(shaToChar, shaUser) == 0)                 // compara as duas strings
        return true;
    return false;
}

unsigned char* getShaOfFileContent(BootEntry *disk, DirEntry *dirEntry, unsigned char* file_content){ // obtem o SHA do arquivo excluido
    int n_Clusters = nOfContiguousCluster(disk, dirEntry);  // encontra o numero de cluster do arquivo excluido
    int startCluster = dirEntry->DIR_FstClusHI << 16 | dirEntry->DIR_FstClusLO; // encontra o primeiro cluster dele
    unsigned char *fileData = (unsigned char*)malloc(dirEntry->DIR_FileSize * sizeof(unsigned char*)); // aloca espaco para o conteudo dele
    unsigned char *sha = (unsigned char*)malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char*)); // aloca espaco para o hash

    int currLen=0;
    if (n_Clusters>1){                                  // mais do que 1 cluster
        for(int i=0; i<n_Clusters;i++){
            unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (startCluster- 2)*disk->BPB_SecPerClus;
                // encontra o primeiro setor dos clusters, o setor raiz
            unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec; // encontra o offset desse primeiro setor
            
            unsigned int bytesInCluster = disk->BPB_SecPerClus * disk->BPB_BytsPerSec; // encontra o byte por cluster 
            int currReadLen = i<n_Clusters-1 ? bytesInCluster : dirEntry->DIR_FileSize - i * bytesInCluster;
            // se o cluster lido não for o ultimo, currReadLen vai ser o bytes por cluster, se n vai ser o tamanho do arquivo - o cluster lido * byte por cluster
            // ou seja, se n for o ultimo vai ser o tamanho do cluster, se n vai ser o resto do tamanho do arquivo no cluster
            for(int i=0;i<currReadLen;i++){
                fileData[currLen] = file_content[rootClusterOffset+i];  // passa o conteudo do arquivo para o fileDAta
                currLen++;
            }
            startCluster++;                             // proximo cluster
        }
    }
    else{                                               //um unico cluster - arquivo pequeno
        unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (startCluster- 2)*disk->BPB_SecPerClus;
        // encontra o primeiro setor dos clusters, o setor raiz
        unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;   // encontra o offset desse primeiro setor
        for(unsigned int i=0;i<dirEntry->DIR_FileSize;i++){
            fileData[i] = file_content[rootClusterOffset+i];    // passa o conteudo do arquivo para o fileDAta
        }
    }
    SHA1(fileData, dirEntry->DIR_FileSize, sha); // gera o SHA1
    return sha;                                 // retorna esse SHA
}

unsigned char* getfilename(DirEntry* dirEntry){         // pega o nome do arquivo
    unsigned char *ptrFile = malloc(12 * sizeof(unsigned char *));  // aloca espaço para o nome do arquivo na memoria
    unsigned int idx=0;
    for(int i=0;i<8;i++){                       // praticamente a mesma coisa no showRootDirectory no direcoty.c
        if(dirEntry->DIR_Name[i]==' ')
            break;
        ptrFile[idx] = dirEntry->DIR_Name[i];   // não imprime na tela, e sim aloca no ptrFile
        idx++;
    }

    if(dirEntry->DIR_Name[8]!=' '){
        ptrFile[idx] = '.';
        idx++;
    }

    for(int i=8;i<12;i++){
        if(dirEntry->DIR_Name[i]==' ')
            break;
        ptrFile[idx] = dirEntry->DIR_Name[i];
        idx++;
    }

    ptrFile[idx] = '\0';                        // insere um \0 para saber que é o fim do nome
    return ptrFile;                             // retorna o nome
}

void unmapDisk(unsigned char* file_content, int fileSize){ // desmapear o arquivo do disco
    munmap(file_content, fileSize);             
}

void updateRootDir(unsigned char* file_content , BootEntry* disk, char *filename, int nEntries){ // fuanção para atualizar o diretorio
    unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs * disk->BPB_FATSz32);
    unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;
    // encontra o primeiro setor dos clusters e seu offset
    file_content[rootClusterOffset + nEntries] = (unsigned char) filename[0]; // recupera seu primeiro caractere

}

void updateFat(unsigned char* file_content , BootEntry* disk, int currCluster, unsigned int value){ // função para atualizar a fat
    for (int i=0; i<disk->BPB_NumFATs; i++){
        unsigned int *fat = (unsigned int*)(file_content + (disk->BPB_RsvdSecCnt + i * disk->BPB_FATSz32) * disk->BPB_BytsPerSec  + 4*currCluster);
        // Tamanho em setores da área reservada + o numero da FAT * tamanho dos setores da fat * bytes por setor + 4 * custer atual
        // serve para encontrar o lugar onde deve estar o proximo cluster
        *fat = value; // Coloca na area das FATS o seu proximo cluster no cluster atual
    }
}

int getDeletedDirEntry(int fd, BootEntry* disk, char *filename, char *shaFile){ // tenta fazer o undelete do arquivo
    unsigned int nEntries=0;
    unsigned int currCluster = disk->BPB_RootClus; // Cluster onde o diretório raiz pode ser encontrado
    unsigned int totalPossibleEntry = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry); // bytes por cluster / tamanho do dir

    struct stat fs;                                 // struct para arquivos
    if(fstat(fd, &fs) == -1)                        // erro para ler a struct
    {
        perror("Erro ao ler o stat");
    }
    unsigned char* file_content  = mmap(NULL , fs.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // mapeia o diretorio
    // cria uma area compartilhada, alterações feitas na região de mapeamento serão gravadas de volta no arquivo. 
    //alem de permitir o acesso de leitura e gravação 
    int fileCount=0;    // numero de arquivos parecidos (distinguem do primeiro nome)
    int nEntries1=0;
    int currCluster1=0; 
    DirEntry* dirEntry1;

    do{
        DirEntry* dirEntry = getclusterPtr(file_content,disk,currCluster);  // coloca na struct o cluster 
        for(unsigned int m=0;m<totalPossibleEntry;m++){ //verifica todos arquivos no cluster
            if (dirEntry->DIR_Attr == 0x00){        // não tem mais diretorios para entrar (0x00 indica a partir dali n tem mais nada)
                break;
            }
            if(isDirectory(dirEntry)==0 && dirEntry->DIR_Name[0] == 0xe5){     // não for diretorio e for excluido
                if (dirEntry->DIR_Name[1]==filename[1]){            //segundo caractere igual
                    char *recFilename = getfilename(dirEntry);      // pega o nome do arquivo exluido
                    if(strcmp(recFilename+1,filename+1)==0){        // compara os nomes, com exeção do primeiro caractere
                        if (shaFile){                // Necessario o uso do SHA
                            bool shaMatched = checkSHA(getShaOfFileContent(disk, dirEntry, file_content), shaFile); 
                            if (shaMatched){         // sha é igual
                                fileCount=1;         
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                            }
                        }
                        else{                       // Não foi fornecido o sha1
                            if (fileCount<1){   // não tem nenhum arquivo semelhante
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                                fileCount++;    // aumenta em +1, pois pode existir outros
                            }
                            else{ // erro, mais de 1 arquivo parecido (Hello, Mello, Tello)
                                printf("%s: Erro de ambiguidade. Múltiplos candidatos encontrados.\n",filename);
                                fflush(stdout);
                                return 1;
                            }
                        }
                    }
                }
            }
            dirEntry++;                         //proxima entrada do for
            nEntries+=sizeof(DirEntry);
        }
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*currCluster);
        //Tamanho em setores da área reservada*bytes por setor + 4*cluster atual -> calcula o proximo cluster na FAT
        if(*fat >= 0x0ffffff8 || *fat==0x00){   // não tem mais cluster para serem lidos ou parte não utilizada
            break;
        }
        currCluster=*fat;                       // proximo cluster
    } while(1);

    if (fileCount==1){                          // somente um arquivo encontrado
        updateRootDir(file_content, disk, filename, nEntries1); //recupera seu primeiro caractere

        int n_Clusters = nOfContiguousCluster(disk, dirEntry1); // conta o numero de clusters
        if (n_Clusters>1){                      // mais do que um cluster para atualizar
            int startCluster = dirEntry1->DIR_FstClusHI << 16 | dirEntry1->DIR_FstClusLO;   // primeiro cluster
            for(int i=1;i<n_Clusters;i++){      // vai colocar no mapa da fat os devidos proximos clusters
                updateFat(file_content , disk, startCluster, startCluster+1);
                startCluster++;
            }
            updateFat(file_content , disk, startCluster, 0x0ffffff8);   // coloca que ultimo cluster com o 0x0ffffff8 indicando o fim
        }
        else                                    // um unico cluster
            updateFat(file_content , disk, currCluster1+1, 0x0ffffff8); // coloca que ultimo cluster com o 0x0ffffff8 indicando o fim
        unmapDisk(file_content, fs.st_size);    // desmapeia o disco, para evitar alterações indesejadas

        if (shaFile)                            // mostra na tela caso ocorra um sucesso
            printf("%s: Undelete com sucesso usando SHA-1\n",filename);
        else
            printf("%s: Undelete com sucesso\n",filename);
        fflush(stdout);
        return 1;
    }
    return -1;
}

void recoverFile(int fd, BootEntry* disk, char *filename, char *shaFile){ // recupera o arquivo
    if (getDeletedDirEntry(fd, disk, filename, shaFile) == -1){ // função serve somente para entrar na de cima, facilita o file(main)
        printf("%s: arquivo não encontrado\n",filename);
        fflush(stdout);
    }
}
