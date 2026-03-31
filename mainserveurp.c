/*
 * mainserveurp.c - Un serveur avec pool de processus
 */

#include "csapp.h"
#include "ftp.h"
#include "time.h"

#define MAX_NAME_LEN 256
#define N_PROC 5
#define NB_SLAVES 3
#define BASE_PORT 2121
#define BASE_PORT_SLAVE 2122

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
    for(int i = 0; i < NB_SLAVES; i++){
        if(children[i] > 0){
            kill(children[i], SIGINT);
        }
    }
    exit(0);
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
            printf("Client déconnecte\n");
            break;
        }
        //La requête est illisible
        else if (n != sizeof(req)) {
            fprintf(stderr, "Erreur lecture requête\n");
            break;
        }
        if (req.type == GET){

            uint64_t offset = req.offset;
            time_t client_mtime = req.mtime;
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

            // Reprise autorisee uniquement si le client a la meme version du fichier.
            if (offset > 0 && client_mtime != sf.st_mtime)
            {
                offset = 0;
            }

            //Sécurité si la valeur de offset est > que la taille du fichier
            if(offset > sf.st_size){
                offset = 0;
            }
            

            lseek(filefd, offset, SEEK_SET);
            transfer.block_size = MAX_BLOCK_SIZE;
            transfer.file_size =(uint64_t)sf.st_size - offset;
            transfer.block_count = (uint32_t)((transfer.file_size + MAX_BLOCK_SIZE -1) / MAX_BLOCK_SIZE);
            transfer.start_offset = offset;
            transfer.mtime = sf.st_mtime;
            Rio_writen(connfd, &transfer, sizeof(transfer));

            //Lecture et envoi des données par paquets.
            while ((n = read(filefd, buf, MAX_BLOCK_SIZE)) > 0)
            {
                if( rio_writen(connfd, buf, n) < 0) {
                    perror("rio_written");
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

void start_slave(int slave_id) {
    int listenfd, connfd, slavefd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];

    listenfd = Open_listenfd(BASE_PORT_SLAVE + slave_id);
    printf("Serveur esclave %d: écoute sur le port %d\n", slave_id, BASE_PORT_SLAVE + slave_id);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd < 0) {
            if (errno == EINTR) continue;
            perror("accept esclave");
            continue;
        }

        Getnameinfo((SA *)&clientaddr, clientlen,
                    client_hostname, MAX_NAME_LEN, 0, 0, 0);
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string,
                  INET_ADDRSTRLEN);
        printf("Esclave %d connecté à %s (%s)\n", slave_id,
               client_hostname, client_ip_string);

        handle_connection(connfd);
        Close(connfd);
        
        //Le client s'est déconnecté de l'esclave
        //L'esclave informe le maitre qu'il est libre
        slavefd = Open_clientfd("127.0.0.1", BASE_PORT);
        identification_t id;
        id.type = SLAVE;
        id.slave_id = slave_id;
        Rio_writen(slavefd, &id, sizeof(id));
        Close(slavefd);
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
    int current_slave = 0;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    slave_info_t slaves[NB_SLAVES];
    int slaves_busy[NB_SLAVES];
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
    pid_t pid;

    if (argc != 1)
    {
        fprintf(stderr, "usage: %s\n", argv[0]);
        exit(0);
    }

    Signal(SIGCHLD, sigchld_handler);
    port = BASE_PORT;
    listenfd = Open_listenfd(port);

    // Lancement des serveurs esclaves
    for (int i = 0; i < NB_SLAVES; i++) {
        pid = Fork();
        if (pid == 0) {
            start_slave(i);
        } else {
            slaves_busy[i] = 0;
            children[i] = pid;
            slaves[i].port = BASE_PORT_SLAVE + i;
            strcpy(slaves[i].host, "127.0.0.1");
            printf("Serveur maître: esclave %d lancé sur %s:%d (pid %d)\n", i, slaves[i].host, slaves[i].port, pid);
        }
    }
    //Le maitre tue ses esclaves si il est tué
    Signal(SIGINT, sigint_handler);
    // Creation du pool
    //for (int i = 0; i < N_PROC; i++)
    //{
    //    if ((pid = Fork()) == 0)
    //    {
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
                
                // Détermine qui communique avec lui
                identification_t id;
                ssize_t n = Rio_readn(connfd, &id, sizeof(id));
                if (n != sizeof(id)) {
                    printf("Serveur maître: Erreur lecture identification\n");
                    Close(connfd);
                    continue;
                }

                if (id.type == SLAVE) {
                    slaves_busy[id.slave_id] = 0;
                    printf("Serveur maître: esclave %d libre\n", id.slave_id);
                    Close(connfd);
                    continue;
                }

                // C'est un client qui s'est connecté
                printf("server connected to %s (%s)\n", client_hostname,
                    client_ip_string);
                // Le serveur maître cherche quel esclave va répondre à la requête du client
                // avec une politique Round-Robin
                int chosen_slave = -1;
                for (int i = 0; i<NB_SLAVES; i++) {
                    int j = (current_slave + i) % NB_SLAVES;
                    if (slaves_busy[j] == 0) {
                        slaves_busy[j] = 1;
                        chosen_slave = j;
                        break;
                    }
                }
                if (chosen_slave == -1) {
                    printf("Serveur maître: tous les esclaves sont occupés, client rejeté\n");
                    Close(connfd);
                    continue;
                }
                slave_info_t redirection_info;
                redirection_info = slaves[chosen_slave];
                // Envoi au client des infos du slave choisi
                Rio_writen(connfd, &redirection_info, sizeof(redirection_info));
                printf("Serveur maître: client redirigé vers l'esclave n°%d (%s:%d)\n", chosen_slave, redirection_info.host, redirection_info.port);
                // La prochaine connexion utilisera l'esclave suivant (Round-Robin)'
                current_slave = (chosen_slave + 1) % NB_SLAVES;
                //handle_connection(connfd);
                Close(connfd);
            }
        //} else {
        //    children[i] = pid;
        //}
    //}
    //Défini le signal après la création du pool pour modifier uniquement le handler du pere, les fils auront le traitant de SIGINT par défaut.
    //Signal(SIGINT, sigint_handler);
    //while (children_alive > 0)
    //{
    //    pause();
    //}

    return 0;
}