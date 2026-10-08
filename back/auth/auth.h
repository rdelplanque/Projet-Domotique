/* ============================================================
   auth.h – Authentification : login (et plus tard droits, logout)
   ============================================================ */

#ifndef AUTH_H
#define AUTH_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* À appeler une fois au démarrage (dans main) : donne au module
   la connexion à la base dont il a besoin pour vérifier les comptes. */
void auth_initialiser(PGconn *conn);

/* POST /api/login
   Corps attendu : {"email":"...","mot_de_passe":"..."}
   Réponses :
     200 {"ok":true,"jeton":"...","profil":"...","prenom":"...","nom":"..."}
     400 corps absent, JSON invalide ou champs manquants
     401 e-mail ou mot de passe incorrect
     403 compte expiré
     500 / 503 problème côté serveur */
void auth_login(const Requete *req, Reponse *rep);

#endif /* AUTH_H */