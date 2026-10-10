/* ============================================================
   auth.h – Authentification : login, jeton, logout (droits : plus tard)
   ============================================================ */

#ifndef AUTH_H
#define AUTH_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"
#include "session.h"

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

/* Lit l'en-tête « Authorization: Bearer <jeton> » de la requête
   et renvoie la session correspondante.
   NULL si pas d'en-tête, mauvais format, jeton inconnu ou expiré.
   (Appelée par route pour toutes les routes PROTÉGÉES.) */
const Session *auth_session(const Requete *req);

/* POST /api/logout   (route protégée : req->session est rempli)
   Ferme la session : le jeton ne vaudra plus rien.
   Réponse : 200 {"ok":true} */
void auth_logout(const Requete *req, Reponse *rep);

/* GET /api/moi   (route protégée)
   Dit au navigateur qui est connecté, d'après le jeton.
   Réponse : 200 {"ok":true,"id":...,"profil":"..."} */
void auth_moi(const Requete *req, Reponse *rep);

#endif /* AUTH_H */