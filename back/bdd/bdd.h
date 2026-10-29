/* ============================================================
   bdd.h – Module base de données du serveur R-Domotik
   Le SEUL module qui parle à PostgreSQL (bibliothèque libpq).
   ============================================================ */

#ifndef BDD_H
#define BDD_H

#include <libpq-fe.h>             /* PGconn : la connexion libpq    */
#include "../config/config.h"     /* Config : les réglages bdd_*    */

/* Ouvre la connexion avec les réglages de cfg (hôte, port, nom...).
   Renvoie la connexion (PGconn *, comme un FILE * pour un fichier),
   ou NULL si la connexion a échoué (message déjà affiché). */
PGconn *bdd_connecter(const Config *cfg);

/* Vérifie que les 11 tables du projet et l'extension pgcrypto existent.
   Renvoie 0 si tout est là, -1 sinon (les manques sont affichés). */
int bdd_verifier(PGconn *conn);

/* Avant chaque requête : la connexion marche-t-elle encore ?
   Si PostgreSQL a été redémarré, elle est cassée : on essaie de la
   rétablir une fois. Renvoie 1 si la connexion est utilisable, 0 sinon. */
int bdd_connexion_ok(PGconn *conn);

/* Ferme la connexion (comme fclose). Accepte NULL sans planter. */
void bdd_fermer(PGconn *conn);

#endif /* BDD_H */