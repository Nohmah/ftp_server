#ifndef __FTP__

#define MAX_NAME_LEN 256
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

#endif