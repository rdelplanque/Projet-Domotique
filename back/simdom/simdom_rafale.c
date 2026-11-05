/* ============================================================
   simdom_rafale.c – Lire plusieurs équipements d'un coup
   Toutes les demandes partent ensemble, puis les réponses sont
   rangées grâce à leur adresse (voir simdom.h).
   ============================================================ */

#include <string.h>         /* memcmp, memcpy, memmove, memset */
#include <errno.h>          /* errno, EINTR            */
#include <poll.h>           /* poll : attendre avec un délai */
#include <sys/socket.h>     /* recv                    */

#include "simdom.h"
#include "simdom_interne.h"

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
