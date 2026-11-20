/* ============================================================
   scenario.c – Les scénarios (voir scenario.h)
   ============================================================ */

#include <stdio.h>          /* snprintf                */
#include <string.h>         /* strstr, strncmp, strspn, strlen */
#include <ctype.h>          /* isspace, iscntrl        */
#include <cjson/cJSON.h>

#include "scenario.h"
#include "../bdd/bdd_scenario.h"
#include "../bdd/bdd_maison.h"
#include "ldsd.h"
#include "execution.h"
#include "../auth/session.h"

#define MARQUE_EN_TETE "En-tête :"
#define EN_TETE_DIRECT "au clic"

/* La connexion à la base, donnée une fois par main */
static PGconn *connexion = NULL;

/* La liste lue dans la base : environ 530 Ko, trop gros pour la pile
   d'une fonction (même raison que « maison » dans maison.c). */
static ListeScenarios liste;

/* La maison, pour retrouver les noms écrits dans les scénarios,
   et le scénario lu (mêmes raisons de taille : hors de la pile). */
static Maison    maison_lue;
static Programme programme;
static Scenario  un_scenario;

/* Le texte reçu de l'éditeur (8 Ko : hors de la pile, lui aussi) */
static char texte_recu[SCENARIO_TAILLE_TEXTE];

#define NOM_TAILLE_MAX 100      /* nom_scenario varchar(100) */


void scenario_initialiser(PGconn *conn)
{
    connexion = conn;
    execution_initialiser(conn);
}


TypeScenario scenario_type(const char *script)
{
    const char *p = strstr(script, MARQUE_EN_TETE);
    size_t n;

    if (p == NULL) {
        return SCENARIO_INCONNU;
    }
    p += strlen(MARQUE_EN_TETE);

    /* Sauter la fin de la ligne « En-tête : », puis les lignes vides
       et les espaces (\r compris : texte tapé sous Windows) */
    p += strspn(p, " \t\r\n");
    if (*p == '\0') {
        return SCENARIO_INCONNU;
    }

    /* « au clic » doit être seul sur sa ligne (espaces de fin tolérés) */
    n = strlen(EN_TETE_DIRECT);
    if (strncmp(p, EN_TETE_DIRECT, n) == 0) {
        p += n;
        p += strspn(p, " \t\r");
        if (*p == '\n' || *p == '\0') {
            return SCENARIO_DIRECT;
        }
    }
    return SCENARIO_PROGRAMME;
}


/* Le type en texte pour le navigateur */
static const char *nom_type(TypeScenario t)
{
    switch (t) {
    case SCENARIO_DIRECT:    return "direct";
    case SCENARIO_PROGRAMME: return "programme";
    default:                 return "inconnu";
    }
}


/* GET /api/scenarios */
void scenario_lister(const Requete *req, Reponse *rep)
{
    cJSON *objet;
    cJSON *tableau;
    cJSON *ligne;
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

        ligne = cJSON_CreateObject();
        cJSON_AddNumberToObject(ligne, "id", (double)s->id);
        cJSON_AddStringToObject(ligne, "nom", s->nom);
        cJSON_AddStringToObject(ligne, "createur", s->createur);
        cJSON_AddStringToObject(ligne, "type", nom_type(scenario_type(s->script)));
        cJSON_AddBoolToObject(ligne, "utilisable", s->utilisable);
        cJSON_AddBoolToObject(ligne, "actif", s->actif);
        cJSON_AddBoolToObject(ligne, "en_cours", s->id == scenario_en_cours());
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


/* POST /api/scenarios/utilisable */
void scenario_utilisable(const Requete *req, Reponse *rep)
{
    cJSON *corps;
    const cJSON *id_json;
    const cJSON *choix;
    long long id;
    int utilisable;
    int trouve;
    ErreurLdsd err;
    char texte[LDSD_TAILLE_MESSAGE + 64];

    /* 1. Le corps : {"id":3,"utilisable":true} */
    corps = (req->corps != NULL) ? cJSON_Parse(req->corps) : NULL;
    id_json = cJSON_GetObjectItemCaseSensitive(corps, "id");
    choix = cJSON_GetObjectItemCaseSensitive(corps, "utilisable");
    if (!cJSON_IsNumber(id_json) || id_json->valuedouble < 1 || !cJSON_IsBool(choix)) {
        cJSON_Delete(corps);
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...,\"utilisable\":true|false}");
        return;
    }
    id = (long long)id_json->valuedouble;
    utilisable = cJSON_IsTrue(choix);
    cJSON_Delete(corps);

    /* 2. Rendre utilisable : seulement un texte correct */
    if (utilisable) {
        trouve = bdd_lire_scenario(connexion, id, &un_scenario);
        if (trouve != 0) {
            http_reponse_erreur(rep, trouve < 0 ? 500 : 404,
                                trouve < 0 ? "erreur de la base de données" : "scénario inconnu");
            return;
        }
        if (bdd_lire_maison(connexion, 0, &maison_lue) != 0) {
            http_reponse_erreur(rep, 500, "erreur de la base de données");
            return;
        }
        if (ldsd_lire(un_scenario.script, &maison_lue, &programme, &err) != 0) {
            snprintf(texte, sizeof texte, "erreur dans le scénario, ligne %d : %s",
                     err.ligne, err.message);
            http_reponse_erreur(rep, 409, texte);
            return;
        }
    } else {
        /* 3. Non utilisable : il ne doit plus tourner */
        execution_arreter_si(id, "arrêté (rendu non utilisable)");
    }

    /* 4. Enregistrer */
    trouve = bdd_ecrire_utilisable(connexion, id, utilisable);
    if (trouve != 0) {
        http_reponse_erreur(rep, trouve < 0 ? 500 : 404,
                            trouve < 0 ? "erreur de la base de données" : "scénario inconnu");
        return;
    }
    printf("[scénario] n° %lld rendu %s par l'utilisateur %lld\n", id,
           utilisable ? "utilisable" : "non utilisable", req->session->id_utilisateur);

    snprintf(texte, sizeof texte, "{\"ok\":true,\"id\":%lld,\"utilisable\":%s}",
             id, utilisable ? "true" : "false");
    http_reponse_json(rep, 200, texte);
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

/* Le texte reçu est-il correct ? (choix : on n'enregistre pas un texte faux)
   Renvoie 0 si oui ; sinon -1 et la réponse est remplie :
     409 {"erreur":"ligne 4 : ...","ligne":4}   texte faux
     500                                         base inaccessible */
static int texte_correct(Reponse *rep)
{
    ErreurLdsd err;
    cJSON *objet;
    char texte[LDSD_TAILLE_MESSAGE + 32];

    if (bdd_lire_maison(connexion, 0, &maison_lue) != 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return -1;
    }
    if (ldsd_lire(texte_recu, &maison_lue, &programme, &err) == 0) {
        return 0;
    }

    /* Le numéro de ligne est aussi envoyé à part : la page peut s'en servir */
    snprintf(texte, sizeof texte, "ligne %d : %s", err.ligne, err.message);
    objet = cJSON_CreateObject();
    cJSON_AddStringToObject(objet, "erreur", texte);
    cJSON_AddNumberToObject(objet, "ligne", err.ligne);
    http_reponse_liberer(rep);
    rep->code = 409;
    rep->json = cJSON_PrintUnformatted(objet);
    cJSON_Delete(objet);
    return -1;
}


/* POST /api/scenarios/creer */
void scenario_creer(const Requete *req, Reponse *rep)
{
    char nom[NOM_TAILLE_MAX + 1];
    char texte[64];
    long long id = 0;
    int resultat;

    if (lire_formulaire(req, rep, 0, NULL, nom) != 0
        || texte_correct(rep) != 0) {
        return;                                 /* réponse d'erreur déjà prête */
    }

    /* Le créateur est celui qui est connecté : lu dans sa session,
       jamais envoyé par la page (modifiable avec F12). */
    resultat = bdd_creer_scenario(connexion, nom, texte_recu,
                                  req->session->id_utilisateur, &id);
    if (resultat < 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }
    if (resultat == 1) {
        http_reponse_erreur(rep, 409, "ce nom est déjà pris par un autre scénario");
        return;
    }
    printf("[scénario] n° %lld « %s » créé par l'utilisateur %lld\n",
           id, nom, req->session->id_utilisateur);

    snprintf(texte, sizeof texte, "{\"ok\":true,\"id\":%lld}", id);
    http_reponse_json(rep, 200, texte);
}


/* POST /api/scenarios/modifier */
void scenario_modifier(const Requete *req, Reponse *rep)
{
    char nom[NOM_TAILLE_MAX + 1];
    char texte[64];
    long long id = 0;
    int resultat;

    if (lire_formulaire(req, rep, 1, &id, nom) != 0) {
        return;
    }

    /* 1. Le scénario existe-t-il ? */
    resultat = bdd_lire_scenario(connexion, id, &un_scenario);
    if (resultat != 0) {
        http_reponse_erreur(rep, resultat < 0 ? 500 : 404,
                            resultat < 0 ? "erreur de la base de données" : "scénario inconnu");
        return;
    }

    /* 2. Choix A : pas de modification pendant qu'il tourne */
    if (scenario_en_cours() == id) {
        http_reponse_erreur(rep, 409, "scénario en cours : arrêtez-le avant de le modifier");
        return;
    }

    /* 3. Le texte, puis l'enregistrement */
    if (texte_correct(rep) != 0) {
        return;
    }
    resultat = bdd_modifier_scenario(connexion, id, nom, texte_recu);
    if (resultat < 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }
    if (resultat == 1) {
        /* Il existe (étape 1) : c'est donc le nom qui est pris */
        http_reponse_erreur(rep, 409, "ce nom est déjà pris par un autre scénario");
        return;
    }
    printf("[scénario] n° %lld « %s » modifié par l'utilisateur %lld (repasse non utilisable)\n",
           id, nom, req->session->id_utilisateur);

    snprintf(texte, sizeof texte, "{\"ok\":true,\"id\":%lld}", id);
    http_reponse_json(rep, 200, texte);
}
