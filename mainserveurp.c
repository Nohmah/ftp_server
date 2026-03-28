/*
 * mainserveurp.c - Un serveur avec pool de processus
 */

#include "csapp.h"
#include "ftp.h"
#include "time.h"

#define MAX_NAME_LEN 256
#define N_PROC 5

#define SERV_DIR "./servdir/"

volatile sig_atomic_t children_alive = N_PROC;

//Tableau global stockant les pids des processus du pool.
pid_t children[N_PROC];

void sigchld_handler(int sig)
{
    pid_t pid;
    int status;

    // Boucle non blocante pour récolter les enfants terminés et arrêtés
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if(WIFEXITED(status) || WIFSIGNALED(status)){
            children_alive --;
        }
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
}

void handle_connection(int connfd)
{
    request_t req;
    response_t response;
    transfer_t transfer;
    char filepath[MAX_NAME_LEN + sizeof(SERV_DIR)];
    char buf[MAX_BLOCK_SIZE];
    int filefd;
    //int size_t;
    ssize_t n;
    struct stat sf;

    while (1){
        //Lit la requête du client
        n = Rio_readn(connfd, &req, sizeof(req));
        //Le client a fermé la connexion
        if (n == 0){
            printf("Client deconnecte\n");
            break;
        }
        //La requête est illisible
        else if (n != sizeof(req)) {
            fprintf(stderr, "Erreur lecture requête\n");
            break;
        }
        if (req.type == GET){

            uint64_t offset = req.offset;
            //Construit le chemin du fichier
            snprintf(filepath, sizeof(filepath), "%s%s", SERV_DIR, req.name);
            printf("GET %s\n", filepath);


            filefd = open(filepath, O_RDONLY);

            if (filefd < 0)
            {   
                //En faire une fonction
                response.code_retour = ERREUR;
                strcpy(response.message, "Fichier introuvable dans le serveur");
                Rio_writen(connfd, &response, sizeof(response));
                //fprintf(stderr, "Fichier introuvable : %s\n", filepath);
                continue;
            }
            response.code_retour = SUCCES;
            Rio_writen(connfd, &response, sizeof(response));
            //Récupère la taille du fichier pour lecture en 1 fois.
            fstat(filefd, &sf);

            //Sécurité si la valeur de offset est > que la taille du fichier
            if(offset > sf.st_size){
                offset = 0;
            }
            

            lseek(filefd, offset, SEEK_SET);
            transfer.block_size = MAX_BLOCK_SIZE;
            transfer.file_size =(uint64_t)sf.st_size - offset;
            transfer.block_count = (uint32_t)((transfer.file_size + MAX_BLOCK_SIZE -1) / MAX_BLOCK_SIZE);
            Rio_writen(connfd, &transfer, sizeof(transfer));

            //Lecture et envoi des données par paquets.
            while ((n = read(filefd, buf, MAX_BLOCK_SIZE)) > 0)
            {
                ssize_t nwritten = write(connfd, buf, n);
                if( nwritten != n) {
                    fprintf(stderr, "Client déconnecté pendant le transfert\n");
                    break;

                }
            }

            if( n < 0){
                perror("read");
            }
            close(filefd);
        }
    }
}

/*
 * Note that this code only works with IPv4 addresponseses
 * (IPv6 is not supported)
 */
int main(int argc, char **argv)
{
    //Ignore le SIGPIPE
    Signal(SIGPIPE, SIG_IGN);

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

                handle_connection(connfd);
                Close(connfd);
            }
        } else {
            children[i] = pid;
        }
    }
    //Défini le signal après la création du pool pour modifier uniquement le handler du pere, les fils auront le traitant de SIGINT par défaut.
    Signal(SIGINT, sigint_handler);
    while (children_alive > 0)
    {
        pause();
    }

    return 0;
}