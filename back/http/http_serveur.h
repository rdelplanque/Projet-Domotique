/* ============================================================
   http_serveur.h – Le « guichet » HTTP du serveur R-Domotik
   Ouvre le port de l'API et attend les clients (nginx) en boucle.
   ============================================================ */

#ifndef HTTP_SERVEUR_H
#define HTTP_SERVEUR_H

/* Ouvre le guichet : réserve le port et se met en écoute.
   Renvoie le numéro de la prise d'écoute (un int, comme un fichier ouvert),
   ou -1 en cas d'échec (message déjà affiché). */
int http_ouvrir(int port);

/* Une « tâche périodique » : l'adresse d'une fonction sans paramètre,
   que la boucle appelle régulièrement (ex. maison_synchroniser). */
typedef void (*TachePeriodique)(void);

/* Boucle sans fin : attendre un client, lire, répondre, fermer, recommencer.
   Toutes les intervalle_s secondes, appelle aussi tache (la « ronde »).
   tache = NULL : pas de ronde.
   Ne revient jamais (on arrête le serveur avec Ctrl+C). */
void http_boucle(int ecoute, int intervalle_s, TachePeriodique tache);

#endif /* HTTP_SERVEUR_H */