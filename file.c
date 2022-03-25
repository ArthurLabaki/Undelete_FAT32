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
            case 'i': // -i para informação do sistema
                information=1;
                break;
            case 'l': // -l para listar o diretorio raiz
                listRootDir=1;
                break;
            case 'r': // -r para recuperar o arquivo
                recoverFiles=1;
                recOptarg = optarg;
                break;
            case 's': // -s para recuperar um arquivo com SHA
                sha = 1;
                shaOptarg = optarg;
                break;
            default:
                showUsage();
                break;
        }
    }

    if (anyOption==0 || optind==argc) // instrução
        showUsage();

    // pega informação do sistema de arquivos
    int fd = getFileDirectory(argv[optind]);
    BootEntry* disk = readFileSystem(fd);

    if (information)
        showDiskInformation(disk);
    else if (listRootDir)
        getRootDirectoryEntries(fd, disk);
    else if(sha){ // so usa SHA quando -r estiver
        if(recoverFiles){
            if (recOptarg == NULL)
            showUsage();
        recoverFile(fd, disk, recOptarg, shaOptarg);
        }
        else{
            showUsage();
        }
    }
    else if(recoverFiles && !sha){ //recuperar sem SHA
        if (recOptarg == NULL)
            showUsage();
        recoverFile(fd, disk, recOptarg, NULL);
    }
}
