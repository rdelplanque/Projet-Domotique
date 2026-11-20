/* ============================================================
   scenario.h – Les scénarios
    Permet la création des scenarii et enregistrement en BDD
    Permet la modification des scenarii.
    Permet la suppression des scenarii.
    Horloge interne du serveur pour lancement des scenarii périodiques.
   Pour l'instant : la liste (GET /api/scenarios), la vérification
   d'un texte LDSD (POST /api/scenarios/verifier, lecture dans ldsd.c),
   la création et la modification (éditeur scenario.html)
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

#endif /* SCENARIO_H */
