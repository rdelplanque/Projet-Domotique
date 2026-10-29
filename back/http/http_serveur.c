/* ============================================================
   http_serveur.c – Le « guichet » HTTP du serveur R-Domotik

   Pour chaque client :
     http_requete.c  lit et découpe la requête   (fiche Requete)
     route.c         choisit qui répond          (fiche Reponse)
     http_reponse.c  envoie la réponse
   ============================================================ */

#include <stdio.h>          /* printf, fprintf, perror        */
#include <errno.h>          /* errno, EINTR                   */
#include <time.h>           /* clock_gettime                  */
#include <poll.h>           /* poll : attendre avec un délai  */
#include <string.h>         /* memset, strcmp                 */
#include <unistd.h>         /* close                          */
#include <signal.h>         /* signal, SIGPIPE                */
#include <sys/socket.h>     /* socket, bind, listen, accept   */
#include <sys/time.h>       /* struct timeval (délai)         */
#include <netinet/in.h>     /* struct sockaddr_in, htons      */
#include <arpa/inet.h>      /* htonl                          */

#include "http_serveur.h"
#include "http_requete.h"
#include "http_reponse.h"
#include "../route/route.h"

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


/* Sert UN client : lire sa requête, la faire traiter, répondre. */
static void servir_client(int client)
{
    Requete req;                        /* la fiche de la requête (~16 Ko)  */
    Reponse rep = { 0, NULL };          /* la fiche de la réponse, vide     */
    int resultat;
    struct timeval delai = { DELAI_LECTURE_S, 0 };

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof delai);

    /* 1. Lire et découper */
    resultat = http_requete_lire(client, &req);
    if (resultat == -1) {
        fprintf(stderr, PREFIXE "rien reçu (client parti ou délai dépassé)\n");
        return;                         /* on ferme sans répondre */
    }

    /* 2. Enveloppe fausse → http répond lui-même ; sinon → route */
    if (resultat == 400) {
        http_reponse_erreur(&rep, 400, "requête invalide");
    } else if (resultat == 413) {
        http_reponse_erreur(&rep, 413, "requête trop grosse");
    } else {
        route_traiter(&req, &rep);
    }

    /* Une ligne de journal par requête : « GET /api/ping → 200 »,
       SAUF la relecture régulière de la page Ma maison quand tout va bien
       (une ligne par seconde et par navigateur noierait le reste). */
    if (!(resultat == 0 && rep.json != NULL && rep.code == 200
          && strcmp(req.chemin, "/api/maison") == 0)) {
        printf(PREFIXE "%s %s -> %d\n",
               resultat == 0 ? req.methode : "?",
               resultat == 0 ? req.chemin : "(requête refusée)",
               rep.json != NULL ? rep.code : 500);
    }

    /* 3. Envoyer, puis libérer la réponse (le JSON était alloué) */
    http_reponse_envoyer(client, &rep);
    http_reponse_liberer(&rep);
}


/* Millisecondes écoulées depuis un instant de départ */
static long ecoule_ms(const struct timespec *debut)
{
    struct timespec maintenant;

    clock_gettime(CLOCK_MONOTONIC, &maintenant);
    return (maintenant.tv_sec - debut->tv_sec) * 1000
         + (maintenant.tv_nsec - debut->tv_nsec) / 1000000;
}

void http_boucle(int ecoute, int intervalle_s, TachePeriodique tache)
{
    int client;
    struct sockaddr_in adresse_client;
    socklen_t taille;
    struct pollfd attente;
    struct timespec derniere_ronde;
    long reste_ms;
    int n;

    attente.fd = ecoute;
    attente.events = POLLIN;                    /* « un client frappe au guichet » */
    clock_gettime(CLOCK_MONOTONIC, &derniere_ronde);

    for (;;) {                                  /* boucle sans fin */
        /* 4a. L'heure de la ronde est-elle passée ? Alors on la fait
               d'abord (même s'il y a du monde : elle n'est jamais oubliée). */
        reste_ms = (long)intervalle_s * 1000 - ecoule_ms(&derniere_ronde);
        if (tache != NULL && reste_ms <= 0) {
            tache();
            clock_gettime(CLOCK_MONOTONIC, &derniere_ronde);
            fflush(stdout);
            continue;
        }

        /* 4b. Attendre un client, mais PAS plus longtemps que l'heure de la
               prochaine ronde (-1 = sans limite, s'il n'y a pas de ronde).
               C'est poll qui attend maintenant, plus accept. */
        n = poll(&attente, 1, tache != NULL ? (int)reste_ms : -1);
        if (n < 0) {
            if (errno != EINTR) {
                perror(PREFIXE "poll");
            }
            continue;
        }
        if (n == 0) {
            continue;                           /* personne : c'est l'heure de la ronde */
        }

        /* 4c. Un client est là : accept ne bloque plus, il le fait entrer.
               accept renvoie une NOUVELLE prise, réservée à ce client. */
        taille = sizeof adresse_client;
        client = accept(ecoute, (struct sockaddr *)&adresse_client, &taille);
        if (client < 0) {
            perror(PREFIXE "accept");
            continue;                           /* on attend le suivant */
        }

        /* (le client est toujours nginx, 127.0.0.1 : inutile de l'afficher ;
           la vraie IP du navigateur est dans l'en-tête X-Real-IP) */
        servir_client(client);

        /* 5. Fermer la connexion de ce client, puis recommencer. */
        close(client);
        fflush(stdout);
    }
}
