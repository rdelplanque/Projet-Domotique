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
