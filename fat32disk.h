#ifndef FATFILESYSTEM_H
#define FATFILESYSTEM_H

#include <stdlib.h>
#include <stdint.h>

#pragma pack(push,1)  // Struct para o sistema de arquivos
typedef struct BootEntry {
  unsigned char  BS_jmpBoot[3];     // Instruções em assembly para pular para o código de inicialização
  unsigned char  BS_OEMName[8];     // OEM nome em ASCII
  unsigned short BPB_BytsPerSec;    // Bytes por setor. Os valores permitidos incluem 512, 1024, 2048 e 4096
  unsigned char  BPB_SecPerClus;    // Setores por cluster (unidade de dados). Os valores permitidos são potências de 2, mas o tamanho do cluster deve ser de 32 KB ou menor
  unsigned short BPB_RsvdSecCnt;    // Tamanho em setores da área reservada
  unsigned char  BPB_NumFATs;       // Número de FATs
  unsigned short BPB_RootEntCnt;    // Número máximo de arquivos no diretório raiz para FAT12 e FAT16. Isso é 0 para FAT32
  unsigned short BPB_TotSec16;      // Valor de 16 bits do número de setores no sistema de arquivos
  unsigned char  BPB_Media;         // Tipo de mídia
  unsigned short BPB_FATSz16;       // Tamanho de 16 bits em setores de cada FAT para FAT12 e FAT16. Para FAT32, este campo é 0
  unsigned short BPB_SecPerTrk;     // Setores por faixa do dispositivo de armazenamento
  unsigned short BPB_NumHeads;      // Número de Heads no dispositivo de armazenamento
  unsigned int   BPB_HiddSec;       // Número de setores antes do início da partição
  unsigned int   BPB_TotSec32;      // Valor de 32 bits do número de setores no sistema de arquivos. Este valor ou o valor de 16 bits acima deve ser 0
  unsigned int   BPB_FATSz32;       // Tamanho de 32 bits em setores de uma FAT
  unsigned short BPB_ExtFlags;      // Uma flag para FAT
  unsigned short BPB_FSVer;         // O número da versão principal e secundária
  unsigned int   BPB_RootClus;      // Cluster onde o diretório raiz pode ser encontrado
  unsigned short BPB_FSInfo;        // Setor onde a estrutura FSINFO pode ser encontrada
  unsigned short BPB_BkBootSec;     // Setor onde a cópia de backup do setor de inicialização está localizada
  unsigned char  BPB_Reserved[12];  // Reservado
  unsigned char  BS_DrvNum;         // Número da unidade BIOS INT13h
  unsigned char  BS_Reserved1;      // Não usado
  unsigned char  BS_BootSig;        // Assinatura de inicialização estendida para identificar se os próximos três valores são válidos
  unsigned int   BS_VolID;          // Número de série do volume
  unsigned char  BS_VolLab[11];     // Label de volume em ASCII. O usuário define ao criar o sistema de arquivos
  unsigned char  BS_FilSysType[8];  // Label do tipo de sistema de arquivos em ASCII
} BootEntry;
#pragma pack(pop)

#pragma pack(push,1)  // Struct para os diretorios e arquivos na FAT
typedef struct DirEntry {
  unsigned char  DIR_Name[11];      // Nome do arquivo
  unsigned char  DIR_Attr;          // Atributos do arquivo
  unsigned char  DIR_NTRes;         // Reservado
  unsigned char  DIR_CrtTimeTenth;  // Data de criação (décimos de segundo)
  unsigned short DIR_CrtTime;       // Data de criação (horas, minutos e segundos)
  unsigned short DIR_CrtDate;       // Dia de criação
  unsigned short DIR_LstAccDate;    // Dia de acesso
  unsigned short DIR_FstClusHI;     // 2 bytes mais altos do primeiro endereço do cluster (cluster high)
  unsigned short DIR_WrtTime;       // Data da modificação (horas, minutos e segundos)
  unsigned short DIR_WrtDate;       // Dia da modificação 
  unsigned short DIR_FstClusLO;     // 2 bytes mais baixos do primeiro endereço de cluster (cluster low)
  unsigned int   DIR_FileSize;      // Tamanho do arquivo em bytes. (0 para diretórios)
} DirEntry;
#pragma pack(pop)

unsigned int getFileDirectory(const char *diskName);
BootEntry* readFileSystem(int fd);
void showDiskInformation(BootEntry* disk);
void showUsage();

#endif
