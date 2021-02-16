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
#define MONTH_SHIFT 3   // dd-
#define MONTH_SHIFT_NO_DASH 2
#define YEAR_SHIFT 6    // dd-MM-
#define YEAR_SHIFT_NO_DASH 4
#define HOUR_SHIFT 11    // dd-MM-yyyy_
#define HOUR_SHIFT_NO_DASH 8
#define MINUTE_SHIFT 14 // dd-MM-yyyy_hh:
#define MINUTE_SHIFT_NO_DASH 10
#define DATE_DIM 10
#define NO_DASH_DATE_DIM 8
#define TIMESTAMP_DIM 24
#define NO_DASH_TIMESTAMP_DIM 13
#define AUDIT if(0)
#define FILE_PORTION PAGE_SIZE
#define FORMAT_LENGHT 32
int one_time = 0; // boolean se 1, non ripete la stessa cosa
long byte_corretti = 0;

int dim_prima_riga;

int lasciare_trattini = 1;
char formattazione[FORMAT_LENGHT] = "%s-%s-%s";
int posizioni_date[20] = {4, 5};

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
    address = (char *) mmap(NULL,
                            file_size,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            fd,
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

int raggiuntaData(int separators_count) {
    int j = 0;
    int size = sizeof(posizioni_date) / sizeof(posizioni_date[0]);
    for (j = 0; j < size; j++) {
        if (separators_count == posizioni_date[j]) {
            return 1;
        }
    }
    return 0;
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
int correggi_singola_data(char *addr, long offset, const char *data_corretta, unsigned long byteDaModificare) {
    for (int j = 0; j < byteDaModificare; j++) {
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
long invertiDate(char *addr, int separatori_per_riga, long file_size, int numParse) {
    int count = 0;
    for (long offset = 0; offset < file_size; offset++) {
        if (addr[offset] == CSV_SEPARATOR) {
            count++;
            if (raggiuntaData(count) && addr[offset + 1] != CSV_SEPARATOR) {
                // faccio il parsing della data dd-MM-yyyy
                char *day = parseDayOrMonth(addr, offset);
                char *month = parseDayOrMonth(addr, offset + (lasciare_trattini ? MONTH_SHIFT : MONTH_SHIFT_NO_DASH));
                char *year = parseYear(addr, offset + (lasciare_trattini ? YEAR_SHIFT : YEAR_SHIFT_NO_DASH));
                char *data_corretta;
                if (numParse == 5) {
                    char *hour = parseDayOrMonth(addr, offset + (lasciare_trattini ? HOUR_SHIFT : HOUR_SHIFT_NO_DASH));
                    char *minute = parseDayOrMonth(addr,
                                                   offset + (lasciare_trattini ? MINUTE_SHIFT : MINUTE_SHIFT_NO_DASH));
                    data_corretta = malloc(
                            (lasciare_trattini ? TIMESTAMP_DIM : NO_DASH_TIMESTAMP_DIM) * sizeof(unsigned char));
                    sprintf(data_corretta, formattazione, year, month, day, hour, minute);
                } else {
                    data_corretta = malloc((lasciare_trattini ? DATE_DIM : NO_DASH_DATE_DIM) * sizeof(unsigned char));
                    sprintf(data_corretta, formattazione, year, month, day);
                }
                unsigned long byte_da_modificare = (unsigned long) strlen(data_corretta);
                // printf("Data corretta: %s\n", data_corretta);
                // aggiorno la data nel file in yyyy-MM-dd, ma solo se non è nulla:
                correggi_singola_data(addr, offset, data_corretta, byte_da_modificare);
            }
        } else if (count == separatori_per_riga) {
            count = 0;
        }
    }
    return 0;
}

void usage(void) {
    printf("Utilizzo: prog file\n");
    printf("Per convertire le date da ddMMyyyy a yyyyMMdd senza trattini: \n\tprog file -no-dash\n\tprog file -n\n");
    printf("Per convertire timestamp con trattini dal formato dd-MM-yyyy hh:mm:ss.millis a yyyy-MM-dd hh:mm:00.000000: \n\tprog file -yMdhm\n\tprog file -ymdhm\n\tprog file -t\n");
    printf("Per convertire timestamp senza trattini dal formato ddMMyyyyhhmm a yyyyMMddhhmm: \n\tprog file -no-dash -ymdhm\n\tprog file -n -t\n");
    printf("La conversione di default è con trattini da dd-MM-yyyy a yyyy-MM-dd\n");
}

void command_line_parsing(int argc, char *argv[], int *numParse) {
    int k = 2;
    int isDate = 0;
    char *formatDate = "%s-%s-%s";
    char *formatTimestamp = "%s-%s-%s %s:%s:00.000000";
    char *formatDateNoDash = "%s%s%s";
    char *formatTimestampNoDash = "%s%s%s%s%s";
    while (k < argc) {
        if (strcmp(argv[k], "-no-dash") == 0 ||
            strcmp(argv[k], "-nodash") == 0 ||
            strcmp(argv[k], "-n") == 0) {
            lasciare_trattini = 0;
        } else if (strcmp(argv[k], "-yyyyMMdd") == 0 ||
                   strcmp(argv[k], "-yMd") == 0 ||
                   strcmp(argv[k], "-ymd") == 0 ||
                   strcmp(argv[k], "-d") == 0) {
            isDate = 1;
        } else if (strcmp(argv[k], "-yyyyMMddhhmm") == 0 ||
                   strcmp(argv[k], "-yMdhm") == 0 ||
                   strcmp(argv[k], "-ymdhm") == 0 ||
                   strcmp(argv[k], "-t") == 0) {
            isDate = 0;
        } else if (strcmp(argv[k], "-help") == 0 || strcmp(argv[k], "-h") == 0){
            usage();
            exit(0);
        } else {
            printf("Opzione sconosciuta");
            usage();
            exit(0);
        }
        k++;
    }
    *numParse = (isDate ? 3 : 5);
    if (isDate && lasciare_trattini) {
        strcpy(formattazione, formatDate);
    } else if (isDate && !lasciare_trattini) {
        strcpy(formattazione, formatDateNoDash);
    } else if (!isDate && lasciare_trattini) {
        strcpy(formattazione, formatTimestamp);
    } else if (!isDate && !lasciare_trattini) {
        strcpy(formattazione, formatTimestampNoDash);
    }

    // [3], perché ci deve essere il terminatore stringa
    /*char example[6][3] = {"20", "21", "04", "14", "12", "00"};
    printf("numParse %d. Di seguito un esempio di formattazione della data 2021-04-14 12:00:00.000000\n", *numParse);
    if (*numParse == 5)
        printf(strcat(formattazione, "\n"), strcat(example[0], example[1]), example[2], example[3], example[4],
               example[5]);
    else
        printf(strcat(formattazione, "\n"), strcat(example[0], example[1]), example[2], example[3]);*/
}

/**
 * TODO: se la pagina si ferma a metà di una riga, tornare indietro all'ultimo \n!!!
 * Attenzione la working directory è CMakeFiles!!!
 * @param argc almeno due parametri
 * @param argv il primo parametro deve essere il file csv da modificare
 *
 * @return
 */
int main(int argc, char *argv[]) {
    struct stat statistiche_file;
    int numParse = 3;
    int i = 0;

    if (argc < 2 || strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    } else if (argc > 2) {
        command_line_parsing(argc, argv, &numParse);
    }

    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Errore nella open");
        usage();
        return 0;
    }

    if (fstat(fd, &statistiche_file) != 0) {
        perror("Errore nella fstat");
    }

    long file_size = statistiche_file.st_size;
    printf("Dimensione file: %ld\n", file_size);

    if (1) {
        char *addr;
        int numero_separatori = 0;
        // int indici = (int) file_size / PAGE_SIZE + 1;
        // printf("numero indici +1:%d\n", indici);
        addr = mappa(fd, file_size);
        if (one_time == 0) {
            numero_separatori = skip_first_line(addr);
            printf("Numero separatori: %d\n", numero_separatori);
        }
        invertiDate(addr + dim_prima_riga + 1, numero_separatori, file_size, numParse);

        demappa(addr);
    }
    close(fd);
    return 0;
}
