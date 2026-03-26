#ifndef __FTP__
#define __FTP__

#include <stdint.h>

#define MAX_NAME_LEN 256
#define MAX_BLOCK_SIZE 4096
#define SERV_DIR "./servdir/"
#define CLIENT_DIR "./clientdir/" 
#define SUCCES 0
#define ERREUR 1

typedef enum {
    GET,
    PUT,
    LS,
    ERR
} typereq_t;

typedef struct {
    typereq_t type;
    char name[MAX_NAME_LEN];
} request_t;


typedef struct {
    int code_retour;
    char message[MAX_NAME_LEN];
} response_t;

typedef struct {
    uint32_t block_size;
    uint64_t file_size;
    uint32_t block_count;
} transfer_t;

#endif