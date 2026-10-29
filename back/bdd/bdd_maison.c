/* ============================================================
   bdd_maison.c – Lecture de la maison dans la base
   Trois requêtes : les étages, les pièces, les équipements.
   Chaque ligne du résultat devient une case d'un tableau de *m.
   ============================================================ */

#include <stdio.h>          /* fprintf, snprintf */
#include <stdlib.h>         /* strtoll, atoi     */
#include <string.h>         /* strcmp            */

#include "bdd.h"            /* bdd_connexion_ok  */
#include "bdd_maison.h"

#define PREFIXE "[bdd] "

static const char *SQL_ETAGES =
    "SELECT id_localisation, nom_localisation "
    "FROM localisation "
    "ORDER BY id_localisation";

/* $1 = l'utilisateur connecté. EXISTS(...) vaut 't' si une ligne
   acces_piece autorise CET utilisateur dans CETTE pièce. */
static const char *SQL_PIECES =
    "SELECT p.id_piece, p.nom_piece, p.fk_id_localisation, "
    "       EXISTS (SELECT 1 FROM acces_piece a "
    "               WHERE a.fk_id_piece = p.id_piece "
    "                 AND a.fk_id_utilisateur = $1) "
    "FROM piece p "
    "ORDER BY p.id_piece";

/* host() : l'adresse inet en texte, sans le « /32 » */
static const char *SQL_EQUIPEMENTS =
    "SELECT e.id_equipement, e.nom_equipement, t.nom_type_equipement, "
    "       e.fk_id_piece, e.etat, e.mode, "
    "       host(i.ip_equipement), e.numero_equipement "
    "FROM equipement e "
    "JOIN type_equipement t ON t.id_type_equipement = e.fk_id_type_equipement "
    "JOIN ip_equipement i ON i.id_ip_equipement = e.fk_id_ip_equipement "
    "ORDER BY e.id_equipement";

/* $1 = id, $2 = état, $3 = mode ($2 ou $3 absents = NULL) */
static const char *SQL_ECRIRE_ETAT =
    "UPDATE equipement SET etat = $2, mode = $3 WHERE id_equipement = $1";


/* ------------------------------------------------------------
   Outils internes
   ------------------------------------------------------------ */

/* Lance une requête (avec 0 ou 1 paramètre) et vérifie qu'elle a réussi
   et qu'elle ne renvoie pas plus de max lignes.
   Renvoie le résultat (à libérer par PQclear), ou NULL en cas d'erreur. */
static PGresult *executer(PGconn *conn, const char *sql,
                          const char *param, int max)
{
    PGresult *res = PQexecParams(conn, sql, param ? 1 : 0, NULL,
                                 param ? &param : NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }
    if (PQntuples(res) > max) {
        fprintf(stderr, PREFIXE "trop de lignes (%d, maximum %d)\n",
                PQntuples(res), max);
        PQclear(res);
        return NULL;
    }
    return res;
}

/* Colonne qui peut être NULL (etat, mode) : NULL → VALEUR_ABSENTE.
   (PQgetvalue renverrait "" pour NULL : on doit demander PQgetisnull.) */
static int entier_ou_absent(const PGresult *res, int ligne, int colonne)
{
    if (PQgetisnull(res, ligne, colonne)) {
        return VALEUR_ABSENTE;
    }
    return atoi(PQgetvalue(res, ligne, colonne));
}


/* ------------------------------------------------------------
   Fonction publique
   ------------------------------------------------------------ */

int bdd_lire_maison(PGconn *conn, long long id_utilisateur, Maison *m)
{
    PGresult *res;
    char id_texte[24];          /* l'id en texte, pour le paramètre $1 */
    int i;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }

    /* 1. Les étages */
    res = executer(conn, SQL_ETAGES, NULL, MAISON_MAX_ETAGES);
    if (res == NULL) {
        return -1;
    }
    m->nb_etages = PQntuples(res);
    for (i = 0; i < m->nb_etages; i++) {
        m->etages[i].id = strtoll(PQgetvalue(res, i, 0), NULL, 10);
        snprintf(m->etages[i].nom, sizeof m->etages[i].nom, "%s", PQgetvalue(res, i, 1));
    }
    PQclear(res);

    /* 2. Les pièces (avec « autorisée pour cet utilisateur ? ») */
    snprintf(id_texte, sizeof id_texte, "%lld", id_utilisateur);
    res = executer(conn, SQL_PIECES, id_texte, MAISON_MAX_PIECES);
    if (res == NULL) {
        return -1;
    }
    m->nb_pieces = PQntuples(res);
    for (i = 0; i < m->nb_pieces; i++) {
        m->pieces[i].id        = strtoll(PQgetvalue(res, i, 0), NULL, 10);
        snprintf(m->pieces[i].nom, sizeof m->pieces[i].nom, "%s", PQgetvalue(res, i, 1));
        m->pieces[i].id_etage  = strtoll(PQgetvalue(res, i, 2), NULL, 10);
        m->pieces[i].autorisee = (strcmp(PQgetvalue(res, i, 3), "t") == 0);
    }
    PQclear(res);

    /* 3. Les équipements */
    res = executer(conn, SQL_EQUIPEMENTS, NULL, MAISON_MAX_EQUIPEMENTS);
    if (res == NULL) {
        return -1;
    }
    m->nb_equipements = PQntuples(res);
    for (i = 0; i < m->nb_equipements; i++) {
        Equipement *e = &m->equipements[i];     /* raccourci vers la case i */

        e->id       = strtoll(PQgetvalue(res, i, 0), NULL, 10);
        snprintf(e->nom,  sizeof e->nom,  "%s", PQgetvalue(res, i, 1));
        snprintf(e->type, sizeof e->type, "%s", PQgetvalue(res, i, 2));
        e->id_piece = strtoll(PQgetvalue(res, i, 3), NULL, 10);
        e->etat     = entier_ou_absent(res, i, 4);
        e->mode     = entier_ou_absent(res, i, 5);
        snprintf(e->ip,     sizeof e->ip,     "%s", PQgetvalue(res, i, 6));
        snprintf(e->entree, sizeof e->entree, "%s", PQgetvalue(res, i, 7));
    }
    PQclear(res);

    return 0;
}

int bdd_ecrire_etat(PGconn *conn, long long id_equipement, int etat, int mode)
{
    char id_texte[24], etat_texte[4], mode_texte[4];
    const char *params[3];
    PGresult *res;

    if (!bdd_connexion_ok(conn)) {
        fprintf(stderr, PREFIXE "base de données injoignable\n");
        return -1;
    }

    snprintf(id_texte, sizeof id_texte, "%lld", id_equipement);
    snprintf(etat_texte, sizeof etat_texte, "%d", etat);
    snprintf(mode_texte, sizeof mode_texte, "%d", mode);

    /* Un paramètre NULL (pointeur NULL) devient NULL en SQL */
    params[0] = id_texte;
    params[1] = (etat == VALEUR_ABSENTE) ? NULL : etat_texte;
    params[2] = (mode == VALEUR_ABSENTE) ? NULL : mode_texte;

    res = PQexecParams(conn, SQL_ECRIRE_ETAT, 3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    PQclear(res);
    return 0;
}
