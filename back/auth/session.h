/* ============================================================
   session.h – Les sessions ouvertes (jetons), gardées en mémoire

   Après un login réussi, le serveur donne au navigateur un JETON :
   un long texte aléatoire, impossible à deviner. Le navigateur le
   renverra avec chaque requête suivante pour prouver qui il est
   (comme le bracelet d'un festival : on le montre à chaque entrée).
   ============================================================ */

#ifndef SESSION_H
#define SESSION_H

#include <time.h>           /* time_t */

#define SESSION_TAILLE_JETON 65         /* 64 caractères hexadécimaux + '\0' */
#define SESSION_MAX          64         /* sessions ouvertes en même temps   */
#define SESSION_DUREE_S      (8 * 3600) /* une session dure 8 heures         */

typedef struct {
    char      jeton[SESSION_TAILLE_JETON];  /* "" = case libre       */
    long long id_utilisateur;
    char      profil[51];                   /* pour vérifier les droits */
    time_t    expiration;                   /* date de fin           */
} Session;

/* Ouvre une session et copie le nouveau jeton dans jeton_sortie
   (SESSION_TAILLE_JETON caractères de place). Renvoie 0 ou -1. */
int session_creer(long long id_utilisateur, const char *profil, char *jeton_sortie);

/* Renvoie la session de ce jeton, ou NULL si inconnu ou expiré. */
const Session *session_trouver(const char *jeton);

/* Ferme la session de ce jeton (déconnexion). */
void session_supprimer(const char *jeton);

#endif /* SESSION_H */