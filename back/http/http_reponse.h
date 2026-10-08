/* ============================================================
   http_reponse.h – Préparation et envoi d'une réponse HTTP
   Le module métier (via route) remplit la fiche Reponse ;
   http_reponse_envoyer la met « sous enveloppe » HTTP.
   ============================================================ */

#ifndef HTTP_REPONSE_H
#define HTTP_REPONSE_H

/* La fiche d'une réponse : un code HTTP et un texte JSON.
   json est alloué (malloc) : toujours finir par http_reponse_liberer. */
typedef struct {
    int   code;     /* 200, 400, 401, 404...      */
    char *json;     /* "{\"ok\":true}", ou NULL    */
} Reponse;

/* Remplit la réponse avec un code et une COPIE du texte JSON donné.
   Renvoie 0 si OK, -1 si mémoire insuffisante. */
int http_reponse_json(Reponse *rep, int code, const char *json);

/* Remplit une réponse d'erreur {"erreur":"message"} (guillemets échappés).
   Renvoie 0 si OK, -1 si mémoire insuffisante. */
int http_reponse_erreur(Reponse *rep, int code, const char *message);

/* Écrit la réponse HTTP complète sur la connexion client.
   Si la fiche est vide (json NULL), envoie une erreur 500.
   Renvoie 0 si OK, -1 si l'envoi a échoué (client parti). */
int http_reponse_envoyer(int client, const Reponse *rep);

/* Libère le JSON alloué (comme fclose pour un fichier). */
void http_reponse_liberer(Reponse *rep);

#endif /* HTTP_REPONSE_H */