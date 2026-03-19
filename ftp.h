#ifndef __FTP__

#define MAX_NAME_LEN 256
#define SERV_DIR "./servdir/"
#define CLIENT_DIR "./clientdir/" 

typedef enum {
    GET,
    PUT,
    LS
} typereq_t;

typedef struct {
    typereq_t type;
    char name[MAX_NAME_LEN];
} request_t;

#endif