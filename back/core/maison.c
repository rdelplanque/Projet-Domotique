/* ============================================================
   maison.c – Pilotage direct du bâtiment (page « Ma maison »)
   - GET /api/maison : consultation ;
   - POST /api/commande : pilotage d'un équipement ;
   - maison_synchroniser : toute la maison relue dans SimDom (démarrage) ;
   - maison_ronde : relecture régulière, par petites tranches.
   ============================================================ */

#include <stdio.h>          /* printf  */
#include <stdlib.h>         /* strtoll */
#include <string.h>         /* strcmp  */
#include <time.h>           /* clock_gettime : durée de la ronde */
#include <cjson/cJSON.h>

#include "maison.h"
#include "../bdd/bdd_maison.h"
#include "../auth/session.h"
#include "../auth/droits.h"
#include "../simdom/simdom.h"

#define PREFIXE "[maison] "

/* Noms exacts dans la base (type_utilisateur, type_equipement) */
#define PROFIL_INVITE "Invité"
#define TYPE_CLIM     "Climatisation réversible"

/* Au-delà, une relecture est signalée dans le terminal */
#define RONDE_LENTE_MS 1000

/* SimDom met 45 à 95 ms par équipement (mesuré) : une relecture complète
   prend 10 à 20 s. Pendant ce temps le guichet est fermé. La ronde ne relit
   donc qu'une TRANCHE d'équipements à la fois (~0,5 à 1 s), en commençant
   par la pièce affichée dans la page. */
#define TRANCHE           8

/* La pièce affichée reste prioritaire tant que la page la demande ;
   sans nouvelle demande pendant ce temps, on l'oublie (page fermée...). */
#define PRIORITE_DUREE_S  10

/* La connexion à la base, donnée une fois par main */
static PGconn *connexion = NULL;

/* La maison lue dans la base. « static » ici (et pas dans la fonction) :
   environ 120 Ko, trop gros pour la pile d'une fonction.
   Le serveur traite une requête à la fois, une seule fiche suffit. */
static Maison maison;

/* Ronde (voir plus bas) */
static long long piece_prioritaire = 0;     /* 0 = aucune               */
static time_t    moment_priorite = 0;       /* dernière demande de la page */
static int       marque_page = 0;           /* prochain équipement du tour */

static int relire_piece(long long id_piece);    /* écrite plus bas */


void maison_initialiser(PGconn *conn)
{
    connexion = conn;
}


/* Règle de visibilité : un Invité ne voit QUE ses pièces (acces_piece) ;
   les autres profils voient toute la maison.
   Le tri est fait ICI, côté serveur : ce qui n'est pas envoyé ne peut pas
   être lu par le navigateur, même avec les outils de développement (F12). */
static int peut_voir_piece(const char *profil, const Piece *piece)
{
    if (strcmp(profil, PROFIL_INVITE) == 0) {
        return piece->autorisee;            /* ligne dans acces_piece ? */
    }
    return 1;
}

/* La règle du pilotage, écrite UNE fois (servira aussi pour les commandes) :
   il faut le droit « piloter » et voir la pièce. */
static int peut_piloter(const char *profil, const Piece *piece)
{
    return a_le_droit(profil, DROIT_PILOTER) && peut_voir_piece(profil, piece);
}

/* La pièce d'id donné est-elle visible ? (pour filtrer les équipements) */
static int piece_visible(const char *profil, long long id_piece)
{
    int i;

    for (i = 0; i < maison.nb_pieces; i++) {
        if (maison.pieces[i].id == id_piece) {
            return peut_voir_piece(profil, &maison.pieces[i]);
        }
    }
    return 0;                               /* pièce inconnue : on ne montre pas */
}

/* L'étage contient-il au moins une pièce visible ? (sinon on ne l'envoie pas) */
static int etage_visible(const char *profil, long long id_etage)
{
    int i;

    for (i = 0; i < maison.nb_pieces; i++) {
        if (maison.pieces[i].id_etage == id_etage
            && peut_voir_piece(profil, &maison.pieces[i])) {
            return 1;
        }
    }
    return 0;
}

/* Ajoute un nombre, ou null si la valeur est absente (état indéterminé...) */
static void ajouter_entier_ou_null(cJSON *objet, const char *cle, int valeur)
{
    if (valeur == VALEUR_ABSENTE) {
        cJSON_AddNullToObject(objet, cle);
    } else {
        cJSON_AddNumberToObject(objet, cle, valeur);
    }
}


/* GET /api/maison */
void maison_lire(const Requete *req, Reponse *rep)
{
    const Session *s = req->session;        /* route a vérifié le jeton */
    cJSON *objet;
    cJSON *liste;
    cJSON *ligne;
    int i;

    char texte[24];
    long long id_piece = 0;

    /* 1. Demander les données à la base */
    if (bdd_lire_maison(connexion, s->id_utilisateur, &maison) != 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }

    /* 1bis. La page dit quelle pièce elle affiche : ?piece=23
             → la ronde la relira en premier. ?relire=1 (bouton Rafraîchir)
             → on la relit tout de suite dans SimDom, avant de répondre. */
    if (http_requete_parametre(req, "piece", texte, sizeof texte) == 0) {
        id_piece = strtoll(texte, NULL, 10);
        if (!piece_visible(s->profil, id_piece)) {
            id_piece = 0;                   /* pièce inconnue ou interdite : ignorée */
        }
    }
    if (id_piece > 0) {
        piece_prioritaire = id_piece;
        moment_priorite = time(NULL);
        if (http_requete_parametre(req, "relire", texte, sizeof texte) == 0
            && relire_piece(id_piece) == 0
            && bdd_lire_maison(connexion, s->id_utilisateur, &maison) != 0) {
            http_reponse_erreur(rep, 500, "erreur de la base de données");
            return;
        }
    }

    /* 2. Fabriquer la réponse JSON : trois listes, comme trois tables */
    objet = cJSON_CreateObject();
    cJSON_AddBoolToObject(objet, "ok", 1);

    liste = cJSON_AddArrayToObject(objet, "etages");
    for (i = 0; i < maison.nb_etages; i++) {
        if (!etage_visible(s->profil, maison.etages[i].id)) {
            continue;                       /* étage sans pièce visible */
        }
        ligne = cJSON_CreateObject();
        cJSON_AddNumberToObject(ligne, "id", (double)maison.etages[i].id);
        cJSON_AddStringToObject(ligne, "nom", maison.etages[i].nom);
        cJSON_AddItemToArray(liste, ligne);
    }

    liste = cJSON_AddArrayToObject(objet, "pieces");
    for (i = 0; i < maison.nb_pieces; i++) {
        const Piece *p = &maison.pieces[i];

        if (!peut_voir_piece(s->profil, p)) {
            continue;                       /* pièce non autorisée (Invité) */
        }
        ligne = cJSON_CreateObject();
        cJSON_AddNumberToObject(ligne, "id", (double)p->id);
        cJSON_AddStringToObject(ligne, "nom", p->nom);
        cJSON_AddNumberToObject(ligne, "etage", (double)p->id_etage);
        cJSON_AddBoolToObject(ligne, "piloter", peut_piloter(s->profil, p));
        cJSON_AddItemToArray(liste, ligne);
    }

    liste = cJSON_AddArrayToObject(objet, "equipements");
    for (i = 0; i < maison.nb_equipements; i++) {
        const Equipement *e = &maison.equipements[i];

        if (!piece_visible(s->profil, e->id_piece)) {
            continue;                       /* dans une pièce non visible */
        }
        ligne = cJSON_CreateObject();
        cJSON_AddNumberToObject(ligne, "id", (double)e->id);
        cJSON_AddStringToObject(ligne, "nom", e->nom);
        cJSON_AddStringToObject(ligne, "type", e->type);
        cJSON_AddNumberToObject(ligne, "piece", (double)e->id_piece);
        ajouter_entier_ou_null(ligne, "etat", e->etat);
        ajouter_entier_ou_null(ligne, "mode", e->mode);
        cJSON_AddItemToArray(liste, ligne);
    }

    /* 3. La boîte cJSON devient du texte dans la fiche réponse */
    http_reponse_liberer(rep);
    rep->code = 200;
    rep->json = cJSON_PrintUnformatted(objet);  /* NULL si mémoire pleine → 500 */
    cJSON_Delete(objet);
}


/* ------------------------------------------------------------
   Relecture de SimDom
   ------------------------------------------------------------ */

/* L'adresse SimDom d'un équipement de la base */
static AdresseSimdom adresse_de(const Equipement *e)
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
static int relire_piece(long long id_piece)
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


/* ------------------------------------------------------------
   Pilotage : POST /api/commande
   ------------------------------------------------------------ */

static const Equipement *trouver_equipement(long long id)
{
    int i;

    for (i = 0; i < maison.nb_equipements; i++) {
        if (maison.equipements[i].id == id) {
            return &maison.equipements[i];
        }
    }
    return NULL;
}

static const Piece *trouver_piece(long long id)
{
    int i;

    for (i = 0; i < maison.nb_pieces; i++) {
        if (maison.pieces[i].id == id) {
            return &maison.pieces[i];
        }
    }
    return NULL;
}

/* Lit un champ du JSON qui doit valoir 0 ou 1.
   Renvoie 0 ou 1, ou -1 s'il est absent ou invalide. */
static int champ_0_ou_1(const cJSON *json, const char *cle)
{
    const cJSON *element = cJSON_GetObjectItemCaseSensitive(json, cle);

    if (!cJSON_IsNumber(element)) {
        return -1;
    }
    if (element->valuedouble == 0) {
        return 0;
    }
    if (element->valuedouble == 1) {
        return 1;
    }
    return -1;
}

/* Réponse 200 : {"ok":true,"id":57,"etat":1,"mode":null} */
static void repondre_etat(Reponse *rep, long long id, int etat, int mode)
{
    cJSON *objet = cJSON_CreateObject();

    cJSON_AddBoolToObject(objet, "ok", 1);
    cJSON_AddNumberToObject(objet, "id", (double)id);
    ajouter_entier_ou_null(objet, "etat", etat);
    ajouter_entier_ou_null(objet, "mode", mode);

    http_reponse_liberer(rep);
    rep->code = 200;
    rep->json = cJSON_PrintUnformatted(objet);
    cJSON_Delete(objet);
}

/* Le cœur de la commande, une fois le JSON lu.
   etat_voulu : 0 ou 1 ; mode_voulu : 0, 1 ou -1 (pas donné). */
static void executer_commande(const Session *s, long long id,
                              int etat_voulu, int mode_voulu, Reponse *rep)
{
    const Equipement *e;
    const Piece *p;
    AdresseSimdom a;
    int etat;
    int mode;

    /* 1. L'équipement existe-t-il, et cet utilisateur peut-il le piloter ?
          Une pièce invisible (Invité) répond « inconnu » : on ne révèle
          même pas qu'elle existe (même idée que pour GET /api/maison). */
    if (bdd_lire_maison(connexion, s->id_utilisateur, &maison) != 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }
    e = trouver_equipement(id);
    p = (e != NULL) ? trouver_piece(e->id_piece) : NULL;
    if (e == NULL || p == NULL || !peut_voir_piece(s->profil, p)) {
        http_reponse_erreur(rep, 404, "équipement inconnu");
        return;
    }
    if (!peut_piloter(s->profil, p)) {
        http_reponse_erreur(rep, 403, "accès interdit");
        return;
    }

    a = adresse_de(e);
    if (a.clim && mode_voulu == -1) {
        mode_voulu = (e->mode == 1) ? 1 : 0;    /* mode pas donné : on garde l'actuel */
    }

    /* 2. État RÉEL avant la commande (une panne a pu arriver dans SimDom).
          Commander un équipement en panne le réparerait : interdit. */
    mode = e->mode;
    etat = simdom_lire(&a, &mode);
    if (etat == SIMDOM_DECONNECTE) {
        http_reponse_erreur(rep, 503, "maison non connectée");
        return;
    }
    if (etat == SIMDOM_SANS_REPONSE) {
        bdd_ecrire_etat(connexion, e->id, VALEUR_ABSENTE, mode);
        http_reponse_erreur(rep, 503, "l'équipement ne répond pas");
        return;
    }
    if (etat == 2) {
        bdd_ecrire_etat(connexion, e->id, 2, mode);
        http_reponse_erreur(rep, 409, "équipement en panne : commande refusée");
        return;
    }

    /* 3. La commande, puis relecture pour confirmer */
    if (simdom_commander(&a, etat_voulu, mode_voulu) != 0) {
        http_reponse_erreur(rep, 503, "maison non connectée");
        return;
    }
    etat = simdom_lire(&a, &mode);
    if (etat == SIMDOM_DECONNECTE) {
        http_reponse_erreur(rep, 503, "maison non connectée");
        return;
    }
    if (etat == SIMDOM_SANS_REPONSE) {
        etat = VALEUR_ABSENTE;
    }

    /* 4. La base suit l'état relu (même si la commande n'a pas marché) */
    bdd_ecrire_etat(connexion, e->id, etat, a.clim ? mode : VALEUR_ABSENTE);

    if (etat != etat_voulu || (a.clim && mode != mode_voulu)) {
        http_reponse_erreur(rep, 502, "commande non prise en compte par la maison");
        return;
    }
    printf(PREFIXE "%s (%s) → %s, par l'utilisateur %lld\n",
           e->nom, p->nom, etat == 1 ? "allumé" : "éteint", s->id_utilisateur);
    repondre_etat(rep, e->id, etat, a.clim ? mode : VALEUR_ABSENTE);
}

void maison_commander(const Requete *req, Reponse *rep)
{
    cJSON *json;
    const cJSON *id;
    int etat;
    int mode = -1;

    /* Corps attendu : {"id":57,"etat":1} (+ "mode":0 ou 1 pour une clim) */
    if (req->corps == NULL || (json = cJSON_Parse(req->corps)) == NULL) {
        http_reponse_erreur(rep, 400, "corps JSON attendu");
        return;
    }
    id = cJSON_GetObjectItemCaseSensitive(json, "id");
    etat = champ_0_ou_1(json, "etat");
    if (cJSON_GetObjectItemCaseSensitive(json, "mode") != NULL) {
        mode = champ_0_ou_1(json, "mode");
        if (mode == -1) {
            etat = -1;                          /* mode donné mais invalide */
        }
    }
    if (!cJSON_IsNumber(id) || id->valuedouble < 1 || etat == -1) {
        cJSON_Delete(json);
        http_reponse_erreur(rep, 400, "id, etat (0 ou 1) et mode (0 ou 1) attendus");
        return;
    }

    executer_commande(req->session, (long long)id->valuedouble, etat, mode, rep);
    cJSON_Delete(json);
}
