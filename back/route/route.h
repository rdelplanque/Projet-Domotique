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
     route connue + bonne méthode → la fonction de cette route ;
     route connue + autre méthode → 405 ;
     route inconnue              → 404. */
void route_traiter(const Requete *req, Reponse *rep);

#endif /* ROUTE_H */