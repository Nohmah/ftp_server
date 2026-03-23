/*
 * mainserveurp.c - Un serveur avec pool de processus
 */

#include "csapp.h"
#include "ftp.h"

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

void handle_request(int connfd)
{
    request_t req;
    response_t res;
    char filepath[MAX_NAME_LEN + sizeof(SERV_DIR)];
    char buf[MAXLINE];
    int filefd;
    //int size_t;
    ssize_t n;
    struct stat sf;

    //Lit la requête du client
    if (Rio_readn(connfd, &req, sizeof(req)) != sizeof(req))
    {
        fprintf(stderr, "Erreur lecture requête\n");
        return;
    }

    if (req.type != GET)
    {
        res.code_retour = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        fprintf(stderr, "Requête non supportée\n"); // répétitif avec le client
        return;
    }

    //Construit le chemin du fichier
    snprintf(filepath, sizeof(filepath), "%s%s", SERV_DIR, req.name);
    printf("GET %s\n", filepath);

    filefd = open(filepath, O_RDONLY);
    if (filefd < 0)
    {   
        res.code_retour = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        fprintf(stderr, "Fichier introuvable : %s\n", filepath);
        return;
    }
    res.code_retour = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));
    //Récupère la taille du fichier pour lecture en 1 fois.
    fstat(filefd, &sf);
    off_t size = sf.st_size;

    //Lecture et envoi des données.
    while ((n = read(filefd, buf, size)) > 0)
    {
        Rio_writen(connfd, buf, n);
    }

    close(filefd);
}

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

                handle_request(connfd);
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