#include "csapp.h"
#include "ftp.h"
#include <utime.h>
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
    transfer_t transfer;
    char buf[MAX_BLOCK_SIZE];
    struct timeval debut, fin;
    int filefd;
    ssize_t n;
    uint64_t bytes_left;
    uint32_t i;
    size_t to_read;
    int transfer_ok;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    host = argv[1];

    clientfd = Open_clientfd(host, PORT);
    printf("Connecté au serveur maître%s\n", host);

    // S'identifie comme client au serveur maître
    identification_t id;
    id.type = CLIENT;
    Rio_writen(clientfd, &id, sizeof(id));

    // Récupère les informations sur l'esclave choisi par le serveur maître
    slave_info_t redirection_info;
    n = Rio_readn(clientfd, &redirection_info, sizeof(redirection_info));
    if (n != sizeof(redirection_info)) {
        fprintf(stderr, "Erreur: aucun serveur disponible pour se connecter\n");
        Close(clientfd);
        exit(1);
    }
    printf("Redirigé vers le serveur esclave %s:%d\n", redirection_info.host, redirection_info.port);
    Close(clientfd);

    // Se connecte au serveur esclave
    clientfd = Open_clientfd(redirection_info.host, redirection_info.port);
    printf("Connecté\n");

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

            //Précise l'offset du fichier pour la reprise du telechargement
            struct stat st;
            uint64_t offset = 0;
            //Variable qui stockera le mtime du fichier coté client.
            time_t local_mtime = 0;

            snprintf(filepath, sizeof(filepath), "%s%s", CLIENT_DIR, filename);
            

            if(stat(filepath, &st) == 0){
                offset = st.st_size;
                //Récupère le timestamp pour la requete.
                local_mtime = st.st_mtime;
            }

            req.offset = offset;
            //Envoie le mtime du fichier du client au serveur.
            req.mtime = local_mtime;

            Rio_writen(clientfd, &req, sizeof(req));

            Rio_readn(clientfd, &response, sizeof(response));

            if (response.code_retour == ERREUR)
            {
                fprintf(stderr, "Erreur:%s\n", response.message);
                continue;
            }
            if (Rio_readn(clientfd, &transfer, sizeof(transfer)) != sizeof(transfer))
            {
                fprintf(stderr, "Erreur: entete de transfert incomplete\n");
                break;
            }
            if (transfer.block_size == 0 || transfer.block_size > MAX_BLOCK_SIZE)
            {
                fprintf(stderr, "Erreur: taille de bloc non supportee (%u)\n", transfer.block_size);
                break;
            }

            // Vérifier l'existence du dossier clientdir avant d'ouvrir le fichier de destination
            // Et le crée si besoin
            struct stat dir;
            if (stat(CLIENT_DIR, &dir) == -1) {
                if (mkdir(CLIENT_DIR, 0755) == -1) {
                    perror("mkdir");
                    continue;
                }
            }

            //Ouvre le fichier de destination
            snprintf(filepath, sizeof(filepath), "%s%s", CLIENT_DIR, filename);
            int open_flags = O_WRONLY | O_CREAT;
            //Si l'offset est à 0, alors on réécrit tout le fichier.
            if (transfer.start_offset == 0)
            {
                open_flags |= O_TRUNC;
            }
            filefd = open(filepath, open_flags, 0644);
            if (filefd < 0)
            {
                perror("open client file");
                exit(1);
            }
            // Avance la tete jusqu'a l'offset effectivement valide par le serveur.
            if (lseek(filefd, transfer.start_offset, SEEK_SET) < 0)
            {
                perror("lseek");
                close(filefd);
                continue;
            }
            //Reception et ecriture des donnees bloc par bloc.
            bytes_left = transfer.file_size;
            transfer_ok = 1;
            double taille = 0;
            gettimeofday(&debut, NULL);
            for (i = 0; i < transfer.block_count; i++)
            {
                to_read = (bytes_left >= transfer.block_size) ? transfer.block_size : (size_t)bytes_left;

                n = Rio_readn(clientfd, buf, to_read);
                if (n != (ssize_t)to_read)
                {
                    fprintf(stderr, "Erreur: bloc %u incomplet recu\n", i + 1);
                    transfer_ok = 0;
                    break;
                }

                if (write(filefd, buf, n) != n)
                {
                    perror("write");
                    transfer_ok = 0;
                    break;
                }
                taille +=n;

                bytes_left -= (uint64_t)n;
            }

            if (transfer_ok && bytes_left == 0)
            {
                printf("Fichier '%s' recu dans %s (%u blocs de %u octets max)\n",
                       filename, filepath, transfer.block_count, transfer.block_size);
            }
            else
            {
                fprintf(stderr, "Erreur: transfert incomplet pour '%s'\n", filename);
            }   
            gettimeofday(&fin, NULL);
            printf("Transfer successfully complete.\n");

            // Conserve le mtime serveur pour valider les futures reprises.
            if (transfer_ok && bytes_left == 0)
            {
                struct utimbuf tb;
                tb.actime = transfer.mtime;
                tb.modtime = transfer.mtime;
                if (utime(filepath, &tb) < 0)
                {
                    perror("utime");
                }
            }

            //Récupère les statistiques du transfert
            double temps = (fin.tv_sec - debut.tv_sec) + (fin.tv_usec - debut.tv_usec)/1e6;
            printf("%f bytes received in %f seconds (%f Kbytes/s)\n", taille, temps, (taille*1e-3)/temps);
            //printf("Fichier '%s' reçu dans %s\n", filename, filepath);
            close(filefd);
        } else if (strcmp(saisie, "bye")==0){
            break;
        } else {
            // La requete n'est pas supportée
            fprintf(stderr, "Erreur: Requête non supportée par le serveur\n");
        }
    }
    Close(clientfd);
    return 0;
}