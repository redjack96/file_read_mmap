#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#define PAGE_SIZE 4096
#define CSV_SEPARATOR ';'
#define LINE_SEPARATOR '\n'
#define MONTH_SHIFT 3
#define YEAR_SHIFT 6
#define DATE_DIM 10
#define TIMESTAMP_DIM 23
#define FIRST_DATE_SEP 4
#define SECOND_DATE_SEP 5
#define SEPARATORS_PER_LINE 7
#define AUDIT if(0)
#define FILE_PORTION PAGE_SIZE

int one_time = 0; // boolean se 1, non ripete la stessa cosa
int dim_prima_riga;
long byte_corretti = 0;

char *mappa(int fd, long file_size);

int demappa(char *indirizzo_pagina);

char *parseDayOrMonth(const char *addr, int i);

char *parseYear(const char *addr, long start);

void modifica_data(const char *address, int start, unsigned char *corretta);

int skip_first_line(const char *addr);

long invertiDate(char *addr, int separatori_per_riga, int indice);

int correggi_singola_data(char *addr, long offset, const char *data_corretta);

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
    int indici = (int) file_size / PAGE_SIZE;
    int numero_separatori;
    printf("numero indici +1:%d\n", indici);
    addr = mappa(fd, file_size);
    if (one_time == 0) {
        numero_separatori = skip_first_line(addr);
        printf("Numero separatori: %d\n", numero_separatori);
    }
    byte_corretti = invertiDate(addr, numero_separatori, 0); //5397
    printf("byte_corretti = %ld", byte_corretti);

    for (int i = 1; i < indici; i++) {

        demappa(addr);
        lseek(fd, byte_corretti + 1, SEEK_SET);

        long dimensione_rimanente = file_size - i * FILE_PORTION;
        addr = mappa(fd, dimensione_rimanente);

        byte_corretti = invertiDate(addr, numero_separatori, i); //5397
        printf("byte_corretti = %ld", byte_corretti);
    }


    // msync(addr, file_size, MS_SYNC);
    // demappa(addr);

    // AUDIT printf("File completo in RAM:\n%s\n", addr);

    close(fd);
    return 0;
}

/**
 * Inverte le date per la porzione di file
 * @param addr indirizzo base della porzione di file caricata in RAM
 * @return long: numero di byte a partire dall'inizio della porzione del file su cui fare lseek.
 */
long invertiDate(char *addr, int separatori_per_riga, int indice) {
    unsigned long riga = 0;
    int finito = 0;
    int byte_trattati_porzione = 0;
    long posizione_termine_riga;
    for (long offset = byte_corretti;
         offset < (indice + 1) * FILE_PORTION; offset++) { // offset parte da porzione a FILE_PORTION (4096)
        /**
         * 1) Copio ogni riga, finché non arrivo a \n o EOF, tenendo traccia delle posizioni dei separatori che precedono le date
         * 2) Salvo la posizione dell'ultimo \n
         * 3) Se la riga contiene tutti i separatori, eseguo la modifica e ricomincio dal passo 1, altrimenti restituisco l'indirizzo al passo 2.
         */
        int dimensione_riga = 0;
        char *indirizzo_prima_data;
        char *indirizzo_seconda_data;
        int starting_row_offset = offset;

        // 1)
        int num_sep = 0;
        while (offset - indice * FILE_PORTION <= 0) {
            if (addr[offset] == CSV_SEPARATOR) {
                num_sep++; // Conto i separatori presenti in essa
                if (num_sep ==
                    FIRST_DATE_SEP) { // mantengo traccia della posizione dei separatori che precedono le date
                    indirizzo_prima_data = &addr[offset];
                } else if (num_sep == SECOND_DATE_SEP) {
                    indirizzo_seconda_data = &addr[offset];
                }
            }
            if (addr[offset] == LINE_SEPARATOR) {
                posizione_termine_riga = offset;
                break;
            } else if (addr[offset] == LINE_SEPARATOR) break;
            offset++;
            dimensione_riga++;  // calcolo la dimensione della riga.
        }
        // 2) mantengo la posizione dell'ultimo terminatore di riga (EOF o \n)

        // se la riga non ha tutti i separatori, non tocco la riga.
        if (num_sep != separatori_per_riga) {
            AUDIT printf("File dal termine riga: \n%s\n", &addr[posizione_termine_riga]);
            printf("Separatori insufficienti: %d, Indice: %d, Porzione di file:\n%s\n", num_sep, indice, addr);
            return posizione_termine_riga;
        }

        // copio la riga
        char *riga_copiata = malloc(dimensione_riga * sizeof(char *));
        for (int k = 0; k < dimensione_riga; k++) {
            riga_copiata[k] = addr[starting_row_offset + k];
        }
        printf("Riga copiata: %s\n", riga_copiata);

        // faccio il parsing della prima data
        char *day = parseDayOrMonth(indirizzo_prima_data, 0); //;01-02-2020
        char *month = parseDayOrMonth(indirizzo_prima_data, MONTH_SHIFT);  //012345678910
        char *year = parseYear(indirizzo_prima_data, YEAR_SHIFT);
        char *data_corretta = malloc(DATE_DIM * sizeof(unsigned char));
        sprintf(data_corretta, "%s-%s-%s", year, month, day);
        printf("Data corretta: %s\n", data_corretta);

        // copio la prima data nel file:
        for (int v = 0; v < DATE_DIM; v++) {
            *(indirizzo_prima_data + v + 1) = data_corretta[v];
        }

        // faccio il parsing della seconda data
        day = parseDayOrMonth(indirizzo_seconda_data, 0); //;01-02-2020
        month = parseDayOrMonth(indirizzo_seconda_data, MONTH_SHIFT);  //012345678910
        year = parseYear(indirizzo_seconda_data, YEAR_SHIFT);
        data_corretta = malloc(DATE_DIM * sizeof(unsigned char));
        sprintf(data_corretta, "%s-%s-%s", year, month, day);

        // copio la seconda data nel file:
        for (int v = 0; v < DATE_DIM; v++) {
            *(indirizzo_seconda_data + v + 1) = data_corretta[v];
        }

    }
    printf("Porzione di file: %s\n", addr);
    printf("Indice: %d, posizione termine riga:\n%s\n", indice, posizione_termine_riga);
    return posizione_termine_riga;
}

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
    if (file_size > PAGE_SIZE) {
        address = (char *) mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                page_index * PAGE_SIZE);
        // lseek(fd,);
        // address[PAGE_SIZE-1] = '\0';
    } else {
        address = (char *) mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                page_index * PAGE_SIZE);
        // address[file_size-1] = '\0';
    }


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

char *parseDayOrMonth(const char *addr, int i) {
    unsigned char *giorno_del_mese = malloc(2 * sizeof(unsigned char));
    giorno_del_mese[0] = addr[i + 1];
    giorno_del_mese[1] = addr[i + 2];
    return giorno_del_mese;
}

char *parseYear(const char *addr, long start) {
    unsigned char *anno = malloc(4 * sizeof(unsigned char));
    for (int i = 0; i < 4; i++) {
        anno[i] = addr[start + i + 1];
    }
    return anno;
}

int correggi_singola_data(char *addr, long offset, const char *data_corretta) {
    for (int j = 0; j < 10; j++) {
        addr[offset + 1 + j] = data_corretta[j];
        AUDIT printf("Carattere %d: %c\n", j, data_corretta[j]);
    }
    return 0;
}
