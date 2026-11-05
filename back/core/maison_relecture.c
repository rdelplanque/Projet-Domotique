/* ============================================================
   maison_relecture.c – Relire SimDom
   La synchronisation au démarrage, la ronde par tranches
   et la relecture d'une pièce (bouton Rafraîchir).
   ============================================================ */

#include <stdio.h>          /* printf  */
#include <string.h>         /* strcmp  */
#include <time.h>           /* clock_gettime, time */
#include <cjson/cJSON.h>

#include "maison_interne.h"

/* Nom exact dans la base (type_equipement) */
#define TYPE_CLIM     "Climatisation réversible"

/* Au-delà, une relecture est signalée dans le terminal */
#define RONDE_LENTE_MS 3000

/* SimDom met 45 à 95 ms par équipement (mesuré) : une relecture complète
   prend 10 à 20 s. Pendant ce temps le guichet est fermé. La ronde ne relit
   donc qu'une TRANCHE d'équipements à la fois (~0,5 à 1 s), en commençant
   par la pièce affichée dans la page. */
#define TRANCHE           8

/* La pièce affichée reste prioritaire tant que la page la demande ;
   sans nouvelle demande pendant ce temps, on l'oublie (page fermée...). */
#define PRIORITE_DUREE_S  10

/* Ronde (voir plus bas) */
static long long piece_prioritaire = 0;     /* 0 = aucune               */
static time_t    moment_priorite = 0;       /* dernière demande de la page */
static int       marque_page = 0;           /* prochain équipement du tour */


/* La page signale la pièce qu'elle affiche : la ronde la relit en premier */
void maison_noter_piece(long long id_piece)
{
    piece_prioritaire = id_piece;
    moment_priorite = time(NULL);
}

/* L'adresse SimDom d'un équipement de la base */
AdresseSimdom adresse_de(const Equipement *e)
{
    AdresseSimdom a;

    a.ip = e->ip;
    a.entree = e->entree;
    a.clim = (strcmp(e->type, TYPE_CLIM) == 0);   /* clim = message de 14 octets */
    return a;
}

/* Texte d'un état, pour le terminal */
static const char *nom_etat(int etat)
{
    switch (etat) {
        case 0:  return "éteint";
        case 1:  return "allumé";
        case 2:  return "en panne";
        default: return "indéterminé";
    }
}

/* Millisecondes écoulées depuis debut */
static long ecoule_ms(const struct timespec *debut)
{
    struct timespec maintenant;

    clock_gettime(CLOCK_MONOTONIC, &maintenant);
    return (maintenant.tv_sec - debut->tv_sec) * 1000
         + (maintenant.tv_nsec - debut->tv_nsec) / 1000000;
}

/* Le cœur de toutes les relectures : relit dans SimDom, en une rafale,
   les équipements dont les numéros (cases de maison.equipements) sont
   dans choisis[0..n-1], et écrit dans la base ceux qui ont changé.
   maison doit avoir été lue juste avant (bdd_lire_maison).
   bavard = 1 : écrire chaque changement dans le terminal.
   Renvoie le nombre de changements, ou -1 si SimDom est perdu. */
static int relire(const int choisis[], int n, int bavard, int *sans_reponse)
{
    /* static : tableaux de 512 cases, gardés hors de la pile */
    static AdresseSimdom adresses[MAISON_MAX_EQUIPEMENTS];
    static int etats[MAISON_MAX_EQUIPEMENTS];
    static int modes[MAISON_MAX_EQUIPEMENTS];
    int i;
    int changes = 0;

    for (i = 0; i < n; i++) {
        adresses[i] = adresse_de(&maison.equipements[choisis[i]]);
        modes[i] = maison.equipements[choisis[i]].mode;
    }
    if (simdom_lire_tout(adresses, n, etats, modes) != 0) {
        fprintf(stderr, PREFIXE "SimDom perdu pendant la relecture\n");
        return -1;
    }

    *sans_reponse = 0;
    for (i = 0; i < n; i++) {
        const Equipement *e = &maison.equipements[choisis[i]];
        int etat = etats[i];

        if (etat == SIMDOM_SANS_REPONSE) {
            etat = VALEUR_ABSENTE;              /* indéterminé (NULL) */
            (*sans_reponse)++;
        }
        if ((etat != e->etat || modes[i] != e->mode)
            && bdd_ecrire_etat(connexion, e->id, etat, modes[i]) == 0) {
            changes++;
            if (bavard) {
                printf(PREFIXE "changement : %s : %s → %s\n",
                       e->nom, nom_etat(e->etat), nom_etat(etat));
            }
        }
    }
    return changes;
}

/* Prépare une relecture : SimDom connecté (sinon on retente) et maison lue.
   Renvoie 0 si on peut relire, -1 sinon. */
static int preparer(void)
{
    if (!simdom_est_connecte() && simdom_connecter() != 0) {
        return -1;                              /* états de la base gardés */
    }
    /* 0 = aucun utilisateur : on n'a besoin que des équipements */
    if (bdd_lire_maison(connexion, 0, &maison) != 0) {
        fprintf(stderr, PREFIXE "relecture impossible : base injoignable\n");
        return -1;
    }
    return 0;
}

/* Relit tout de suite les équipements d'une pièce (bouton Rafraîchir).
   Renvoie 0 si la relecture a eu lieu, -1 sinon. */
int relire_piece(long long id_piece)
{
    static int choisis[MAISON_MAX_EQUIPEMENTS];
    int n = 0;
    int i;
    int sans_reponse;

    if (preparer() != 0) {
        return -1;
    }
    for (i = 0; i < maison.nb_equipements; i++) {
        if (maison.equipements[i].id_piece == id_piece) {
            choisis[n++] = i;
        }
    }
    return relire(choisis, n, 1, &sans_reponse) < 0 ? -1 : 0;
}

void maison_synchroniser(void)
{
    static int choisis[MAISON_MAX_EQUIPEMENTS];
    struct timespec debut;
    int changes;
    int sans_reponse = 0;
    int i;

    if (preparer() != 0) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &debut);
    for (i = 0; i < maison.nb_equipements; i++) {
        choisis[i] = i;                         /* toute la maison */
    }
    changes = relire(choisis, maison.nb_equipements, 0, &sans_reponse);
    if (changes >= 0) {
        printf(PREFIXE "synchronisation : %d équipements lus en %ld ms, %d changés, %d sans réponse\n",
               maison.nb_equipements, ecoule_ms(&debut), changes, sans_reponse);
    }
}

/* La ronde : à chaque passage, une TRANCHE d'équipements.
     1. d'abord ceux de la pièce affichée dans la page (si elle est récente) ;
     2. puis on complète avec les suivants du tour de la maison
        (marque_page : on reprend là où le passage précédent s'est arrêté). */
void maison_ronde(void)
{
    int choisis[TRANCHE];
    struct timespec debut;
    long long prioritaire = piece_prioritaire;
    int complete = 1;                           /* pièce affichée relue en entier ? */
    int n = 0;
    int vus = 0;
    int i;
    int changes;
    int sans_reponse = 0;
    long duree;

    if (preparer() != 0 || maison.nb_equipements == 0) {
        return;
    }
    if (time(NULL) - moment_priorite > PRIORITE_DUREE_S) {
        prioritaire = 0;                        /* page fermée ou ailleurs */
    }

    /* 1. La pièce affichée (au plus TRANCHE équipements) */
    for (i = 0; i < maison.nb_equipements; i++) {
        if (prioritaire != 0 && maison.equipements[i].id_piece == prioritaire) {
            if (n < TRANCHE) {
                choisis[n++] = i;
            } else {
                complete = 0;                   /* pièce plus grande que la tranche */
            }
        }
    }

    /* 2. Le tour de la maison. On saute la pièce affichée si elle vient
          d'être relue en entier (sinon le tour s'occupe du reste). */
    while (n < TRANCHE && vus < maison.nb_equipements) {
        if (marque_page >= maison.nb_equipements) {
            marque_page = 0;                    /* fin du tour : on recommence */
        }
        if (!complete || maison.equipements[marque_page].id_piece != prioritaire) {
            choisis[n++] = marque_page;
        }
        marque_page++;
        vus++;
    }

    clock_gettime(CLOCK_MONOTONIC, &debut);
    changes = relire(choisis, n, 1, &sans_reponse);
    duree = ecoule_ms(&debut);
    if (changes >= 0 && duree > RONDE_LENTE_MS) {
        printf(PREFIXE "ronde lente : %d équipements en %ld ms\n", n, duree);
    }
}
