/* ============================================================
   execution.h – Le moteur : exécute UN scénario à la fois

   Pas de thread : le scénario avance par petites tranches dans la
   boucle HTTP (comme la ronde). À chaque appel, execution_avancer
   commande quelques équipements (environ 0,25 s de travail), puis
   la boucle revient servir les pages.

   Le moteur ne connaît ni HTTP ni la planification : il est lancé
   par le service (bouton Lancer) ou par le planificateur (programmés).
   Un seul scénario tourne à la fois, direct ou programmé.
   ============================================================ */

#ifndef EXECUTION_H
#define EXECUTION_H

#include <stddef.h>                 /* size_t */
#include <libpq-fe.h>

/* Taille conseillée du message rempli par execution_lancer */
#define EXECUTION_TAILLE_MESSAGE 400

/* Ce qu'il est advenu d'une demande de lancement */
typedef enum {
    LANCEMENT_OK,
    LANCEMENT_OCCUPE,           /* un scénario tourne déjà                    */
    LANCEMENT_INCONNU,          /* pas de scénario avec cet id                */
    LANCEMENT_NON_UTILISABLE,
    LANCEMENT_MAUVAIS_TYPE,     /* direct attendu et programmé reçu, ou l'inverse */
    LANCEMENT_ERREUR_TEXTE,     /* le texte ne se lit plus (la maison a changé ?) */
    LANCEMENT_BASE,             /* base de données inaccessible               */
    LANCEMENT_MAISON            /* SimDom non connecté                        */
} Lancement;

/* Appelée une fois au démarrage (par scenario_initialiser). */
void execution_initialiser(PGconn *conn);

/* Lance le scénario id : il est relu dans la base et revérifié (ldsd_lire),
   car le texte a pu changer, la maison aussi.
     par_planificateur : 0 = bouton « Lancer » (il doit être direct),
                         1 = planificateur (il doit être programmé) ;
     qui       : pour le journal (« l'utilisateur 3 », « le planificateur ») ;
     message   : rempli en cas de refus (EXECUTION_TAILLE_MESSAGE octets).
   Le lancement ne fait que préparer : le travail est fait par execution_avancer. */
Lancement execution_lancer(long long id, int par_planificateur, const char *qui,
                           char *message, size_t taille);

/* Fait avancer le scénario en cours d'une petite tranche.
   Renvoie 1 s'il reste du travail, 0 sinon (rien en cours, ou fini). */
int execution_avancer(void);

/* Arrête le scénario id s'il est en cours. pourquoi : écrit dans le journal.
   Renvoie 1 s'il a été arrêté, 0 s'il n'était pas en cours. */
int execution_arreter_si(long long id, const char *pourquoi);

/* Id du scénario en cours, 0 si aucun. */
long long execution_en_cours(void);

/* Nom du scénario en cours ("" si aucun), pour les messages. */
const char *execution_nom_en_cours(void);

#endif /* EXECUTION_H */
