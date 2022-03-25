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

int nOfContiguousCluster(BootEntry *disk, DirEntry *dirEntry){
    int fileSize = dirEntry->DIR_FileSize;
    int nBytesPerCluster = disk->BPB_SecPerClus * disk->BPB_BytsPerSec;
    int n_Clusters = 0;

    if (fileSize % nBytesPerCluster)
        n_Clusters = fileSize/nBytesPerCluster + 1;
    else
        n_Clusters = fileSize/nBytesPerCluster;
    return n_Clusters;
}

bool checkSHA(unsigned char *shaFile, char *shaUser){  // compara dois SHA para ver se é o mesmo
    char shaToChar[SHA_DIGEST_LENGTH*2];

    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(shaToChar+(i*2), "%02x", shaFile[i]);
    if(strcmp(shaToChar, shaUser) == 0)
        return true;
    return false;
}

unsigned char* getShaOfFileContent(BootEntry *disk, DirEntry *dirEntry, unsigned char* file_content){ // obtem o SHA do arquivo
    int n_Clusters = nOfContiguousCluster(disk, dirEntry);
    int startCluster = dirEntry->DIR_FstClusHI << 16 | dirEntry->DIR_FstClusLO;
    unsigned char *fileData = (unsigned char*)malloc(dirEntry->DIR_FileSize * sizeof(unsigned char*));
    unsigned char *sha = (unsigned char*)malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char*));

    int currLen=0;
    if (n_Clusters>1){
        for(int i=0; i<n_Clusters;i++){
            unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (startCluster- 2)*disk->BPB_SecPerClus;
            unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;

            unsigned int bytesInCluster = disk->BPB_SecPerClus * disk->BPB_BytsPerSec;
            int currReadLen = i<n_Clusters-1 ? bytesInCluster : dirEntry->DIR_FileSize - i * bytesInCluster;
            for(int i=0;i<currReadLen;i++){
                fileData[currLen] = file_content[rootClusterOffset+i];
                currLen++;
            }
            startCluster++;
        }
    }
    else{
        unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (startCluster- 2)*disk->BPB_SecPerClus;
        unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;
        for(unsigned int i=0;i<dirEntry->DIR_FileSize;i++){
            fileData[i] = file_content[rootClusterOffset+i];
        }
    }
    SHA1(fileData, dirEntry->DIR_FileSize, sha); // gerar
    return sha;
}

unsigned char* getfilename(DirEntry* dirEntry){ // pega o nome do arquivo
    unsigned char *ptrFile = malloc(12 * sizeof(unsigned char *));
    unsigned int idx=0;
    for(int i=0;i<8;i++){
        if(dirEntry->DIR_Name[i]==' ')
            break;
        ptrFile[idx] = dirEntry->DIR_Name[i];
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

    ptrFile[idx] = '\0';
    return ptrFile;
}

void unmapDisk(unsigned char* file_content, int fileSize){ // desmapear o disco
    munmap(file_content, fileSize);
}

void updateRootDir(unsigned char* file_content , BootEntry* disk, char *filename, int nEntries){ // fuanção para atualizar o diretorio
    unsigned int rootSector = (disk->BPB_RsvdSecCnt + disk->BPB_NumFATs * disk->BPB_FATSz32);
    unsigned int rootClusterOffset = rootSector*disk->BPB_BytsPerSec;

    file_content[rootClusterOffset + nEntries] = (unsigned char) filename[0];

}

void updateFat(unsigned char* file_content , BootEntry* disk, int currCluster, unsigned int value){ // função para atualizar a fat
    for (int i=0; i<disk->BPB_NumFATs; i++){
        unsigned int *fat = (unsigned int*)(file_content + (disk->BPB_RsvdSecCnt + i * disk->BPB_FATSz32) * disk->BPB_BytsPerSec  + 4*currCluster);
        *fat = value;
    }
}

int getDeletedDirEntry(int fd, BootEntry* disk, char *filename, char *shaFile){ // tenta fazer o undelete do arquivo
    unsigned int nEntries=0;
    unsigned int currCluster = disk->BPB_RootClus;
    unsigned int totalPossibleEntry = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);

    struct stat fs;
    if(fstat(fd, &fs) == -1)
    {
        perror("Erro ao ler o stat");
    }
    unsigned char* file_content  = mmap(NULL , fs.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int fileCount=0;
    // guarda a primeira ocorrencia (dirEntry1)
    int nEntries1=0;
    int currCluster1=0;
    DirEntry* dirEntry1;

    do{
        DirEntry* dirEntry = getclusterPtr(file_content,disk,currCluster);
        for(unsigned int m=0;m<totalPossibleEntry;m++){ //verifica outros arquivos 
            if (dirEntry->DIR_Attr == 0x00){        // não tem mais diretorios para entrar
                break;
            }
            if(isDirectory(dirEntry)==0 && dirEntry->DIR_Name[0] == 0xe5){     //não mostra arquivos deletados
                if (dirEntry->DIR_Name[1]==filename[1]){
                    char *recFilename = getfilename(dirEntry);
                    if(strcmp(recFilename+1,filename+1)==0){
                        if (shaFile){ // Necessario o uso do SHA
                            bool shaMatched = checkSHA(getShaOfFileContent(disk, dirEntry, file_content), shaFile);
                            if (shaMatched){
                                fileCount=1;
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                            }
                        }
                        else{
                            // mais de 1 arquivo parecido (Hello, Mello, Tello)
                            if (fileCount<1){
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                                fileCount++;
                            }
                            else{ // erro
                                printf("%s: Erro de ambiguidade. Múltiplos candidatos encontrados.\n",filename);
                                fflush(stdout);
                                return 1;
                            }
                        }
                    }
                }
            }
            dirEntry++;
            nEntries+=sizeof(DirEntry);
        }
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*currCluster);
        if(*fat >= 0x0ffffff8 || *fat==0x00){ // fim da fat
            break;
        }
        currCluster=*fat;
    } while(1);

    if (fileCount==1){
        updateRootDir(file_content, disk, filename, nEntries1);

        int n_Clusters = nOfContiguousCluster(disk, dirEntry1);
        if (n_Clusters>1){ // mais do que um cluster para atualizar
            int startCluster = dirEntry1->DIR_FstClusHI << 16 | dirEntry1->DIR_FstClusLO;
            for(int i=1;i<n_Clusters;i++){
                updateFat(file_content , disk, startCluster, startCluster+1);
                startCluster++;
            }
            updateFat(file_content , disk, startCluster, 0x0ffffff8);
        }
        else
            updateFat(file_content , disk, currCluster1+1, 0x0ffffff8);
        unmapDisk(file_content, fs.st_size);

        if (shaFile)
            printf("%s: Undelete com sucesso usando SHA-1\n",filename);
        else
            printf("%s: Undelete com sucesso\n",filename);
        fflush(stdout);
        return 1;
    }
    return -1;
}

void recoverFile(int fd, BootEntry* disk, char *filename, char *shaFile){ // recupera o arquivo
    if (getDeletedDirEntry(fd, disk, filename, shaFile) == -1){
        printf("%s: arquivo não encontrado\n",filename);
        fflush(stdout);
    }
}
