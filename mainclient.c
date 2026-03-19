#include "csapp.h"
#include "ftp.h"
#define MAX_NAME_LEN 256
#define CLIENT_DIR "./clientdir/"

int main(int argc, char **argv)
{
    int clientfd;
    char *host;
    int port;
    char filename[MAX_NAME_LEN];
    char filepath[MAX_NAME_LEN + sizeof(CLIENT_DIR)];
    request_t req;
    char buf[MAXLINE];
    rio_t rio;
    int filefd;
    ssize_t n;

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <host> <port> <filename>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = atoi(argv[2]);
    strncpy(filename, argv[3], MAX_NAME_LEN - 1);

    clientfd = Open_clientfd(host, port);

    //Construit la requete a envoyer au serveur.
    req.type = GET;
    strncpy(req.name, filename, sizeof(req.name) - 1);
    Rio_writen(clientfd, &req, sizeof(req));

    //Ouvre le fichier de destination
    snprintf(filepath, sizeof(filepath), "%s%s", CLIENT_DIR, filename);
    filefd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (filefd < 0)
    {
        perror("open client file");
        exit(1);
    }

    //Reception et écriture des données.
    Rio_readinitb(&rio, clientfd);
    while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0)
    {
        if (write(filefd, buf, n) != n)
        {
            perror("write");
            break;
        }
    }

    printf("Fichier '%s' reçu dans %s\n", filename, filepath);

    close(filefd);
    Close(clientfd);
    return 0;
}