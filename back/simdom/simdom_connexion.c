/* ============================================================
   simdom_connexion.c – La connexion TCP avec SimDom
   Ouvrir, fermer, envoyer, vider ce qui attend.
   ============================================================ */

#include <stdio.h>          /* printf, fprintf, perror */
#include <string.h>         /* memset                  */
#include <errno.h>          /* errno, EAGAIN           */
#include <unistd.h>         /* close                   */
#include <sys/socket.h>     /* socket, connect, send, recv */
#include <sys/time.h>       /* struct timeval          */
#include <netinet/in.h>     /* struct sockaddr_in      */
#include <netinet/tcp.h>    /* TCP_NODELAY, TCP_QUICKACK */
#include <arpa/inet.h>      /* inet_pton, htons        */

#include "simdom.h"
#include "simdom_interne.h"

/* Ce que le module retient (une seule connexion pour tout le serveur) */
static char ip_simdom[16];
static int  port_simdom = 0;
static int  echec_signale = 0;  /* 1 = « connexion impossible » déjà affiché */
int delai = 2000;               /* attente maximale d'une réponse, en ms */
int prise = -1;                 /* la connexion ; -1 = pas connecté */


/* ------------------------------------------------------------
   Outils partagés avec simdom.c et simdom_rafale.c
   ------------------------------------------------------------ */

/* Ferme la connexion après un problème : le prochain appel retentera. */
void couper(const char *raison)
{
    if (prise >= 0) {
        fprintf(stderr, PREFIXE "connexion perdue (%s)\n", raison);
        close(prise);
        prise = -1;
    }
}

/* Accusé de réception IMMÉDIAT de ce qu'on vient de recevoir.
   Sans ça, Linux attend jusqu'à 40 ms avant d'accuser réception ; si SimDom
   envoie sa réponse en plusieurs morceaux, il attend cet accusé avant
   d'envoyer la suite : 40 ms perdues par lecture, soit ~9 s par ronde
   de 211 équipements. Linux remet ce réglage à zéro tout seul : il faut
   le redemander après chaque recv. (Option propre à Linux, d'où le #ifdef.) */
void accuser_reception(void)
{
#ifdef TCP_QUICKACK
    int oui = 1;
    setsockopt(prise, IPPROTO_TCP, TCP_QUICKACK, &oui, sizeof oui);
#endif
}

/* Envoie tout le message. MSG_NOSIGNAL : si SimDom a fermé la connexion,
   send renvoie une erreur au lieu de TUER le serveur (signal SIGPIPE). */
int envoyer(const unsigned char *message, size_t taille)
{
    size_t envoyes = 0;
    long n;

    while (envoyes < taille) {
        n = (long)send(prise, message + envoyes, taille - envoyes, MSG_NOSIGNAL);
        if (n <= 0) {
            couper("envoi impossible");
            return -1;
        }
        envoyes += (size_t)n;
    }
    return 0;
}

/* Jette les octets en attente : réponses arrivées APRÈS un délai dépassé.
   Sans ça, on risquerait de lire la réponse d'un autre équipement.
   Renvoie 0, ou -1 si SimDom a fermé la connexion. */
int vider(void)
{
    unsigned char poubelle[64];
    long n;

    for (;;) {
        n = (long)recv(prise, poubelle, sizeof poubelle, MSG_DONTWAIT);
        if (n > 0) {
            continue;                           /* jeté, on regarde s'il y en a d'autres */
        }
        if (n == 0) {
            couper("fermée par SimDom");        /* 0 octet = l'autre côté a fermé */
            return -1;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;                           /* plus rien en attente */
        }
        couper("erreur de lecture");
        return -1;
    }
}

/* Pas connecté ? On tente une fois (ex. connexion coupée puis SimDom revenu). */
int verifier_connexion(void)
{
    if (prise >= 0) {
        return 0;
    }
    return simdom_connecter();
}


/* ------------------------------------------------------------
   Fonctions publiques
   ------------------------------------------------------------ */

void simdom_initialiser(const char *ip, int port, int delai_ms)
{
    snprintf(ip_simdom, sizeof ip_simdom, "%s", ip);
    port_simdom = port;
    delai = delai_ms;
}

int simdom_connecter(void)
{
    struct sockaddr_in adresse;
    struct timeval limite;
    int oui = 1;

    if (prise >= 0) {
        return 0;                               /* déjà connecté */
    }

    memset(&adresse, 0, sizeof adresse);
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons((unsigned short)port_simdom);
    if (inet_pton(AF_INET, ip_simdom, &adresse.sin_addr) != 1) {
        fprintf(stderr, PREFIXE "adresse invalide : %s\n", ip_simdom);
        return -1;
    }

    prise = socket(AF_INET, SOCK_STREAM, 0);
    if (prise < 0) {
        perror(PREFIXE "socket");
        return -1;
    }

    /* Sans limite, connect peut bloquer longtemps si le PC ne répond pas.
       Sous Linux, SO_SNDTIMEO limite aussi la durée de connect. */
    limite.tv_sec = delai / 1000;
    limite.tv_usec = (delai % 1000) * 1000;
    setsockopt(prise, SOL_SOCKET, SO_SNDTIMEO, &limite, sizeof limite);

    if (connect(prise, (struct sockaddr *)&adresse, sizeof adresse) != 0) {
        /* Message une seule fois : la ronde retente à chaque relecture */
        if (!echec_signale) {
            fprintf(stderr, PREFIXE "connexion impossible à %s:%d (nouvel essai à chaque relecture)\n",
                    ip_simdom, port_simdom);
            echec_signale = 1;
        }
        close(prise);
        prise = -1;
        return -1;
    }

    /* TCP_NODELAY : envoyer chaque petit message TOUT DE SUITE.
       Par défaut (algorithme de Nagle), TCP retient un petit message tant que
       le précédent n'a pas été accusé : après une commande (sans réponse),
       la relecture pouvait attendre jusqu'à 200 ms l'accusé de Windows. */
    setsockopt(prise, IPPROTO_TCP, TCP_NODELAY, &oui, sizeof oui);

    printf(PREFIXE "connecté à %s:%d\n", ip_simdom, port_simdom);
    echec_signale = 0;
    return 0;
}

int simdom_est_connecte(void)
{
    return prise >= 0;
}

void simdom_fermer(void)
{
    if (prise >= 0) {
        close(prise);
        prise = -1;
    }
}
