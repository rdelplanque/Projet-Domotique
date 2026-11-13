/* ============================================================
   scenario.h – Les scénarios
    Permet la création des scenarii et enregistrement en BDD
    Permet la modification des scenarii.
    Permet la suppression des scenarii.
    Horloge interne du serveur pour lancement des scenarii périodiques.
   Pour l'instant : la liste (GET /api/scenarios), la vérification
   d'un texte LDSD (POST /api/scenarios/verifier, lecture dans ldsd.c)
   et le lancement d'un scénario direct (execution.c).
   ============================================================ */

#ifndef SCENARIO_H
#define SCENARIO_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* Types de scénario, d'après la ligne qui suit « En-tête : » */
typedef enum {
    SCENARIO_DIRECT,        /* « au clic »                         */
    SCENARIO_PROGRAMME,     /* « le … à … » ou « chaque … à … »    */
    SCENARIO_INCONNU        /* pas d'en-tête trouvé                */
} TypeScenario;

/* À appeler une fois au démarrage (dans main), comme maison_initialiser. */
void scenario_initialiser(PGconn *conn);

/* Lit le type dans le texte LDSD, sans analyser le reste :
   la première ligne non vide après « En-tête : ». */
TypeScenario scenario_type(const char *script);

/* GET /api/scenarios   (route protégée, droit « scenarios »)
   Réponse 200 :
   {"ok":true,
    "scenarios":[{"id":1,"nom":"Soirée salon","createur":"Admin Test",
                  "type":"direct","utilisable":true,"actif":false,
                  "en_cours":false,"script":"En-tête :\nau clic\n..."}, ...]}
   type : "direct", "programme" ou "inconnu".
   en_cours : true pour le scénario direct lancé (execution.c).
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

#endif /* SCENARIO_H */
