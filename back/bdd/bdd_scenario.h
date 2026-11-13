/* ============================================================
   bdd_scenario.h – Les scénarios dans la base (table scenario)
   ============================================================ */

#ifndef BDD_SCENARIO_H
#define BDD_SCENARIO_H

#include <libpq-fe.h>

#define SCENARIO_MAX          64      /* scénarios lus au maximum           */
#define SCENARIO_TAILLE_TEXTE 8192    /* texte LDSD : 8191 octets au plus   */

/* Un scénario, tel que lu dans la base */
typedef struct {
    long long id;
    char      nom[101];                       /* nom_scenario (UNIQUE)      */
    char      createur[103];                  /* « Prénom Nom » du créateur */
    char      script[SCENARIO_TAILLE_TEXTE];  /* le texte LDSD              */
    int       utilisable;                     /* 1 / 0                      */
    int       actif;                          /* 1 / 0 (programmés)         */
} Scenario;

/* Tous les scénarios, triés par nom */
typedef struct {
    int      nb;
    Scenario scenarios[SCENARIO_MAX];
} ListeScenarios;

/* Remplit *l avec tous les scénarios de la base.
   Renvoie 0 si OK, -1 en cas d'erreur de la base. */
int bdd_lire_scenarios(PGconn *conn, ListeScenarios *l);

/* Lit le scénario d'id donné dans *s.
   Renvoie 0 si trouvé, 1 s'il n'existe pas, -1 en cas d'erreur de la base. */
int bdd_lire_scenario(PGconn *conn, long long id, Scenario *s);

/* Rend le scénario utilisable (1) ou non (0). Un scénario rendu non
   utilisable est aussi désactivé (actif = false).
   Renvoie 0 si fait, 1 s'il n'existe pas, -1 en cas d'erreur de la base. */
int bdd_ecrire_utilisable(PGconn *conn, long long id, int utilisable);

#endif /* BDD_SCENARIO_H */
