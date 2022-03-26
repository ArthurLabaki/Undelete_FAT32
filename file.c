#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "fat32disk.h"
#include "directory.h"
#include "recover.h"

int main(int argc, char **argv){ // main
    int flag=0, anyOption=0;
    int information=0, listRootDir=0, recoverFiles=0, sha=0;
    char *recOptarg, *shaOptarg;

    // qual input é pra fazer
    while((flag=getopt(argc, argv, "ilr:s:")) != -1){
        anyOption=1;
        switch(flag){
            case 'i':                                   // -i para informação do sistema (informações)
                information=1;
                break;
            case 'l':                                   // -l para listar o diretorio raiz (listar)
                listRootDir=1;
                break;
            case 'r':                                   // -r para recuperar o arquivo (recuperar)
                recoverFiles=1;
                recOptarg = optarg;
                break;
            case 's':                                   // -s para recuperar um arquivo com SHA
                sha = 1;
                shaOptarg = optarg;
                break;
            default:                                    // outro comando errado ou nenhum comando digitado
                showUsage();
                break;
        }
    }

    if (anyOption==0 || optind==argc)                   // instrução
        showUsage();
                                     
    int fd = getFileDirectory(argv[optind]);            // pega informação do sistema de arquivos
    BootEntry* disk = readFileSystem(fd);               // mapeia o disco na struct
    // Envia para a função, dependendo da qual foi escolhida
    if (information)
        showDiskInformation(disk);
    else if (listRootDir)
        getRootDirectoryEntries(fd, disk);
    else if(sha){                                       // recuperar com SHA
        if(recoverFiles){
            if (recOptarg == NULL)                      // so usa SHA quando -r estiver
            showUsage();
        recoverFile(fd, disk, recOptarg, shaOptarg);
        }
        else{
            showUsage();
        }
    }
    else if(recoverFiles && !sha){                      //recuperar sem SHA
        if (recOptarg == NULL)
            showUsage();
        recoverFile(fd, disk, recOptarg, NULL);
    }
}
