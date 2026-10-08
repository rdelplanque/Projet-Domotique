/* ============================================================
   auth.c – Authentification : login
   Le module qui DÉCIDE : il demande à bdd de vérifier les
   identifiants, puis accepte ou refuse, et crée la session.
   ============================================================ */

#include <stdio.h>          /* printf */
#include <string.h>         /* strlen */
#include <cjson/cJSON.h>

#include "auth.h"
#include "session.h"
#include "../bdd/bdd_utilisateur.h"

#define PREFIXE "[auth] "

/* Limites des champs (au-delà : refus avant même d'interroger la base) */
#define EMAIL_MAX         255   /* varchar(255) dans la table          */
#define MOT_DE_PASSE_MAX  72    /* bcrypt ignore au-delà de 72 octets  */

/* La connexion à la base, donnée une fois par main (auth_initialiser) */
static PGconn *connexion = NULL;


void auth_initialiser(PGconn *conn)
{
    connexion = conn;
}


/* Lit un champ texte non vide du JSON ; renvoie NULL s'il est absent,
   vide, pas un texte, ou plus long que max. */
static const char *champ_texte(const cJSON *json, const char *cle, size_t max)
{
    const cJSON *element = cJSON_GetObjectItemCaseSensitive(json, cle);
    size_t longueur;

    if (!cJSON_IsString(element)) {
        return NULL;
    }
    longueur = strlen(element->valuestring);
    if (longueur == 0 || longueur > max) {
        return NULL;
    }
    return element->valuestring;
}

/* Fabrique la réponse 200 du login avec cJSON */
static void repondre_succes(Reponse *rep, const char *jeton, const Utilisateur *u)
{
    cJSON *objet = cJSON_CreateObject();

    cJSON_AddBoolToObject(objet, "ok", 1);
    cJSON_AddStringToObject(objet, "jeton", jeton);
    cJSON_AddStringToObject(objet, "profil", u->profil);
    cJSON_AddStringToObject(objet, "prenom", u->prenom);
    cJSON_AddStringToObject(objet, "nom", u->nom);

    http_reponse_liberer(rep);                  /* fiche vide avant de la remplir */
    rep->code = 200;
    rep->json = cJSON_PrintUnformatted(objet);  /* NULL si mémoire pleine → 500 */
    cJSON_Delete(objet);
}


void auth_login(const Requete *req, Reponse *rep)
{
    cJSON *json;
    const char *email;
    const char *mot_de_passe;
    Utilisateur u;
    char jeton[SESSION_TAILLE_JETON];
    int resultat;

    /* 1. Le corps doit être un JSON avec email et mot_de_passe */
    if (req->corps == NULL) {
        http_reponse_erreur(rep, 400, "corps JSON attendu");
        return;
    }
    json = cJSON_Parse(req->corps);
    if (json == NULL) {
        http_reponse_erreur(rep, 400, "JSON invalide");
        return;
    }
    email = champ_texte(json, "email", EMAIL_MAX);
    mot_de_passe = champ_texte(json, "mot_de_passe", MOT_DE_PASSE_MAX);
    if (email == NULL || mot_de_passe == NULL) {
        cJSON_Delete(json);
        http_reponse_erreur(rep, 400, "email et mot_de_passe obligatoires");
        return;
    }

    /* 2. Demander à la base. email et mot_de_passe pointent DANS l'arbre
          json : on ne doit le libérer qu'APRÈS les avoir utilisés. */
    resultat = bdd_verifier_identifiants(connexion, email, mot_de_passe, &u);

    if (resultat == -1) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
    } else if (resultat == 0) {
        /* Même message que l'e-mail soit inconnu ou le mot de passe faux :
           on ne dit pas à un pirate quels e-mails existent. */
        printf(PREFIXE "échec de connexion : %s\n", email);
        http_reponse_erreur(rep, 401, "e-mail ou mot de passe incorrect");
    } else if (u.expire) {
        printf(PREFIXE "compte expiré : %s\n", email);
        http_reponse_erreur(rep, 403, "compte expiré");
    } else if (session_creer(u.id, u.profil, jeton) != 0) {
        http_reponse_erreur(rep, 503, "impossible de créer la session");
    } else {
        /* 3. Succès. On journalise l'e-mail, JAMAIS le mot de passe ni le jeton. */
        printf(PREFIXE "connexion réussie : %s (%s)\n", email, u.profil);
        repondre_succes(rep, jeton, &u);
    }

    cJSON_Delete(json);
}