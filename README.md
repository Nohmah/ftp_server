# FTP simple en C

Petit projet de protocole FTP en C réalisé avec des sockets POSIX et un modèle serveur maître / serveurs esclaves.

Le but est de permettre à un client de se connecter à un serveur, de récupérer des fichiers présents dans le répertoire `servdir/`, et de télécharger des fichiers de manière fiable, avec gestion des offsets pour reprendre un téléchargement.

## Fonctionnalités

- Serveur principal qui accepte les connexions clientes
- Répartition des clients entre plusieurs serveurs esclaves sur des ports distincts
- Commande `GET` pour télécharger un fichier depuis le serveur
- Gestion du `offset` pour reprendre un téléchargement à partir d’un point donné
- Transfert de fichiers par blocs
- Réponse serveur structurée avec codes de retour et messages
- Conception simple, compatible avec un environnement Unix/Linux

## Architecture du projet

Le projet est composé de :

- `mainserveurp.c` : serveur maître, responsable des connexions entrantes et de la répartition sur les esclaves
- `mainclient.c` : client FTP en ligne de commande
- `ftp.h` : structures partagées pour les requêtes, réponses et transfert
- `csapp.c` et `csapp.h` : utilitaires réseau et wrappers POSIX
- `echo.c` : implémentation utile du projet
- `servdir/` : fichiers disponibles côté serveur
- `Makefile` : compilation automatique du projet

## Exemple de fonctionnement

Le serveur écoute sur le port 2121, puis redirige le client vers un serveur esclave (2122, 2123, etc.) pour le traitement du transfert.

Le client se connecte ensuite à ce serveur esclave pour envoyer une requête de type `GET` :

- nom du fichier
- offset courant pour reprise de téléchargement

Le serveur répond avec :

- un code de retour (`SUCCES` / `ERREUR`)
- des informations sur le transfert (`taille du fichier`, `taille des blocs`, `nombre de blocs`)
- puis les données du fichier envoyées bloc par bloc

## Prérequis

- GCC
- Make
- Environnement Unix/Linux ou macOS
- Bibliothèques POSIX standards (`fork`, `socket`, `pthread`, `signal`, etc.)

Note : ce projet utilise des primitives POSIX et n’est pas prévu pour un environnement Windows natif sans adaptation.

## Installation et lancement

### 1. Cloner le projet

```bash
git clone https://github.com/<votre-utilisateur>/<votre-repo>.git
cd <votre-repo>
```

### 2. Compiler le projet

```bash
make
```

Cela va générer :

- `mainserveurp`
- `mainclient`

### 3. Préparer les répertoires

Le serveur sert les fichiers dans `./servdir/`.

Le client télécharge dans `./clientdir/`.

Créez donc ce répertoire si nécessaire :

```bash
mkdir -p clientdir
```

### 4. Lancer le serveur

Dans un terminal :

```bash
./mainserveurp
```

Le serveur démarre et attend les connexions. Il initialise ensuite les serveurs esclaves sur les ports de redirection.

### 5. Lancer le client

Dans un autre terminal :

```bash
./mainclient 127.0.0.1
```

### 6. Utiliser le client

Une fois connecté, vous pouvez utiliser la commande :

```text
ftp> get test.txt
```

Le fichier `test.txt` sera téléchargé dans `./clientdir/`.

Pour quitter le client :

```text
ftp> bye
```

## Structure des fichiers fournis

```text
.
├── csapp.c
├── csapp.h
├── echo.c
├── ftp.h
├── mainclient.c
├── mainserveurp.c
├── Makefile
├── README.md
├── servdir/
│   └── test.txt
└── clientdir/   # créé manuellement si nécessaire
```

## Exemple de fichier servi

Le répertoire `servdir/` contient des fichiers disponibles pour les téléchargements. Par défaut, le projet fournit :

- `servdir/test.txt`

## Remarques importantes

- Le serveur attend que les fichiers soient présents dans `./servdir/`.
- Le client n’ouvre pas automatiquement le dossier `clientdir/`, il faut donc le créer avant les téléchargements.
- Le programme a été conçu dans un cadre pédagogique pour illustrer les sockets TCP, les signaux, la redirection client/serveur et les transferts de fichiers.

## Licence

Aucune licence explicite n’est définie dans le dépôt pour le moment. Pensez à ajouter une licence si vous souhaitez publier le projet publiquement sur GitHub.

## Projets futurs possibles

- ajout de commandes `ls`, `put`, `cd`
- gestion de plusieurs fichiers depuis un serveur plus complet
- sécurisation des transferts et validation des erreurs
- interface graphique ou version plus robuste

## Auteur

Projet de cours / projet universitaire, adapté à un usage GitHub et à la démonstration de concepts réseau en C.
