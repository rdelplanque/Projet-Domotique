/* ============================================================
   scenario.c – Les routes /api/scenarios/... (voir scenario.h)

   Ici, seulement le HTTP : lire le JSON reçu, appeler le service
   (les règles, service.c), traduire son Resultat en réponse.
   ============================================================ */

#include <stdio.h>          /* snprintf                */
#include <string.h>         /* strlen, memcpy          */
#include <ctype.h>          /* isspace, iscntrl        */
#include <cjson/cJSON.h>

#include "scenario.h"
#include "service.h"
#include "ldsd.h"
#include "execution.h"
#include "planificateur.h"
#include "../bdd/bdd_scenario.h"
#include "../bdd/bdd_maison.h"
#include "../auth/session.h"

#define NOM_TAILLE_MAX 100      /* nom_scenario varchar(100) */

/* La connexion à la base (lister, vérifier) */
static PGconn *connexion = NULL;

/* La liste lue dans la base : environ 530 Ko, trop gros pour la pile
   d'une fonction (même raison que « maison » dans maison.c). */
static ListeScenarios liste;

/* La maison et le scénario lu, pour « Vérifier » (hors de la pile) */
static Maison    maison_lue;
static Programme programme;

/* Le texte reçu de l'éditeur (8 Ko : hors de la pile, lui aussi) */
static char texte_recu[SCENARIO_TAILLE_TEXTE];


void scenario_routes_initialiser(PGconn *conn)
{
    connexion = conn;
}


/* ------------------------------------------------------------
   Petits outils
   ------------------------------------------------------------ */

/* Traduit le résultat du service en réponse HTTP.
   OK : {"ok":true,"id":3} ; sinon {"erreur":"...","ligne":4} (ligne si texte faux). */
static void repondre(Reponse *rep, const Resultat *r, long long id)
{
    static const int CODES[] = {
        [ISSUE_OK] = 200, [ISSUE_INCONNU] = 404, [ISSUE_REFUS] = 409,
        [ISSUE_BASE] = 500, [ISSUE_MAISON] = 503
    };
    cJSON *objet;
    char json[64];

    if (r->issue == ISSUE_OK) {
        snprintf(json, sizeof json, "{\"ok\":true,\"id\":%lld}", id);
        http_reponse_json(rep, 200, json);
        return;
    }
    if (r->ligne == 0) {
        http_reponse_erreur(rep, CODES[r->issue], r->message);
        return;
    }
    /* Texte faux : le numéro de ligne est aussi envoyé à part (pour la page) */
    objet = cJSON_CreateObject();
    cJSON_AddStringToObject(objet, "erreur", r->message);
    cJSON_AddNumberToObject(objet, "ligne", r->ligne);
    http_reponse_liberer(rep);
    rep->code = CODES[r->issue];
    rep->json = cJSON_PrintUnformatted(objet);  /* NULL si mémoire pleine → 500 */
    cJSON_Delete(objet);
}

/* Lit {"id":3} dans le corps, et le booléen nommé choix s'il est demandé
   (choix = NULL : pas de booléen). Renvoie l'id, ou 0 si le corps est invalide. */
static long long lire_id(const Requete *req, const char *choix, int *valeur)
{
    cJSON *corps;
    const cJSON *id;
    const cJSON *b;
    long long resultat = 0;

    corps = (req->corps != NULL) ? cJSON_Parse(req->corps) : NULL;
    id = cJSON_GetObjectItemCaseSensitive(corps, "id");
    if (cJSON_IsNumber(id) && id->valuedouble >= 1) {
        resultat = (long long)id->valuedouble;
    }
    if (choix != NULL) {
        b = cJSON_GetObjectItemCaseSensitive(corps, choix);
        if (cJSON_IsBool(b)) {
            *valeur = cJSON_IsTrue(b);
        } else {
            resultat = 0;
        }
    }
    cJSON_Delete(corps);
    return resultat;
}


/* ------------------------------------------------------------
   Lire : la liste, vérifier un texte
   ------------------------------------------------------------ */

/* GET /api/scenarios */
void scenario_lister(const Requete *req, Reponse *rep)
{
    cJSON *objet;
    cJSON *tableau;
    cJSON *ligne;
    EnTete e;
    ErreurLdsd err;
    char quand[128];
    const char *type;
    int i;

    (void)req;      /* tous les profils autorisés voient tous les scénarios */

    if (bdd_lire_scenarios(connexion, &liste) != 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }

    objet = cJSON_CreateObject();
    cJSON_AddBoolToObject(objet, "ok", 1);
    tableau = cJSON_AddArrayToObject(objet, "scenarios");

    for (i = 0; i < liste.nb; i++) {
        const Scenario *s = &liste.scenarios[i];

        /* Le type et le « quand », d'après l'en-tête (lu par ldsd) */
        quand[0] = '\0';
        if (ldsd_lire_en_tete(s->script, &e, &err) != 0) {
            type = "inconnu";
        } else if (e.type == DECLENCHEUR_DIRECT) {
            type = "direct";
        } else {
            type = "programme";
            ldsd_decrire_en_tete(&e, quand, sizeof quand);
        }

        ligne = cJSON_CreateObject();
        cJSON_AddNumberToObject(ligne, "id", (double)s->id);
        cJSON_AddStringToObject(ligne, "nom", s->nom);
        cJSON_AddStringToObject(ligne, "createur", s->createur);
        cJSON_AddStringToObject(ligne, "type", type);
        cJSON_AddStringToObject(ligne, "quand", quand);
        cJSON_AddBoolToObject(ligne, "utilisable", s->utilisable);
        cJSON_AddBoolToObject(ligne, "actif", s->actif);
        cJSON_AddBoolToObject(ligne, "en_cours", s->id == execution_en_cours());
        cJSON_AddBoolToObject(ligne, "en_attente", planificateur_en_attente(s->id));
        cJSON_AddStringToObject(ligne, "script", s->script);
        cJSON_AddItemToArray(tableau, ligne);
    }

    http_reponse_liberer(rep);
    rep->code = 200;
    rep->json = cJSON_PrintUnformatted(objet);  /* NULL si mémoire pleine → 500 */
    cJSON_Delete(objet);
}


/* POST /api/scenarios/verifier */
void scenario_verifier(const Requete *req, Reponse *rep)
{
    cJSON *corps;
    const cJSON *texte;
    cJSON *objet;
    cJSON *lecture;
    ErreurLdsd err;
    char ligne[400];
    char action[350];
    int i;

    /* 1. Le corps : {"texte":"..."} */
    if (req->corps == NULL) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"texte\":\"...\"}");
        return;
    }
    corps = cJSON_Parse(req->corps);
    texte = cJSON_GetObjectItemCaseSensitive(corps, "texte");
    if (!cJSON_IsString(texte)) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"texte\":\"...\"}");
        return;
    }

    /* 2. La maison à jour (les noms des pièces et équipements) */
    if (bdd_lire_maison(connexion, 0, &maison_lue) != 0) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }

    /* 3. La lecture du texte */
    objet = cJSON_CreateObject();
    if (ldsd_lire(texte->valuestring, &maison_lue, &programme, &err) != 0) {
        cJSON_AddBoolToObject(objet, "ok", 0);
        cJSON_AddNumberToObject(objet, "ligne", err.ligne);
        cJSON_AddStringToObject(objet, "erreur", err.message);
    } else {
        cJSON_AddBoolToObject(objet, "ok", 1);
        cJSON_AddStringToObject(objet, "type",
            programme.en_tete.type == DECLENCHEUR_DIRECT ? "direct" : "programme");
        lecture = cJSON_AddArrayToObject(objet, "lecture");
        ldsd_decrire_en_tete(&programme.en_tete, ligne, sizeof ligne);
        cJSON_AddItemToArray(lecture, cJSON_CreateString(ligne));
        for (i = 0; i < programme.nb_actions; i++) {
            ldsd_decrire_action(&maison_lue, &programme.actions[i], action, sizeof action);
            snprintf(ligne, sizeof ligne, "ligne %d : %s", programme.actions[i].ligne, action);
            cJSON_AddItemToArray(lecture, cJSON_CreateString(ligne));
        }
    }
    cJSON_Delete(corps);

    http_reponse_liberer(rep);
    rep->code = 200;
    rep->json = cJSON_PrintUnformatted(objet);  /* NULL si mémoire pleine → 500 */
    cJSON_Delete(objet);
}



/* ------------------------------------------------------------
   Créer et modifier (éditeur scenario.html)
   ------------------------------------------------------------ */

/* Lit le corps {"id":3,"nom":"...","texte":"..."} (id seulement si avec_id).
   Le nom est débarrassé de ses espaces au début et à la fin.
   Range le résultat dans *id, nom (NOM_TAILLE_MAX + 1 octets) et texte_recu.
   Renvoie 0 si tout est bon, -1 sinon (la réponse d'erreur 400 est remplie). */
static int lire_formulaire(const Requete *req, Reponse *rep, int avec_id,
                           long long *id, char *nom)
{
    cJSON *corps;
    const cJSON *id_json;
    const cJSON *nom_json;
    const cJSON *texte_json;
    const char *debut;
    size_t n;
    size_t i;

    corps = (req->corps != NULL) ? cJSON_Parse(req->corps) : NULL;
    id_json    = cJSON_GetObjectItemCaseSensitive(corps, "id");
    nom_json   = cJSON_GetObjectItemCaseSensitive(corps, "nom");
    texte_json = cJSON_GetObjectItemCaseSensitive(corps, "texte");

    if (!cJSON_IsString(nom_json) || !cJSON_IsString(texte_json)
        || (avec_id && (!cJSON_IsNumber(id_json) || id_json->valuedouble < 1))) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, avec_id
            ? "corps JSON attendu : {\"id\":...,\"nom\":\"...\",\"texte\":\"...\"}"
            : "corps JSON attendu : {\"nom\":\"...\",\"texte\":\"...\"}");
        return -1;
    }
    if (avec_id) {
        *id = (long long)id_json->valuedouble;
    }

    /* Le nom, sans les espaces autour : « début » avance, « n » recule */
    debut = nom_json->valuestring;
    while (isspace((unsigned char)*debut)) {
        debut++;
    }
    n = strlen(debut);
    while (n > 0 && isspace((unsigned char)debut[n - 1])) {
        n--;
    }
    if (n == 0) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, "donnez un nom au scénario");
        return -1;
    }
    if (n > NOM_TAILLE_MAX) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, "nom trop long (100 octets au plus ; une lettre accentuée en compte 2)");
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (iscntrl((unsigned char)debut[i])) {     /* retour à la ligne, tabulation... */
            cJSON_Delete(corps);
            http_reponse_erreur(rep, 400, "le nom doit tenir sur une ligne");
            return -1;
        }
    }
    memcpy(nom, debut, n);
    nom[n] = '\0';

    /* Le texte */
    if (strlen(texte_json->valuestring) >= sizeof texte_recu) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, "texte trop long (8191 octets au plus)");
        return -1;
    }
    snprintf(texte_recu, sizeof texte_recu, "%s", texte_json->valuestring);

    cJSON_Delete(corps);
    return 0;
}


/* POST /api/scenarios/creer */
void scenario_creer(const Requete *req, Reponse *rep)
{
    char nom[NOM_TAILLE_MAX + 1];
    long long id = 0;
    Resultat r;

    if (lire_formulaire(req, rep, 0, NULL, nom) != 0) {
        return;                                 /* réponse 400 déjà prête */
    }
    /* Le créateur est celui qui est connecté : lu dans sa session,
       jamais envoyé par la page (modifiable avec F12). */
    service_creer(nom, texte_recu, req->session->id_utilisateur, &id, &r);
    repondre(rep, &r, id);
}

/* POST /api/scenarios/modifier */
void scenario_modifier(const Requete *req, Reponse *rep)
{
    char nom[NOM_TAILLE_MAX + 1];
    long long id = 0;
    Resultat r;

    if (lire_formulaire(req, rep, 1, &id, nom) != 0) {
        return;
    }
    service_modifier(id, nom, texte_recu, req->session->id_utilisateur, &r);
    repondre(rep, &r, id);
}


/* ------------------------------------------------------------
   Les boutons des cartes : {"id":3} (+ un booléen)
   ------------------------------------------------------------ */

/* POST /api/scenarios/utilisable   {"id":3,"utilisable":true} */
void scenario_utilisable(const Requete *req, Reponse *rep)
{
    int utilisable = 0;
    long long id = lire_id(req, "utilisable", &utilisable);
    Resultat r;

    if (id == 0) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...,\"utilisable\":true|false}");
        return;
    }
    service_utilisable(id, utilisable, req->session->id_utilisateur, &r);
    repondre(rep, &r, id);
}

/* POST /api/scenarios/actif   {"id":3,"actif":true} */
void scenario_actif(const Requete *req, Reponse *rep)
{
    int actif = 0;
    long long id = lire_id(req, "actif", &actif);
    Resultat r;

    if (id == 0) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...,\"actif\":true|false}");
        return;
    }
    service_activer(id, actif, req->session->id_utilisateur, &r);
    repondre(rep, &r, id);
}

/* Les routes qui ne reçoivent que {"id":3} */
static void route_id(const Requete *req, Reponse *rep,
                     void (*service)(long long id, long long qui, Resultat *r))
{
    long long id = lire_id(req, NULL, NULL);
    Resultat r;

    if (id == 0) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...}");
        return;
    }
    service(id, req->session->id_utilisateur, &r);
    repondre(rep, &r, id);
}

/* POST /api/scenarios/lancer     {"id":3} */
void scenario_lancer(const Requete *req, Reponse *rep)
{
    route_id(req, rep, service_lancer);
}

/* POST /api/scenarios/arreter    {"id":3} */
void scenario_arreter(const Requete *req, Reponse *rep)
{
    route_id(req, rep, service_arreter);
}

/* POST /api/scenarios/supprimer  {"id":3} */
void scenario_supprimer(const Requete *req, Reponse *rep)
{
    route_id(req, rep, service_supprimer);
}
