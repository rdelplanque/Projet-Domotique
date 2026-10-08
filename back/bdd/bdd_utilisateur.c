/* ============================================================
   bdd_utilisateur.c – Requêtes sur les utilisateurs
   ============================================================ */

#include <stdio.h>          /* fprintf, snprintf */
#include <stdlib.h>         /* strtoll           */
#include <string.h>         /* strcmp            */

#include "bdd_utilisateur.h"

#define PREFIXE "[bdd] "

/* $1 = e-mail, $2 = mot de passe tapé.
   crypt($2, password_hash) recalcule l'empreinte du mot de passe tapé
   avec le même « sel » que l'empreinte enregistrée : si les deux sont
   égales, le mot de passe est bon. lower() : majuscules ignorées dans l'e-mail.
   La colonne expire est calculée par PostgreSQL (horloge de la base). */
static const char *SQL_IDENTIFIANTS =
    "SELECT u.id_utilisateur, u.prenom, u.nom, t.nom_type_utilisateur, "
    "       (u.date_expiration IS NOT NULL AND u.date_expiration <= now()) "
    "FROM utilisateur u "
    "JOIN type_utilisateur t ON t.id_type_utilisateur = u.fk_id_type_utilisateur "
    "WHERE lower(u.email) = lower($1) "
    "  AND u.password_hash = crypt($2, u.password_hash)";

/* Si PostgreSQL a été redémarré, la connexion est cassée :
   on essaie de la rétablir une fois avant d'abandonner. */
static int connexion_ok(PGconn *conn)
{
    if (PQstatus(conn) == CONNECTION_OK) {
        return 1;
    }
    fprintf(stderr, PREFIXE "connexion perdue, tentative de reconnexion...\n");
    PQreset(conn);
    return PQstatus(conn) == CONNECTION_OK;
}

int bdd_verifier_identifiants(PGconn *conn, const char *email,
                              const char *mot_de_passe, Utilisateur *u)
{
    const char *params[2];
    PGresult *res;

    if (!connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }

    /* Requête paramétrée : l'e-mail et le mot de passe partent À PART
       du texte SQL → aucune injection possible (' OR 1=1 -- ne fait rien). */
    params[0] = email;
    params[1] = mot_de_passe;
    res = PQexecParams(conn, SQL_IDENTIFIANTS, 2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    /* 0 ligne = e-mail inconnu ou mauvais mot de passe (on ne sait pas
       lequel, et c'est voulu : on ne le dira pas au client non plus). */
    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }

    /* Ligne 0 : colonnes dans l'ordre du SELECT. Tout arrive en texte. */
    u->id = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
    snprintf(u->prenom, sizeof u->prenom, "%s", PQgetvalue(res, 0, 1));
    snprintf(u->nom,    sizeof u->nom,    "%s", PQgetvalue(res, 0, 2));
    snprintf(u->profil, sizeof u->profil, "%s", PQgetvalue(res, 0, 3));
    u->expire = (strcmp(PQgetvalue(res, 0, 4), "t") == 0);

    PQclear(res);
    return 1;
}