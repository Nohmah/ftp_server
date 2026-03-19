/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"

#define MAX_NAME_LEN 256
#define N_PROC 5

typedef enum {
    GET,
    PUT,
    LS
} typereq_t;

typedef struct {
    typereq_t type;
    char name[256];
} request_t;

//Tableau global stockant les pids des processus du pool.
pid_t children[N_PROC];

void sigchld_handler(int sig)
{
    pid_t pid;
    int status;

    // Boucle non blocante pour récolter les enfants terminés et arrêtés
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        // printf("[Process %d terminé]\n", pid);
    }
    if (pid == -1 && errno != ECHILD)
    {
        // -1 alors qu'il y a encore des fils à récolter
        perror("waitpid");
    }
    return;
}

void sigint_handler(int sig){
    for(int i = 0; i < N_PROC; i++){
        if(children[i] > 0){
            kill(children[i], SIGINT);
        }
    }

    for(int i = 0; i < N_PROC; i ++){
        if(children[i] > 0) waitpid(children[i], NULL, 0);
    }
}

void echo(int connfd);

/*
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */
int main(int argc, char **argv)
{
    int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
    pid_t pid;

    if (argc != 1)
    {
        fprintf(stderr, "usage: %s\n", argv[0]);
        exit(0);
    }

    Signal(SIGCHLD, sigchld_handler);

    port = 2121;
    listenfd = Open_listenfd(port);


    // Creation du pool
    for (int i = 0; i < N_PROC; i++)
    {
        if ((pid = Fork()) == 0)
        {
            while (1)
            {
                clientlen = sizeof(clientaddr);
                connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
                if (connfd < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    perror("accept");
                    continue;
                }

                Getnameinfo((SA *)&clientaddr, clientlen,
                            client_hostname, MAX_NAME_LEN, 0, 0, 0);

                Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string,
                          INET_ADDRSTRLEN);

                printf("server connected to %s (%s)\n", client_hostname,
                       client_ip_string);

                echo(connfd);
                Close(connfd);
            }
        } else {
            children[i] = pid;
        }
    }
    while (1)
    {
        pause();
    }

    return 0;
}