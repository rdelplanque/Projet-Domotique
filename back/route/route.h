/* ============================================================
   route.h – L'aiguilleur de l'API R-Domotik
   Regarde la méthode et le chemin d'une requête, et appelle
   la fonction qui sait y répondre. Ne parle pas HTTP lui-même.
   ============================================================ */

#ifndef ROUTE_H
#define ROUTE_H

#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* Traite une requête correcte et remplit la réponse :
     route inconnue                          → 404 ;
     route connue + autre méthode            → 405 ;
     route PROTÉGÉE sans jeton valable       → 401 ;
     sinon                                   → la fonction de cette route.
   req n'est pas const : pour une route protégée, route y range la
   session trouvée (req->session) avant d'appeler la fonction. */
void route_traiter(Requete *req, Reponse *rep);

#endif /* ROUTE_H */