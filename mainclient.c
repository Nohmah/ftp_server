#include "csapp.h"
#include "ftp.h"
#define MAX_NAME_LEN 256
#define CLIENT_DIR "./clientdir/"
#define PORT 2121

int main(int argc, char **argv)
{
    int clientfd;
    char *host;
    char filename[MAX_NAME_LEN];
    char filepath[MAX_NAME_LEN + sizeof(CLIENT_DIR)];
    char saisie[MAXLINE];
    request_t req;
    response_t response;
    struct timeval debut, fin;
    
    char buf[MAXLINE];
    rio_t rio;
    int filefd;
    ssize_t n;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    host = argv[1];

    clientfd = Open_clientfd(host, PORT);
    printf("Connected to %s\n", host);
    while (1){
        printf("ftp> ");
        
        //Lecture de la saisie
        if (fgets(saisie, MAXLINE, stdin)==NULL){
            continue;
        }
        saisie[strcspn(saisie, "\n")] = 0;
        if (strlen(saisie) == 0){
            continue;
        }
        if (strncmp(saisie, "get ", 4) == 0){
            gettimeofday(&debut, NULL);
            strncpy(filename, saisie + 4, MAX_NAME_LEN - 1);//+4 pour commencer à lire apres le get
            
            //Vérifie si il y a un nom de fichier après get (à faire dans le serveur)
            if (strlen(filename) == 0)
            {
                fprintf(stderr, "Erreur: aucun nom de fichier spécifié\n");
                continue;
            }

            //Construit la requete a envoyer au serveur.
            req.type = GET;
            strncpy(req.name, filename, sizeof(req.name) - 1);
            Rio_writen(clientfd, &req, sizeof(req));

            Rio_readn(clientfd, &response, sizeof(response));

            if (response.code_retour == ERREUR)
            {
                fprintf(stderr, "Erreur:%s\n", response.message);
                continue;
            }

            //Ouvre le fichier de destination
            snprintf(filepath, sizeof(filepath), "%s%s", CLIENT_DIR, filename);
            filefd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (filefd < 0)
            {
                perror("open client file");
                exit(1);
            }
            double taille = 0;
            //Reception et écriture des données.
            Rio_readinitb(&rio, clientfd);
            while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0)
            {
                if (write(filefd, buf, n) != n)
                {
                    perror("write");
                    break;
                }
                taille+=n;
            }

            gettimeofday(&fin, NULL);
            printf("Transfer successfully complete.\n");
            //Récupère les statistiques du transfert
            int temps_sec = fin.tv_sec - debut.tv_sec;
            int temps_microsec = fin.tv_usec - debut.tv_usec;
            printf("%f bytes received in %d.%d seconds (%f Kbytes)", taille, temps_sec, temps_microsec, (taille*1e-3));
            //printf("Fichier '%s' reçu dans %s\n", filename, filepath);
            close(filefd);
            break;
        } else {
            // La requete n'est pas supportée
            fprintf(stderr, "Erreur: Requête non supportée par le serveur");
        }
    }
    Close(clientfd);
    return 0;
}