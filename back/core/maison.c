/* ============================================================
   maison.c – Pilotage direct du bâtiment (page « Ma maison »)
   Les règles de visibilité et GET /api/maison : la consultation.
   Voir aussi : maison_relecture.c (relire SimDom),
                maison_commande.c (POST /api/commande).
   ============================================================ */

#include <stdlib.h>         /* strtoll */
#include <string.h>         /* strcmp  */
#include <cjson/cJSON.h>

#include "maison_interne.h"
#include "../auth/session.h"
#include "../auth/droits.h"

/* Noms exacts dans la base (type_utilisateur) */
#define PROFIL_INVITE "Invité"

/* La connexion à la base, donnée une fois par main */
PGconn *connexion = NULL;

/* La maison lue dans la base. « static » ici (et pas dans la fonction) :
   environ 120 Ko, trop gros pour la pile d'une fonction.
   Le serveur traite une requête à la fois, une seule fiche suffit. */
Maison maison;


void maison_initialiser(PGconn *conn)
{
    connexion = conn;
}


/* Règle de visibilité : un Invité ne voit QUE ses pièces (acces_piece) ;
   les autres profils voient toute la maison.
   Le tri est fait ICI, côté serveur : ce qui n'est pas envoyé ne peut pas
   être lu par le navigateur, même avec les outils de développement (F12). */
int peut_voir_piece(const char *profil, const Piece *piece)
{
    if (strcmp(profil, PROFIL_INVITE) == 0) {
        return piece->autorisee;            /* ligne dans acces_piece ? */
    }
    return 1;
}

/* La règle du pilotage, écrite UNE fois (servira aussi pour les commandes) :
   il faut le droit « piloter » et voir la pièce. */
int peut_piloter(const char *profil, const Piece *piece)
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
void ajouter_entier_ou_null(cJSON *objet, const char *cle, int valeur)
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
        maison_noter_piece(id_piece);
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
