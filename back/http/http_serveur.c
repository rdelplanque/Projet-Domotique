/* ============================================================
   http_serveur.c – Le « guichet » HTTP du serveur R-Domotik

   VERSION 2 : la requête est lue et découpée par http_requete.c.
   On affiche méthode, chemin et corps, et on répond toujours
   {"ok":true}. Réponses (http_reponse.c) et aiguillage (route)
   viendront ensuite.
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
#include "http_requete.h"

#define PREFIXE "[http] "

/* Nombre de clients qui peuvent attendre en file pendant qu'on
   en sert un (comme la file d'attente devant le guichet). */
#define FILE_ATTENTE 16

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


/* PROVISOIRE : envoie une réponse HTTP minimale (code + JSON).
   Sera remplacé par http_reponse.c à l'étape suivante.
   Les \r\n sont obligatoires en HTTP, et la ligne vide (\r\n\r\n)
   sépare les en-têtes du corps. */
static void repondre(int client, int code, const char *message, const char *json)
{
    char entetes[256];
    int n;

    n = snprintf(entetes, sizeof entetes,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 code, message, strlen(json));

    if (send(client, entetes, (size_t)n, 0) < 0 || send(client, json, strlen(json), 0) < 0) {
        perror(PREFIXE "send");
    }
}

/* Sert UN client : lire et découper sa requête, répondre. */
static void servir_client(int client)
{
    Requete req;            /* la fiche de la requête (~16 Ko) */
    int resultat;
    struct timeval delai = { DELAI_LECTURE_S, 0 };

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof delai);

    resultat = http_requete_lire(client, &req);

    if (resultat == -1) {
        fprintf(stderr, PREFIXE "rien reçu (client parti ou délai dépassé)\n");
        return;                                     /* on ferme sans répondre */
    }
    if (resultat == 400) {
        repondre(client, 400, "Bad Request", "{\"erreur\":\"requete invalide\"}");
        return;
    }
    if (resultat == 413) {
        repondre(client, 413, "Payload Too Large", "{\"erreur\":\"requete trop grosse\"}");
        return;
    }

    /* Requête correcte : on affiche ce qu'on a découpé.
       %.*s : affiche au plus taille_corps caractères. */
    printf(PREFIXE "%s %s (corps : %zu octets)\n",
           req.methode, req.chemin, req.taille_corps);
    if (req.corps != NULL) {
        printf(PREFIXE "corps : %.*s\n", (int)req.taille_corps, req.corps);
    }

    /* Toujours la même réponse : l'aiguillage (route) viendra ensuite */
    repondre(client, 200, "OK", "{\"ok\":true}");
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