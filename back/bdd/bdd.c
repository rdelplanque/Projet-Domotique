/* ============================================================
   bdd.c – Module base de données du serveur R-Domotik
   Pour l'instant : connexion + vérification au démarrage.
   Plus tard : requêtes du login, des équipements, de l'historique...
   ============================================================ */

#include <stdio.h>          /* fprintf, snprintf */
#include <string.h>         /* strcmp            */

#include "bdd.h"

#define PREFIXE "[bdd] "

/* Les 11 tables créées par sql/01_creation_tables.sql.
   NULL à la fin = marque de fin de liste (comme argv[argc]). */
static const char *TABLES[] = {
    "type_utilisateur", "utilisateur", "batiment", "localisation",
    "piece", "acces_piece", "type_equipement", "ip_equipement",
    "equipement", "historique_etat", "scenario",
    NULL
};


/* ------------------------------------------------------------
   Outil interne : exécute une requête qui renvoie UNE valeur
   vrai/faux ('t' ou 'f' pour PostgreSQL), avec UN paramètre $1.
   Renvoie 1 (vrai), 0 (faux) ou -1 (erreur SQL).
   ------------------------------------------------------------ */
static int requete_vrai_faux(PGconn *conn, const char *sql, const char *param)
{
    PGresult *res;
    int reponse;

    /* PQexecParams : le texte SQL et la valeur de $1 partent SÉPARÉMENT.
       PostgreSQL ne mélange jamais les deux : c'est la protection
       contre l'injection SQL (on l'utilisera pour l'e-mail du login). */
    res = PQexecParams(conn, sql,
                       1,           /* nombre de paramètres ($1)          */
                       NULL,        /* types : PostgreSQL les devine      */
                       &param,      /* valeurs des paramètres (tableau)   */
                       NULL, NULL,  /* longueurs, formats : texte simple  */
                       0);          /* résultat en texte                  */

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, PREFIXE "erreur SQL : %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    /* Ligne 0, colonne 0 : la seule valeur du résultat, "t" ou "f" */
    reponse = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    PQclear(res);                   /* chaque résultat doit être libéré */
    return reponse;
}


/* ------------------------------------------------------------
   Fonctions publiques (promises dans bdd.h)
   ------------------------------------------------------------ */

PGconn *bdd_connecter(const Config *cfg)
{
    PGconn *conn;
    char port[6];                   /* "65535" + '\0' */

    /* libpq attend tous les réglages sous forme de texte */
    snprintf(port, sizeof port, "%d", cfg->bdd_port);

    /* Deux tableaux qui vont ensemble, case par case :
       mots_cles[i] = nom du réglage, valeurs[i] = sa valeur.
       Plus sûr que de fabriquer une seule chaîne "host=... password=..."
       (un espace ou une apostrophe dans le mot de passe la casserait). */
    const char *mots_cles[] = {
        "host", "port", "dbname", "user", "password",
        "connect_timeout", "application_name", NULL
    };
    const char *valeurs[] = {
        cfg->bdd_hote, port, cfg->bdd_nom, cfg->bdd_utilisateur, cfg->bdd_mot_de_passe,
        "5",          /* abandon après 5 s si le serveur ne répond pas */
        "r-domotik",  /* nom visible côté PostgreSQL (pg_stat_activity) */
        NULL
    };

    conn = PQconnectdbParams(mots_cles, valeurs, 0);

    /* PQconnectdbParams renvoie TOUJOURS une connexion (sauf mémoire pleine) :
       il faut regarder son état pour savoir si elle a réussi. */
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, PREFIXE "connexion impossible à %s@%s:%d/%s\n",
                cfg->bdd_utilisateur, cfg->bdd_hote, cfg->bdd_port, cfg->bdd_nom);
        if (conn != NULL) {
            fprintf(stderr, PREFIXE "%s", PQerrorMessage(conn));
            PQfinish(conn);         /* même ratée, elle doit être libérée */
        }
        return NULL;
    }

    printf(PREFIXE "connecté à %s@%s:%d/%s\n",
           cfg->bdd_utilisateur, cfg->bdd_hote, cfg->bdd_port, cfg->bdd_nom);
    return conn;
}

int bdd_verifier(PGconn *conn)
{
    int manquants = 0;
    int i;
    int existe;

    /* 1. Les tables. to_regclass('nom') renvoie NULL si la table n'existe pas. */
    for (i = 0; TABLES[i] != NULL; i++) {
        existe = requete_vrai_faux(conn,
                                   "SELECT to_regclass($1) IS NOT NULL",
                                   TABLES[i]);
        if (existe == -1) {
            return -1;              /* erreur SQL : inutile de continuer */
        }
        if (existe == 0) {
            fprintf(stderr, PREFIXE "table manquante : %s\n", TABLES[i]);
            manquants++;
        }
    }

    /* 2. L'extension pgcrypto : indispensable au login (fonction crypt()) */
    existe = requete_vrai_faux(conn,
                               "SELECT EXISTS (SELECT 1 FROM pg_extension WHERE extname = $1)",
                               "pgcrypto");
    if (existe == -1) {
        return -1;
    }
    if (existe == 0) {
        fprintf(stderr, PREFIXE "extension manquante : pgcrypto\n");
        manquants++;
    }

    if (manquants > 0) {
        fprintf(stderr, PREFIXE "%d élément(s) manquant(s) : lancer les scripts de sql/\n",
                manquants);
        return -1;
    }

    printf(PREFIXE "%d tables et pgcrypto présents\n", i);
    return 0;
}

void bdd_fermer(PGconn *conn)
{
    if (conn != NULL) {
        PQfinish(conn);
    }
}