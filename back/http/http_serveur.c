/* ============================================================
   http_serveur.c – Le « guichet » HTTP du serveur R-Domotik

   VERSION 1 (étape de test) : on affiche la requête brute reçue
   et on répond toujours {"ok":true}. Le découpage de la requête
   (http_requete.c) et l'aiguillage (route) viendront ensuite.
   ============================================================ */

#include <stdio.h>          /* printf, fprintf, perror        */
#include <string.h>         /* strlen                         */
#include <unistd.h>         /* close                          */
#include <signal.h>         /* signal, SIGPIPE                */
#include <sys/socket.h>     /* socket, bind, listen, accept   */
#include <sys/time.h>       /* struct timeval (délai)         */
#include <netinet/in.h>     /* struct sockaddr_in, htons      */
#include <arpa/inet.h>      /* inet_ntop                      */

#include "http_serveur.h"

#define PREFIXE "[http] "

/* Nombre de clients qui peuvent attendre en file pendant qu'on
   en sert un (comme la file d'attente devant le guichet). */
#define FILE_ATTENTE 16

/* Taille maximale d'une requête lue (version 1) */
#define TAILLE_TAMPON 4096

/* Au-delà de 5 s sans rien recevoir, on abandonne ce client
   (sinon un client muet bloquerait tout le serveur). */
#define DELAI_LECTURE_S 5


int http_ouvrir(int port)
{
    int ecoute;
    int oui = 1;
    struct sockaddr_in adresse;

    /* Si le client ferme sa connexion pendant qu'on lui écrit, le système
       envoie le signal SIGPIPE, qui tue le programme par défaut.
       On l'ignore : l'écriture renverra simplement une erreur. */
    signal(SIGPIPE, SIG_IGN);

    /* 1. Créer une prise réseau (socket) TCP/IPv4.
          Comme fopen, on reçoit un numéro qui la représente. */
    ecoute = socket(AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0) {
        perror(PREFIXE "socket");
        return -1;
    }

    /* Permet de relancer le serveur tout de suite après l'avoir arrêté
       (sinon : « Address already in use » pendant ~1 minute). */
    setsockopt(ecoute, SOL_SOCKET, SO_REUSEADDR, &oui, sizeof oui);

    /* 2. Attacher la prise à une adresse et un port (bind).
          127.0.0.1 = seulement depuis la VM elle-même : seul nginx
          peut nous joindre, personne ne peut contourner nginx. */
    memset(&adresse, 0, sizeof adresse);
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons((unsigned short)port);    /* htons : ordre des octets réseau */
    adresse.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* 127.0.0.1 */

    if (bind(ecoute, (struct sockaddr *)&adresse, sizeof adresse) < 0) {
        fprintf(stderr, PREFIXE "impossible d'utiliser le port %d : ", port);
        perror(NULL);       /* ex. Address already in use : serveur déjà lancé ? */
        close(ecoute);
        return -1;
    }

    /* 3. Ouvrir le guichet : se mettre en écoute (listen). */
    if (listen(ecoute, FILE_ATTENTE) < 0) {
        perror(PREFIXE "listen");
        close(ecoute);
        return -1;
    }

    printf(PREFIXE "en écoute sur 127.0.0.1:%d\n", port);
    return ecoute;
}


/* Sert UN client : lire sa requête, l'afficher, répondre, fermer. */
static void servir_client(int client)
{
    char tampon[TAILLE_TAMPON];
    ssize_t lus;
    struct timeval delai = { DELAI_LECTURE_S, 0 };

    /* Réponse fixe pour la version 1. Les \r\n sont obligatoires en HTTP,
       et la ligne vide (\r\n\r\n) sépare les en-têtes du corps. */
    const char *reponse =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 11\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true}";

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof delai);

    /* Lire ce que le client a envoyé (une seule lecture en version 1).
       -1 pour garder une place pour le '\0' final. */
    lus = recv(client, tampon, sizeof tampon - 1, 0);
    if (lus <= 0) {
        fprintf(stderr, PREFIXE "rien reçu (client parti ou délai dépassé)\n");
        return;
    }
    tampon[lus] = '\0';

    printf(PREFIXE "----- requête reçue (%zd octets) -----\n%s\n"
           PREFIXE "--------------------------------------\n", lus, tampon);

    /* Envoyer la réponse */
    if (send(client, reponse, strlen(reponse), 0) < 0) {
        perror(PREFIXE "send");
    }
}


void http_boucle(int ecoute)
{
    int client;
    struct sockaddr_in adresse_client;
    socklen_t taille;
    char ip[INET_ADDRSTRLEN];

    for (;;) {                                  /* boucle sans fin */
        /* 4. Attendre un client (accept). Le programme reste BLOQUÉ ici
              tant que personne ne se connecte : c'est ce qui le garde allumé.
              accept renvoie une NOUVELLE prise, réservée à ce client. */
        taille = sizeof adresse_client;
        client = accept(ecoute, (struct sockaddr *)&adresse_client, &taille);
        if (client < 0) {
            perror(PREFIXE "accept");
            continue;                           /* on attend le suivant */
        }

        inet_ntop(AF_INET, &adresse_client.sin_addr, ip, sizeof ip);
        printf(PREFIXE "client connecté depuis %s\n", ip);

        servir_client(client);

        /* 5. Fermer la connexion de ce client, puis recommencer. */
        close(client);
        fflush(stdout);
    }
}