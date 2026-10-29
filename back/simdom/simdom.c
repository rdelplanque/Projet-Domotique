/* ============================================================
   simdom.c – Dialogue avec le simulateur SimDom
   Ne connaît rien de la base ni du web : il fabrique des messages
   binaires, les envoie, et lit les réponses.
   ============================================================ */

#include <stdio.h>          /* printf, fprintf, perror */
#include <string.h>         /* memcmp, strlen          */
#include <errno.h>          /* errno, EINTR, EAGAIN    */
#include <time.h>           /* clock_gettime           */
#include <unistd.h>         /* close                   */
#include <poll.h>           /* poll : attendre avec un délai */
#include <sys/socket.h>     /* socket, connect, send, recv */
#include <sys/time.h>       /* struct timeval          */
#include <netinet/in.h>     /* struct sockaddr_in      */
#include <netinet/tcp.h>    /* TCP_NODELAY, TCP_QUICKACK */
#include <arpa/inet.h>      /* inet_pton, htons        */

#include "simdom.h"

#define PREFIXE "[simdom] "

#define TAILLE_MESSAGE   13     /* luminaires, prises, volets */
#define TAILLE_CLIM      14     /* climatisations : + 1 octet de mode */
#define TAILLE_ADRESSE   12     /* IP (4) + entrée (8) : identifie l'équipement */

#define CODE_ETEINDRE    0x00
#define CODE_ALLUMER     0x01
#define CODE_LIRE        0x04

/* Ce que le module retient (une seule connexion pour tout le serveur) */
static char ip_simdom[16];
static int  port_simdom = 0;
static int  delai = 2000;       /* attente maximale d'une réponse, en ms */
static int  prise = -1;         /* la connexion ; -1 = pas connecté */
static int  echec_signale = 0;  /* 1 = « connexion impossible » déjà affiché */


/* ------------------------------------------------------------
   Outils internes
   ------------------------------------------------------------ */

/* Ferme la connexion après un problème : le prochain appel retentera. */
static void couper(const char *raison)
{
    if (prise >= 0) {
        fprintf(stderr, PREFIXE "connexion perdue (%s)\n", raison);
        close(prise);
        prise = -1;
    }
}

/* Remplit les 12 premiers octets du message : IP puis entrée.
   Renvoie 0, ou -1 si l'adresse ou l'entrée est mal écrite. */
static int ecrire_adresse(unsigned char *message, const AdresseSimdom *a)
{
    struct in_addr ip;
    int i;

    /* "192.168.0.100" → 4 octets c0 a8 00 64 (déjà dans le bon ordre) */
    if (inet_pton(AF_INET, a->ip, &ip) != 1) {
        return -1;
    }
    memcpy(message, &ip.s_addr, 4);

    /* "00010111" → 8 octets 00 00 00 01 00 01 01 01 (un octet par chiffre) */
    if (strlen(a->entree) != 8) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (a->entree[i] != '0' && a->entree[i] != '1') {
            return -1;
        }
        message[4 + i] = (unsigned char)(a->entree[i] - '0');   /* '1' - '0' = 1 */
    }
    return 0;
}

/* Accusé de réception IMMÉDIAT de ce qu'on vient de recevoir.
   Sans ça, Linux attend jusqu'à 40 ms avant d'accuser réception ; si SimDom
   envoie sa réponse en plusieurs morceaux, il attend cet accusé avant
   d'envoyer la suite : 40 ms perdues par lecture, soit ~9 s par ronde
   de 211 équipements. Linux remet ce réglage à zéro tout seul : il faut
   le redemander après chaque recv. (Option propre à Linux, d'où le #ifdef.) */
static void accuser_reception(void)
{
#ifdef TCP_QUICKACK
    int oui = 1;
    setsockopt(prise, IPPROTO_TCP, TCP_QUICKACK, &oui, sizeof oui);
#endif
}

/* Envoie tout le message. MSG_NOSIGNAL : si SimDom a fermé la connexion,
   send renvoie une erreur au lieu de TUER le serveur (signal SIGPIPE). */
static int envoyer(const unsigned char *message, size_t taille)
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
static int vider(void)
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

/* Millisecondes écoulées depuis un instant de départ */
static long ecoule_ms(const struct timespec *debut)
{
    struct timespec maintenant;

    clock_gettime(CLOCK_MONOTONIC, &maintenant);
    return (maintenant.tv_sec - debut->tv_sec) * 1000
         + (maintenant.tv_nsec - debut->tv_nsec) / 1000000;
}

/* Reçoit exactement taille octets avant la fin du délai (compté depuis debut).
   Renvoie 0, SIMDOM_SANS_REPONSE (délai dépassé) ou SIMDOM_DECONNECTE. */
static int recevoir(unsigned char *tampon, size_t taille, const struct timespec *debut)
{
    size_t recus = 0;
    struct pollfd attente;
    long reste;
    long n;

    attente.fd = prise;
    attente.events = POLLIN;

    while (recus < taille) {
        reste = delai - ecoule_ms(debut);
        if (reste <= 0) {
            return SIMDOM_SANS_REPONSE;
        }
        /* poll : « réveille-moi quand il y a quelque chose à lire,
           ou au bout de reste ms au plus tard » */
        n = poll(&attente, 1, (int)reste);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return n == 0 ? SIMDOM_SANS_REPONSE : SIMDOM_DECONNECTE;
        }
        n = (long)recv(prise, tampon + recus, taille - recus, 0);
        if (n <= 0) {
            couper(n == 0 ? "fermée par SimDom" : "erreur de lecture");
            return SIMDOM_DECONNECTE;
        }
        recus += (size_t)n;
        accuser_reception();
    }
    return 0;
}

/* Pas connecté ? On tente une fois (ex. connexion coupée puis SimDom revenu). */
static int verifier_connexion(void)
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
        /* Message une seule fois : la ronde retente toutes les 5 s */
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

int simdom_commander(const AdresseSimdom *a, int etat, int mode)
{
    unsigned char message[TAILLE_CLIM];
    size_t taille = a->clim ? TAILLE_CLIM : TAILLE_MESSAGE;

    /* Seuls 00 et 01 partent d'ici : 02 (panne) est réservé aux tests,
       et un code inconnu (03...) bloquerait l'équipement. */
    if ((etat != 0 && etat != 1) || ecrire_adresse(message, a) != 0) {
        return -1;
    }
    if (verifier_connexion() != 0) {
        return -1;
    }
    message[12] = (etat == 1) ? CODE_ALLUMER : CODE_ETEINDRE;
    if (a->clim) {
        message[13] = (mode == 1) ? 1 : 0;
    }
    return envoyer(message, taille);
}

int simdom_lire(const AdresseSimdom *a, int *mode)
{
    unsigned char demande[TAILLE_CLIM];
    unsigned char reponse[TAILLE_CLIM];
    size_t taille = a->clim ? TAILLE_CLIM : TAILLE_MESSAGE;
    struct timespec debut;
    int resultat;

    if (ecrire_adresse(demande, a) != 0) {
        return SIMDOM_SANS_REPONSE;
    }
    if (verifier_connexion() != 0 || vider() != 0) {
        return SIMDOM_DECONNECTE;
    }
    demande[12] = CODE_LIRE;
    if (a->clim) {
        demande[13] = 0;                        /* sans 14e octet, une clim ne répond pas */
    }
    if (envoyer(demande, taille) != 0) {
        return SIMDOM_DECONNECTE;
    }

    /* La réponse reprend l'adresse demandée : on vérifie que c'est bien
       la bonne (sinon c'est une vieille réponse en retard : on la saute). */
    clock_gettime(CLOCK_MONOTONIC, &debut);
    for (;;) {
        resultat = recevoir(reponse, taille, &debut);
        if (resultat != 0) {
            return resultat;                    /* délai dépassé ou coupure */
        }
        if (memcmp(reponse, demande, TAILLE_ADRESSE) == 0) {
            break;
        }
    }

    if (reponse[12] > 2) {
        return SIMDOM_SANS_REPONSE;             /* état inconnu : indéterminé */
    }
    if (a->clim && mode != NULL) {
        *mode = (reponse[13] == 1) ? 1 : 0;
    }
    return reponse[12];
}

/* Taille du message d'une réponse dont on ne connaît pas la demande :
   d'après l'automate (octet 4 de l'IP) : .102 et .112 = climatisations. */
static size_t taille_selon_ip(const unsigned char *message)
{
    return (message[3] == 102 || message[3] == 112) ? TAILLE_CLIM : TAILLE_MESSAGE;
}

int simdom_lire_tout(const AdresseSimdom adresses[], int n, int etats[], int modes[])
{
    static unsigned char demandes[SIMDOM_MAX_RAFALE * TAILLE_CLIM];
    static unsigned char adresse_de[SIMDOM_MAX_RAFALE][TAILLE_ADRESSE];  /* 12 octets par équipement */
    unsigned char recu[256];        /* réponses reçues pas encore traitées */
    size_t nb_recu = 0;
    size_t taille_demandes = 0;
    size_t taille;
    int restantes = 0;              /* réponses encore attendues */
    struct pollfd attente;
    long n_lu;
    int i;

    if (n > SIMDOM_MAX_RAFALE) {
        n = SIMDOM_MAX_RAFALE;
    }

    /* 1. Préparer TOUTES les demandes (code 04) à la suite dans un tampon */
    for (i = 0; i < n; i++) {
        unsigned char *m = demandes + taille_demandes;

        etats[i] = SIMDOM_SANS_REPONSE;
        if (ecrire_adresse(m, &adresses[i]) != 0) {
            memset(adresse_de[i], 0xFF, TAILLE_ADRESSE);  /* ne correspondra à aucune réponse */
            continue;                           /* adresse mal écrite : pas demandé */
        }
        memcpy(adresse_de[i], m, TAILLE_ADRESSE);
        m[12] = CODE_LIRE;
        taille = TAILLE_MESSAGE;
        if (adresses[i].clim) {
            m[13] = 0;
            taille = TAILLE_CLIM;
        }
        taille_demandes += taille;
        restantes++;
    }

    /* 2. Tout envoyer d'un coup */
    if (verifier_connexion() != 0 || vider() != 0) {
        return SIMDOM_DECONNECTE;
    }
    if (envoyer(demandes, taille_demandes) != 0) {
        return SIMDOM_DECONNECTE;
    }

    /* 3. Lire les réponses au fil de l'eau. On abandonne seulement quand
          SimDom ne dit plus rien pendant « delai » ms (pas un délai total :
          tant que des réponses arrivent, on continue). */
    attente.fd = prise;
    attente.events = POLLIN;
    while (restantes > 0) {
        int pret = poll(&attente, 1, delai);

        if (pret < 0 && errno == EINTR) {
            continue;
        }
        if (pret == 0) {
            break;                              /* silence : les autres sont « sans réponse » */
        }
        if (pret < 0) {
            couper("erreur d'attente");
            return SIMDOM_DECONNECTE;
        }
        n_lu = (long)recv(prise, recu + nb_recu, sizeof recu - nb_recu, 0);
        if (n_lu <= 0) {
            couper(n_lu == 0 ? "fermée par SimDom" : "erreur de lecture");
            return SIMDOM_DECONNECTE;
        }
        accuser_reception();
        nb_recu += (size_t)n_lu;

        /* Découper les réponses complètes qui sont dans recu[] */
        while (nb_recu >= TAILLE_ADRESSE) {
            int trouve = -1;

            /* À qui est cette réponse ? On compare les 12 octets d'adresse */
            for (i = 0; i < n; i++) {
                if (etats[i] == SIMDOM_SANS_REPONSE
                    && memcmp(recu, adresse_de[i], TAILLE_ADRESSE) == 0) {
                    trouve = i;
                    break;
                }
            }
            taille = (trouve >= 0)
                   ? (adresses[trouve].clim ? TAILLE_CLIM : TAILLE_MESSAGE)
                   : taille_selon_ip(recu);
            if (nb_recu < taille) {
                break;                          /* réponse incomplète : attendre la suite */
            }
            if (trouve >= 0 && recu[12] <= 2) {
                etats[trouve] = recu[12];
                if (adresses[trouve].clim && modes != NULL) {
                    modes[trouve] = (recu[13] == 1) ? 1 : 0;
                }
                restantes--;
            }
            /* Retirer cette réponse du tampon (memmove : les zones se chevauchent) */
            memmove(recu, recu + taille, nb_recu - taille);
            nb_recu -= taille;
        }
    }
    return 0;
}

void simdom_fermer(void)
{
    if (prise >= 0) {
        close(prise);
        prise = -1;
    }
}
