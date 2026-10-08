/* ============================================================
   session.c – Les sessions ouvertes (jetons), gardées en mémoire
   Si le serveur redémarre, toutes les sessions sont perdues :
   chacun devra se reconnecter (simple et acceptable ici).
   ============================================================ */

#include <stdio.h>          /* fopen, fread, snprintf */
#include <string.h>         /* strcmp, memset         */

#include "session.h"

#define PREFIXE "[auth] "
#define OCTETS_ALEATOIRES 32            /* 32 octets → 64 caractères hexa */

/* Le tableau des sessions. static : invisible hors de ce fichier,
   et il existe pendant toute la vie du serveur. */
static Session sessions[SESSION_MAX];


/* Fabrique un jeton imprévisible avec /dev/urandom (le générateur
   aléatoire du noyau Linux, fait pour la sécurité ; rand() ne l'est pas).
   Chaque octet est écrit en 2 caractères hexadécimaux : 0x3f → "3f". */
static int fabriquer_jeton(char *jeton)
{
    unsigned char octets[OCTETS_ALEATOIRES];
    FILE *alea;
    size_t lus;
    int i;

    alea = fopen("/dev/urandom", "rb");
    if (alea == NULL) {
        perror(PREFIXE "/dev/urandom");
        return -1;
    }
    lus = fread(octets, 1, sizeof octets, alea);
    fclose(alea);
    if (lus != sizeof octets) {
        return -1;
    }

    for (i = 0; i < OCTETS_ALEATOIRES; i++) {
        snprintf(jeton + 2 * i, 3, "%02x", octets[i]);
    }
    return 0;
}

/* Choisit une case : une libre ou expirée, sinon la plus ancienne. */
static Session *case_disponible(time_t maintenant)
{
    Session *plus_ancienne = &sessions[0];
    int i;

    for (i = 0; i < SESSION_MAX; i++) {
        if (sessions[i].jeton[0] == '\0' || sessions[i].expiration <= maintenant) {
            return &sessions[i];
        }
        if (sessions[i].expiration < plus_ancienne->expiration) {
            plus_ancienne = &sessions[i];
        }
    }
    return plus_ancienne;               /* tout est plein : on écrase */
}


int session_creer(long long id_utilisateur, const char *profil, char *jeton_sortie)
{
    time_t maintenant = time(NULL);
    Session *s = case_disponible(maintenant);

    memset(s, 0, sizeof *s);
    if (fabriquer_jeton(s->jeton) != 0) {
        s->jeton[0] = '\0';             /* la case reste libre */
        return -1;
    }
    s->id_utilisateur = id_utilisateur;
    snprintf(s->profil, sizeof s->profil, "%s", profil);
    s->expiration = maintenant + SESSION_DUREE_S;

    memcpy(jeton_sortie, s->jeton, SESSION_TAILLE_JETON);
    return 0;
}

const Session *session_trouver(const char *jeton)
{
    time_t maintenant = time(NULL);
    int i;

    if (jeton == NULL || jeton[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < SESSION_MAX; i++) {
        if (strcmp(sessions[i].jeton, jeton) == 0) {
            if (sessions[i].expiration <= maintenant) {
                sessions[i].jeton[0] = '\0';    /* expirée : on libère */
                return NULL;
            }
            return &sessions[i];
        }
    }
    return NULL;
}

void session_supprimer(const char *jeton)
{
    int i;

    for (i = 0; i < SESSION_MAX; i++) {
        if (jeton != NULL && strcmp(sessions[i].jeton, jeton) == 0) {
            memset(&sessions[i], 0, sizeof sessions[i]);
            return;
        }
    }
}