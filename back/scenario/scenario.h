/* ============================================================
   scenario.h – Les routes /api/scenarios/...

   Le module scénarios (dossier scenario/) :
     scenario.c      les routes : HTTP seulement (JSON reçu → service → réponse)
     service.c       les règles (peut-on modifier, supprimer, activer... ?)
                     + l'entrée dans main : scenario_initialiser, scenario_avancer
     planificateur.c les programmés : minute par minute, file d'attente
     execution.c     le moteur : exécute UN scénario à la fois, par tranches
     ldsd.c          le langage : texte → Programme (en-tête + actions)
   Toutes les routes sont protégées (droit « scenarios », sauf mention).
   Erreurs communes : 400 corps invalide, 404 scénario inconnu, 500 base
   inaccessible ; texte faux : 409 {"erreur":"ligne 4 : ...","ligne":4}.
   ============================================================ */

#ifndef SCENARIO_H
#define SCENARIO_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* Appelée par scenario_initialiser (service.c) : la connexion à la base. */
void scenario_routes_initialiser(PGconn *conn);

/* GET /api/scenarios   (route protégée, droit « scenarios »)
   Réponse 200 :
   {"ok":true,
    "scenarios":[{"id":1,"nom":"Soirée salon","createur":"Admin Test",
                  "type":"direct","utilisable":true,"actif":false,
                  "quand":"","en_cours":false,"en_attente":false,"script":"En-tête :\nau clic\n..."}, ...]}
   type : "direct", "programme" ou "inconnu" (en-tête illisible).
   quand : l'en-tête en clair pour un programmé (« chaque samedi à 08:00 »), "" sinon.
   en_cours : true pour le scénario qui tourne (direct ou programmé).
   en_attente : true pour un programmé dans la file d'attente.
   Erreur : 500 si la base ne répond pas. */
void scenario_lister(const Requete *req, Reponse *rep);

/* POST /api/scenarios/verifier   (route protégée, droit « scenarios »)
   Corps : {"texte":"En-tête :\nau clic\nListe :\n..."}
   Réponses 200 (la requête est correcte, le SCÉNARIO l'est ou non) :
     {"ok":true,"type":"direct",
      "lecture":["au clic","ligne 4 : fermer 2 volets dans \"Salon\"", ...]}
     {"ok":false,"ligne":4,"erreur":"« de » attendu après \"Applique sud\""}
   400 corps invalide ; 500 base inaccessible. */
void scenario_verifier(const Requete *req, Reponse *rep);

/* POST /api/scenarios/utilisable   (route protégée, droit « scenarios_admin »)
   Corps : {"id":3,"utilisable":true}
   Rendre utilisable : seulement si le texte est correct (relu par ldsd_lire).
   Rendre non utilisable : le scénario est aussi désactivé, et arrêté s'il tourne.
   Réponses :
     200 {"ok":true,"id":3,"utilisable":true}
     400 corps invalide ; 404 scénario inconnu ; 500 base inaccessible
     409 {"erreur":"erreur dans le scénario, ligne 4 : ..."} (reste non utilisable) */
void scenario_utilisable(const Requete *req, Reponse *rep);

/* POST /api/scenarios/creer   (route protégée, droit « scenarios »)
   Corps : {"nom":"Soirée cinéma","texte":"En-tête :\nau clic\nListe :\n..."}
   Le créateur est l'utilisateur connecté (sa session). Le texte doit être
   correct (relu par ldsd_lire) ; le scénario est créé non utilisable.
   Réponses :
     200 {"ok":true,"id":12}
     400 corps invalide, nom vide, trop long ou sur plusieurs lignes, texte trop long
     409 {"erreur":"ligne 4 : ...","ligne":4}  texte faux
     409 {"erreur":"ce nom est déjà pris par un autre scénario"}
     500 base inaccessible */
void scenario_creer(const Requete *req, Reponse *rep);

/* POST /api/scenarios/modifier   (route protégée, droit « scenarios »)
   Corps : {"id":12,"nom":"...","texte":"..."}
   Mêmes contrôles que creer. Le scénario redevient non utilisable et
   inactif : l'administrateur le revalide.
   Réponses : celles de creer, plus
     404 scénario inconnu
     409 {"erreur":"scénario en cours : arrêtez-le avant de le modifier"} */
void scenario_modifier(const Requete *req, Reponse *rep);

/* POST /api/scenarios/actif   (droit « scenarios »)
   Corps : {"id":3,"actif":true}
   Activer : programmé, utilisable, texte correct ; « une fois » : heure pas passée.
   Désactiver : il sort de la file d'attente (s'il tourne, il va au bout).
   Réponses : 200 {"ok":true,"id":3} ; 409 {"erreur":"..."} refusé. */
void scenario_actif(const Requete *req, Reponse *rep);

/* POST /api/scenarios/lancer   (droit « scenarios »)
   Corps : {"id":3}   Scénario direct et utilisable.
   Réponses : 200 {"ok":true,"id":3} ;
     409 un scénario (direct ou programmé) est déjà en cours / non utilisable /
         programmé / erreur dans le texte ;
     503 maison non connectée. */
void scenario_lancer(const Requete *req, Reponse *rep);

/* POST /api/scenarios/arreter  (droit « scenarios »)
   Corps : {"id":3}   Arrête le scénario s'il tourne (direct ou programmé).
   Réponses : 200 {"ok":true,"id":3} ; 409 ce scénario n'est pas en cours. */
void scenario_arreter(const Requete *req, Reponse *rep);

/* POST /api/scenarios/supprimer  (droit « scenarios_admin »)
   Corps : {"id":3}
   Réponses : 200 {"ok":true,"id":3} ; 409 scénario en cours (l'arrêter d'abord). */
void scenario_supprimer(const Requete *req, Reponse *rep);

#endif /* SCENARIO_H */
