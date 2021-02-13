//
// Created by giacomo on 13/02/21.
//
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>

#define PAGE_SIZE 4096
#define CSV_SEPARATOR ';'
#define LINE_SEPARATOR '\n'
#define MONTH_SHIFT 3
#define YEAR_SHIFT 6
#define DATE_DIM 10
#define FIRST_DATE_SEP 4
#define SECOND_DATE_SEP 5
#define AUDIT if(0)
#define FILE_PORTION PAGE_SIZE

int one_time = 0; // boolean se 1, non ripete la stessa cosa
long byte_corretti = 0;

int dim_prima_riga;


int skip_first_line(const char *addr) {
    int dim_riga = 0;
    int conteggio_separatori = 0;
    if (one_time == 0) {
        one_time = 1;
        while (addr[dim_riga] != '\n') {
            if (addr[dim_riga] == ';') conteggio_separatori++;
            dim_riga++;
        }
    }
    dim_prima_riga = dim_riga;
    return conteggio_separatori;
}

char *mappa(int fd, long file_size) {
    char *address;
    address = (char *) mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                            0);
    if (address == NULL) {
        perror("mmap errore: ");
    }
    return address;
}

int demappa(char *indirizzo_pagina) {
    int ret;
    if ((ret = munmap(indirizzo_pagina, PAGE_SIZE)) != 0) {
        perror("Errore munmap: ");
    }
    return ret;
}

/**
 *
 * @param addr l'indirizzo del separatore (;) prima di una data
 * @param i
 * @return
 */
char *parseDayOrMonth(const char *addr, long i) {
    unsigned char *giorno_del_mese = malloc(2 * sizeof(unsigned char));
    giorno_del_mese[0] = addr[i + 1];
    giorno_del_mese[1] = addr[i + 2];
    return giorno_del_mese;
}

char *parseYear(const char *addr, long start) {
    char *anno = malloc(4 * sizeof(unsigned char));
    for (int i = 0; i < 4; i++) {
        anno[i] = addr[start + i + 1];
    }
    return anno;
}

/**
 *
 * @param addr Indirizzo al punto esatto in cui scrivere
 * @param offset
 * @param data_corretta
 * @return
 */
int correggi_singola_data(char *addr, long offset, const char *data_corretta) {
    for (int j = 0; j < 10; j++) {
        addr[offset + j + 1] = data_corretta[j];
        AUDIT printf("Carattere %d: %c\n", j, data_corretta[j]);
    }
    return 0;
}

/**
 * Inverte le date per la porzione di file
 * @param addr indirizzo base della porzione di file caricata in RAM
 * @return 0 se tutto va bene.
 */
long invertiDate(char *addr, int separatori_per_riga, long file_size) {
    int count = 0;
    for (long offset = 0; offset < file_size; offset++) {

        if (addr[offset] == CSV_SEPARATOR) {
            count++;
            if (count == FIRST_DATE_SEP || count == SECOND_DATE_SEP) {
                // faccio il parsing della data
                char *day = parseDayOrMonth(addr, offset); //;01-02-2020
                char *month = parseDayOrMonth(addr, offset + MONTH_SHIFT);  //012345678910
                char *year = parseYear(addr, offset + YEAR_SHIFT);
                char *data_corretta = malloc(DATE_DIM * sizeof(unsigned char));
                sprintf(data_corretta, "%s-%s-%s", year, month, day);
                printf("Data corretta: %s\n", data_corretta);

                // copio la prima data nel file:
                if (addr[offset+1] != CSV_SEPARATOR) {
                    correggi_singola_data(addr, offset, data_corretta);
                }

            } else if (count == separatori_per_riga) {
                count = 0;
            }
        }

    }
    return 0;
}


/**
 * TODO: se la pagina si ferma a metà di una riga, tornare indietro all'ultimo \n!!!
 * Attenzione la working directory è CMakeFiles!!!
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    struct stat statistiche_file;

    printf("%s\n", argv[1]);


    if (argc < 2) {
        printf("Utilizzo: prog file\n");
        return 0;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Errore nella open");
        return 0;
    }

    if (fstat(fd, &statistiche_file) != 0) {
        perror("Errore nella fstat");
    }

    long file_size = statistiche_file.st_size;
    printf("Dimensione file: %ld\n", file_size);

    char *addr;
    int indici = (int) file_size / PAGE_SIZE + 1;
    int numero_separatori = 0;
    printf("numero indici +1:%d\n", indici);
    addr = mappa(fd, file_size);
    if (one_time == 0) {
        numero_separatori = skip_first_line(addr);
        printf("Numero separatori: %d\n", numero_separatori);
    }
    invertiDate(addr + dim_prima_riga + 1, numero_separatori, file_size);

    demappa(addr);

    close(fd);
    return 0;
}
