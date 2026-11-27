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
    char      nom[101];                       /* nom_scenario (UNIQUE) : 100 octets + '\0' */
    char      createur[100];                  /* « Prénom Nom » du créateur */
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

/* Crée un scénario (non utilisable, inactif) au nom de l'utilisateur
   id_createur. Son numéro est rangé dans *id_sortie.
   Renvoie 0 si fait, 1 si le nom est déjà pris, -1 en cas d'erreur de la base. */
int bdd_creer_scenario(PGconn *conn, const char *nom, const char *texte,
                       long long id_createur, long long *id_sortie);

/* Remplace le nom et le texte du scénario id. Il redevient non utilisable
   et inactif : l'administrateur doit le revalider.
   Renvoie 0 si fait, 1 si aucun scénario n'a été modifié (id inconnu,
   ou nom déjà pris par un AUTRE scénario), -1 en cas d'erreur de la base. */
int bdd_modifier_scenario(PGconn *conn, long long id, const char *nom, const char *texte);

/* Active (1) ou désactive (0) un scénario programmé. On n'active
   qu'un scénario utilisable (contrôlé aussi dans la requête SQL).
   Renvoie 0 si fait, 1 si aucun scénario modifié (id inconnu, ou
   activation d'un scénario non utilisable), -1 en cas d'erreur de la base. */
int bdd_ecrire_actif(PGconn *conn, long long id, int actif);

/* Supprime le scénario id.
   Renvoie 0 si fait, 1 s'il n'existe pas, -1 en cas d'erreur de la base. */
int bdd_supprimer_scenario(PGconn *conn, long long id);

#endif /* BDD_SCENARIO_H */
