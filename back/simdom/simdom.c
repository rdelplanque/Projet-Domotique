/* ============================================================
   simdom.c – Une commande, une lecture
   Fabrique les messages binaires d'un équipement, les envoie
   et lit la réponse. Ne connaît rien de la base ni du web.
   Voir aussi : simdom_connexion.c (la connexion),
                simdom_rafale.c (plusieurs lectures d'un coup).
   ============================================================ */

#include <string.h>         /* memcmp, memcpy, strlen  */
#include <errno.h>          /* errno, EINTR            */
#include <time.h>           /* clock_gettime           */
#include <poll.h>           /* poll : attendre avec un délai */
#include <sys/socket.h>     /* recv                    */
#include <netinet/in.h>     /* struct in_addr          */
#include <arpa/inet.h>      /* inet_pton               */

#include "simdom.h"
#include "simdom_interne.h"

/* Remplit les 12 premiers octets du message : IP puis entrée.
   Renvoie 0, ou -1 si l'adresse ou l'entrée est mal écrite. */
int ecrire_adresse(unsigned char *message, const AdresseSimdom *a)
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
