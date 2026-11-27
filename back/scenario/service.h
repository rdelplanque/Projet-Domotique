/* ============================================================
   service.h – Les règles des scénarios

   Le service décide : peut-on créer, modifier, supprimer, activer,
   lancer... ce scénario maintenant ? Il appelle la base, le moteur
   (execution.c) et le planificateur, et rend un Resultat.

   Il ne connaît pas HTTP : c'est scenario.c (les routes) qui lit le
   JSON reçu, appelle le service, puis traduit le Resultat en réponse
   (ISSUE_REFUS → 409, etc.). Une règle n'est donc écrite qu'à un
   seul endroit, même si plusieurs routes en ont besoin.

   C'est aussi l'entrée du module dans main : scenario_initialiser
   et scenario_avancer.
   ============================================================ */

#ifndef SERVICE_H
#define SERVICE_H

#include <libpq-fe.h>
#include "execution.h"      /* EXECUTION_TAILLE_MESSAGE */

/* Comment s'est passée la demande (et le code HTTP qu'en tirent les routes) */
typedef enum {
    ISSUE_OK,               /* 200 */
    ISSUE_INCONNU,          /* 404 scénario inconnu                          */
    ISSUE_REFUS,            /* 409 interdit maintenant (en cours, texte faux...) */
    ISSUE_BASE,             /* 500 base de données inaccessible              */
    ISSUE_MAISON            /* 503 SimDom non connecté                       */
} Issue;

typedef struct {
    Issue issue;
    int   ligne;            /* texte faux : numéro de la ligne fautive, sinon 0 */
    char  message[EXECUTION_TAILLE_MESSAGE];
} Resultat;


/* ---------- Entrée du module (main.c) ---------- */

/* Une fois au démarrage : donne la connexion au moteur, au planificateur... */
void scenario_initialiser(PGconn *conn);

/* Le travail de fond, appelé à chaque tour par http_boucle :
   le planificateur (minutes passées, file d'attente), puis le moteur.
   Renvoie 1 s'il reste du travail (un scénario tourne), 0 sinon. */
int scenario_avancer(void);


/* ---------- Les règles (qui = id de l'utilisateur connecté, pour le journal) ---------- */

/* Le texte doit être correct. Créé non utilisable et inactif. *id = son numéro. */
void service_creer(const char *nom, const char *texte, long long qui,
                   long long *id, Resultat *r);

/* Refusé s'il est en cours. Le texte doit être correct. Il redevient non
   utilisable et inactif, et sort de la file d'attente. */
void service_modifier(long long id, const char *nom, const char *texte, long long qui,
                      Resultat *r);

/* Admin. Rendre utilisable : seulement si le texte est correct.
   Non utilisable : désactivé, retiré de la file, arrêté s'il tourne. */
void service_utilisable(long long id, int utilisable, long long qui, Resultat *r);

/* Activer : programmé, utilisable, texte correct, et pour « une fois »
   une heure pas encore passée. Désactiver : sort de la file d'attente
   (s'il tourne déjà, il va jusqu'au bout ; « Arrêter » reste possible). */
void service_activer(long long id, int actif, long long qui, Resultat *r);

/* Admin. Refusé s'il est en cours. Sort de la file d'attente. */
void service_supprimer(long long id, long long qui, Resultat *r);

/* Scénario direct, utilisable ; refusé si un scénario (direct ou programmé)
   tourne déjà. */
void service_lancer(long long id, long long qui, Resultat *r);

/* Arrête le scénario id s'il est en cours (direct ou programmé). */
void service_arreter(long long id, long long qui, Resultat *r);

#endif /* SERVICE_H */
