/* ============================================================
   bdd_utilisateur.h – Requêtes sur les utilisateurs
   ============================================================ */

#ifndef BDD_UTILISATEUR_H
#define BDD_UTILISATEUR_H

#include <libpq-fe.h>

/* Ce que la base renvoie sur un utilisateur reconnu */
typedef struct {
    long long id;           /* id_utilisateur                       */
    char prenom[101];       /* varchar(100) + '\0'                  */
    char nom[101];
    char profil[51];        /* nom_type_utilisateur : "Administrateur"... */
    int  expire;            /* 1 si date_expiration est dépassée    */
} Utilisateur;

/* Vérifie un e-mail et un mot de passe (comparaison faite par
   PostgreSQL avec crypt(), le C ne voit jamais l'empreinte).
   Renvoie 1 et remplit *u si les identifiants sont bons,
           0 s'ils sont faux (e-mail inconnu OU mauvais mot de passe),
          -1 en cas d'erreur de la base. */
int bdd_verifier_identifiants(PGconn *conn, const char *email,
                              const char *mot_de_passe, Utilisateur *u);

#endif /* BDD_UTILISATEUR_H */