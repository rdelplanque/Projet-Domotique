/* ============================================================
   bdd_scenario.c – Les scénarios dans la base (voir bdd_scenario.h)
   ============================================================ */

#include <stdio.h>          /* fprintf, snprintf */
#include <stdlib.h>         /* strtoll           */
#include <string.h>         /* strcmp            */

#include "bdd.h"            /* bdd_connexion_ok  */
#include "bdd_scenario.h"

#define PREFIXE "[bdd] "

/* Le créateur est retrouvé par la clé étrangère : « Prénom Nom » */
static const char *SQL_SCENARIOS =
    "SELECT s.id_scenario, s.nom_scenario, u.prenom || ' ' || u.nom, "
    "       s.script_ldsd, s.utilisable, s.actif "
    "FROM scenario s "
    "JOIN utilisateur u ON u.id_utilisateur = s.fk_id_utilisateur "
    "ORDER BY s.nom_scenario";

/* Le même, pour un seul scénario ($1 = son id) */
static const char *SQL_UN_SCENARIO =
    "SELECT s.id_scenario, s.nom_scenario, u.prenom || ' ' || u.nom, "
    "       s.script_ldsd, s.utilisable, s.actif "
    "FROM scenario s "
    "JOIN utilisateur u ON u.id_utilisateur = s.fk_id_utilisateur "
    "WHERE s.id_scenario = $1";


/* $1 = id, $2 = utilisable ('true' / 'false').
   actif AND $2 : rendu non utilisable → désactivé aussi. */
static const char *SQL_UTILISABLE =
    "UPDATE scenario SET utilisable = $2, actif = (actif AND $2) "
    "WHERE id_scenario = $1";


/* $1 = nom, $2 = texte, $3 = id du créateur.
   ON CONFLICT ... DO NOTHING : si le nom existe déjà, rien n'est inséré
   et RETURNING ne renvoie aucune ligne (pas d'erreur SQL à décoder). */
static const char *SQL_CREER =
    "INSERT INTO scenario (nom_scenario, script_ldsd, utilisable, actif, fk_id_utilisateur) "
    "VALUES ($1, $2, false, false, $3) "
    "ON CONFLICT (nom_scenario) DO NOTHING "
    "RETURNING id_scenario";

/* $1 = id, $2 = nom, $3 = texte.
   NOT EXISTS : on ne touche à rien si un AUTRE scénario porte déjà ce nom
   (0 ligne modifiée au lieu d'une erreur « UNIQUE »). */
static const char *SQL_MODIFIER =
    "UPDATE scenario SET nom_scenario = $2, script_ldsd = $3, "
    "       utilisable = false, actif = false "
    "WHERE id_scenario = $1 "
    "  AND NOT EXISTS (SELECT 1 FROM scenario autre "
    "                  WHERE autre.nom_scenario = $2 AND autre.id_scenario <> $1)";


/* $1 = id, $2 = actif ('true' / 'false').
   (utilisable OR NOT $2) : on n'active jamais un scénario non utilisable. */
static const char *SQL_ACTIF =
    "UPDATE scenario SET actif = $2 "
    "WHERE id_scenario = $1 AND (utilisable OR NOT $2)";

/* $1 = id */
static const char *SQL_SUPPRIMER =
    "DELETE FROM scenario WHERE id_scenario = $1";


/* Range la ligne i du résultat dans *s (colonnes dans l'ordre des SELECT) */
static void remplir(const PGresult *res, int i, Scenario *s)
{
    s->id = strtoll(PQgetvalue(res, i, 0), NULL, 10);
    snprintf(s->nom, sizeof s->nom, "%s", PQgetvalue(res, i, 1));
    snprintf(s->createur, sizeof s->createur, "%s", PQgetvalue(res, i, 2));
    snprintf(s->script, sizeof s->script, "%s", PQgetvalue(res, i, 3));
    s->utilisable = (strcmp(PQgetvalue(res, i, 4), "t") == 0);
    s->actif      = (strcmp(PQgetvalue(res, i, 5), "t") == 0);
}


int bdd_lire_scenarios(PGconn *conn, ListeScenarios *l)
{
    PGresult *res;
    int i;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }

    res = PQexecParams(conn, SQL_SCENARIOS, 0, NULL, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    l->nb = PQntuples(res);
    if (l->nb > SCENARIO_MAX) {
        fprintf(stderr, PREFIXE "%d scénarios : seuls les %d premiers sont lus\n",
                l->nb, SCENARIO_MAX);
        l->nb = SCENARIO_MAX;
    }

    for (i = 0; i < l->nb; i++) {
        remplir(res, i, &l->scenarios[i]);
    }
    PQclear(res);
    return 0;
}

int bdd_lire_scenario(PGconn *conn, long long id, Scenario *s)
{
    PGresult *res;
    char id_texte[24];
    const char *param = id_texte;
    int trouve;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }
    snprintf(id_texte, sizeof id_texte, "%lld", id);
    res = PQexecParams(conn, SQL_UN_SCENARIO, 1, NULL, &param, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    trouve = (PQntuples(res) == 1);
    if (trouve) {
        remplir(res, 0, s);
    }
    PQclear(res);
    return trouve ? 0 : 1;
}

int bdd_ecrire_utilisable(PGconn *conn, long long id, int utilisable)
{
    PGresult *res;
    char id_texte[24];
    const char *params[2];
    int modifie;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }
    snprintf(id_texte, sizeof id_texte, "%lld", id);
    params[0] = id_texte;
    params[1] = utilisable ? "true" : "false";

    res = PQexecParams(conn, SQL_UTILISABLE, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    /* PQcmdTuples : nombre de lignes modifiées, en texte ("1" ou "0") */
    modifie = (strcmp(PQcmdTuples(res), "1") == 0);
    PQclear(res);
    return modifie ? 0 : 1;
}

int bdd_creer_scenario(PGconn *conn, const char *nom, const char *texte,
                       long long id_createur, long long *id_sortie)
{
    PGresult *res;
    char id_texte[24];
    const char *params[3];
    int cree;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }
    snprintf(id_texte, sizeof id_texte, "%lld", id_createur);
    params[0] = nom;
    params[1] = texte;
    params[2] = id_texte;

    res = PQexecParams(conn, SQL_CREER, 3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {     /* RETURNING : des lignes */
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    cree = (PQntuples(res) == 1);       /* 0 ligne : le nom était déjà pris */
    if (cree) {
        *id_sortie = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
    }
    PQclear(res);
    return cree ? 0 : 1;
}

int bdd_modifier_scenario(PGconn *conn, long long id, const char *nom, const char *texte)
{
    PGresult *res;
    char id_texte[24];
    const char *params[3];
    int modifie;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }
    snprintf(id_texte, sizeof id_texte, "%lld", id);
    params[0] = id_texte;
    params[1] = nom;
    params[2] = texte;

    res = PQexecParams(conn, SQL_MODIFIER, 3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    modifie = (strcmp(PQcmdTuples(res), "1") == 0);
    PQclear(res);
    return modifie ? 0 : 1;
}


/* Exécute une requête de modification (UPDATE, DELETE) avec ses paramètres.
   Renvoie 0 si une ligne a été touchée, 1 si aucune, -1 en cas d'erreur. */
static int modifier_une_ligne(PGconn *conn, const char *sql, int nb, const char *const *params)
{
    PGresult *res;
    int modifie;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }
    res = PQexecParams(conn, sql, nb, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    /* PQcmdTuples : nombre de lignes touchées, en texte ("1" ou "0") */
    modifie = (strcmp(PQcmdTuples(res), "1") == 0);
    PQclear(res);
    return modifie ? 0 : 1;
}

int bdd_ecrire_actif(PGconn *conn, long long id, int actif)
{
    char id_texte[24];
    const char *params[2];

    snprintf(id_texte, sizeof id_texte, "%lld", id);
    params[0] = id_texte;
    params[1] = actif ? "true" : "false";
    return modifier_une_ligne(conn, SQL_ACTIF, 2, params);
}

int bdd_supprimer_scenario(PGconn *conn, long long id)
{
    char id_texte[24];
    const char *params[1];

    snprintf(id_texte, sizeof id_texte, "%lld", id);
    params[0] = id_texte;
    return modifier_une_ligne(conn, SQL_SUPPRIMER, 1, params);
}
