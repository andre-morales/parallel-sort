#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int get_chave(int i, int num_regs, int tipo) {
    switch (tipo) {
    case 1:
        return i+1;
        break;
    case 2:
        return num_regs - i;
        break;
    case 3:
        return (int) rand();
        break;
    
    default:
        break;
    }
    return -1;
}

int WRITE(int fd, char* data, size_t length) {
    return write(fd, data, length);
}

int main(int argc, char **argv) {
    const char * const tipos[] = {[1] = "aleatorio", [2] = "em ordem", [3] = "ordem reversa"};
    if (argc < 4) {
        printf("modo de uso:\n");
        printf("\t%s <numero de registros> <tipo> <nome do arquivo de saida> [semente]\n", argv[0]);
        printf("\t Onde tipo = 1 => registros com chaves em ordem\n");
        printf("\t      tipo = 2 => registros com chaves em ordem reversa\n");
        printf("\t      tipo = 3 => registros com chaves aleatorios\n");
        return -1;
    }
    char *filename = argv[3];
    char *end;

    int num_regs =  strtol(argv[1], &end, 10);
    if (*end != '\0') {
        printf("Numero de registros invalido: %s\n", argv[1]);
        return -1;
    }
    int tipo =  strtol(argv[2], &end, 10);
    if (*end != '\0' || tipo <= 0 || tipo > 3) {
        printf("Tipo invalido: %s\n", argv[2]);
        return -1;
    }

    int semente = 2024;
    if (argc >= 5) {
        semente = strtol(argv[4], &end, 10);
        if (*end != '\0') {
            printf("Semente invalida: %s\n", argv[4]);
            return -1;
        }
    }
    srand(semente);

    double tamanho_k = 100.0 * num_regs / 1024;
    double tamanho_m = tamanho_k / 1024;
    double tamanho_g = tamanho_m / 1024;

    printf("Arquivo: %s\n", filename);
    printf("Tamanho: %d bytes (%.2f KB, %.2f MB, %.2f GB)\n", num_regs * 100, tamanho_k, tamanho_m, tamanho_g);
    printf("Tipo: %s\n", tipos[tipo]);
    printf("Semente: %d\n", semente);
    printf("\n");
    printf("Prosseguir? (s/n)\n");
    /*char resposta = getchar();
    if (resposta != 's') {
        exit(0);
    }*/

    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    int r;
    if (fd == -1) {
        printf("Nao foi possivel abrir o arquivo\n");
        exit(-1);
    }

    char* data = malloc(num_regs * 100);
    char* writePtr = data;

    for (int i = 0; i < num_regs; i++) {
        int chave = get_chave(i, num_regs, tipo);
       
        memcpy(writePtr, &chave, sizeof(int));
        writePtr += 4;

        //r = write(fd, &chave, sizeof(int));
        //assert(r >= 0);
        for (int j = 0; j < 96; j++) {
            unsigned char rnd = rand() % 256;
            
            *writePtr = rnd;
            writePtr++;
            
            //r = WRITE(fd, &rnd, 1);
           // assert(r >= 0);
        }
    }

    write(fd, data, num_regs * 100);
    fsync(fd);
    close(fd);

    return 0;
}

