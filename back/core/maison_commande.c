/* ============================================================
   maison_commande.c – Pilotage : POST /api/commande
   Vérifie les droits, relit l'état, commande, relit pour confirmer.
   ============================================================ */

#include <stdio.h>          /* printf */
#include <cjson/cJSON.h>

#include "maison_interne.h"
#include "../auth/session.h"

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
