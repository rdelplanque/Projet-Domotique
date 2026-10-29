/* ============================================================
   maison.h – Pilotage direct du bâtiment (page « Ma maison »)
   Le module qui DÉCIDE ce qu'un utilisateur voit et peut piloter.
   Il demande les données à bdd, puis fabrique la réponse JSON.
   ============================================================ */

#ifndef MAISON_H
#define MAISON_H

#include <libpq-fe.h>
#include "../http/http_requete.h"
#include "../http/http_reponse.h"

/* À appeler une fois au démarrage (dans main), comme auth_initialiser. */
void maison_initialiser(PGconn *conn);

/* GET /api/maison   (route protégée, droit « consulter »)
   Réponse 200 :
   {"ok":true,
    "etages":[{"id":2,"nom":"Rez-de-chaussée"}, ...],
    "pieces":[{"id":23,"nom":"Salon","etage":2,"piloter":true}, ...],
    "equipements":[{"id":57,"nom":"Luminaire salon nord","type":"Luminaire",
                    "piece":23,"etat":1,"mode":null}, ...]}
   etat : 0 éteint, 1 allumé, 2 en panne, null indéterminé.
   mode : 0 climatisation, 1 chauffage, null si pas une climatisation.
   Un Invité ne reçoit que ses pièces autorisées (acces_piece), leurs
   équipements et les étages qui les contiennent ; les autres profils
   reçoivent toute la maison.
   piloter : true si CET utilisateur peut commander les équipements
             de la pièce (droit « piloter » et pièce visible).
   Paramètres facultatifs :
     ?piece=23   la pièce affichée par la page : la ronde la relit en premier ;
     &relire=1   (bouton Rafraîchir) relire cette pièce dans SimDom avant de répondre.
   Erreur : 500 si la base ne répond pas. */
void maison_lire(const Requete *req, Reponse *rep);

/* POST /api/commande   (route protégée, droit « piloter »)
   Corps : {"id":57,"etat":1}   etat 1 = allumer / monter, 0 = éteindre / baisser
           + "mode":0 ou 1 pour une climatisation (sinon le mode actuel est gardé).
   Relit l'état dans SimDom AVANT (panne ?) et APRÈS (confirmation),
   et enregistre l'état relu dans la base.
   Réponses :
     200 {"ok":true,"id":57,"etat":1,"mode":null}
     400 corps invalide
     403 pièce non pilotable par cet utilisateur
     404 équipement inconnu (ou dans une pièce invisible pour un Invité)
     409 équipement en panne : commande refusée
     502 commande non prise en compte par la maison
     503 maison non connectée / l'équipement ne répond pas */
void maison_commander(const Requete *req, Reponse *rep);

/* Lit dans SimDom l'état de tous les équipements et met la base à jour
   (seulement ce qui a changé). Pas de réponse = état NULL (indéterminé).
   Si SimDom n'est pas connecté : nouvelle tentative de connexion,
   sinon rien ne change.
   Appelée au démarrage (main). Dure 10 à 20 s avec SimDom. */
void maison_synchroniser(void);

/* La « ronde », appelée toutes les intervalle_relecture_s secondes par la
   boucle HTTP : relit une petite tranche d'équipements (la pièce affichée
   d'abord, puis la suite du tour de la maison) pour ne pas fermer le
   guichet longtemps. */
void maison_ronde(void);

#endif /* MAISON_H */
