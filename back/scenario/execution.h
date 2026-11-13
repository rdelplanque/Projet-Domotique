/* ============================================================
   execution.h – Exécution d'un scénario direct (bouton Lancer)

   Pas de thread : le scénario avance par petites tranches dans la
   boucle HTTP (comme la ronde). À chaque tour, scenario_avancer
   commande quelques équipements (environ 0,25 s de travail), puis
   la boucle revient servir les pages. Un seul scénario direct à la fois.
   ============================================================ */

#ifndef EXECUTION_H
#define EXECUTION_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* Appelée par scenario_initialiser. */
void execution_initialiser(PGconn *conn);

/* POST /api/scenarios/lancer   (route protégée, droit « scenarios »)
   Corps : {"id":3}
   Le scénario est relu dans la base et revérifié (ldsd_lire) avant de partir.
   Réponses :
     200 {"ok":true,"id":3}
     400 corps invalide
     404 scénario inconnu
     409 déjà un scénario en cours / non utilisable / programmé / erreur dans le texte
     500 base inaccessible
     503 maison non connectée */
void scenario_lancer(const Requete *req, Reponse *rep);

/* POST /api/scenarios/arreter  (route protégée, droit « scenarios »)
   Corps : {"id":3}
   Réponses : 200 {"ok":true,"id":3} ; 400 ; 409 ce scénario n'est pas en cours. */
void scenario_arreter(const Requete *req, Reponse *rep);

/* Le travail en cours, appelé à chaque tour par http_boucle :
   fait avancer le scénario lancé d'une petite tranche.
   Renvoie 1 s'il reste du travail, 0 sinon. */
int scenario_avancer(void);

/* Arrête le scénario id s'il est en cours (ex. rendu non utilisable).
   pourquoi : écrit dans le journal. */
void execution_arreter_si(long long id, const char *pourquoi);

/* Id du scénario en cours, 0 si aucun (pour « en_cours » dans la liste). */
long long scenario_en_cours(void);

#endif /* EXECUTION_H */
