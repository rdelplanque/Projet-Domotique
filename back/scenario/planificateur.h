/* ============================================================
   planificateur.h – Les scénarios programmés

   Garde en mémoire l'en-tête des scénarios programmés ACTIFS et
   UTILISABLES (« le 24/12/2026 à 19:00 », « chaque samedi à 08:00 »).
   Une fois par minute, il regarde lesquels doivent partir et les met
   dans une FILE D'ATTENTE. Dès que le moteur est libre (aucun scénario
   en cours, direct ou programmé), il lance le premier de la file.

   Règles :
     - un programmé dont l'heure arrive pendant un autre scénario
       attend la fin de celui-ci (file d'attente) ;
     - un programmé « une fois » est désactivé dès qu'il entre dans la file ;
     - un scénario retiré (désactivé, rendu non utilisable, modifié,
       supprimé) sort de la file ;
     - au lancement, le texte est relu et revérifié (execution_lancer) ;
     - exécution manquée (serveur arrêté à l'heure prévue) : abandonnée ;
     - changement d'heure été / hiver : non traité.
   ============================================================ */

#ifndef PLANIFICATEUR_H
#define PLANIFICATEUR_H

#include <libpq-fe.h>
#include "ldsd.h"           /* EnTete */

/* Au démarrage (par scenario_initialiser) : lit les programmés actifs.
   La minute en cours compte comme déjà vue : un serveur relancé à 08:00:30
   ne relance pas un scénario de 08:00 (il est peut-être déjà passé). */
void planificateur_initialiser(PGconn *conn);

/* Relit les programmés actifs dans la base. À appeler après tout ce qui
   change « actif » ou « utilisable » : activer, désactiver, modifier,
   supprimer, rendre (non) utilisable. Un « une fois » dont l'heure est
   passée est désactivé à ce moment-là (exécution manquée). */
void planificateur_recharger(void);

/* Retire le scénario id de la file d'attente (s'il y est). */
void planificateur_retirer(long long id);

/* 1 si le scénario id attend dans la file, 0 sinon. */
int planificateur_en_attente(long long id);

/* Un « une fois » : son heure est-elle déjà passée (minute en cours comprise) ?
   Sert au refus d'« Activer » d'un scénario qui ne partirait jamais. */
int planificateur_heure_passee(const EnTete *e);

/* Appelée à chaque tour de la boucle (par scenario_avancer) :
     1. si une ou plusieurs minutes ont passé, met dans la file les
        programmés de ces minutes ;
     2. si le moteur est libre, lance le premier de la file. */
void planificateur_avancer(void);

#endif /* PLANIFICATEUR_H */
