/* ============================================================
   http_reponse.c – Préparation et envoi d'une réponse HTTP

   Ce qui part vers le client (chaque ligne finit par \r\n) :
     HTTP/1.1 200 OK                   ← ligne de statut
     Content-Type: application/json    ← en-têtes
     Content-Length: 11
                                       ← ligne vide
     {"ok":true}                       ← corps
   ============================================================ */

#include <stdio.h>          /* snprintf, perror   */
#include <stdlib.h>         /* malloc, free       */
#include <string.h>         /* strlen, memcpy     */
#include <sys/socket.h>     /* send               */
#include <cjson/cJSON.h>    /* pour fabriquer {"erreur":...} proprement */

#include "http_reponse.h"

#define PREFIXE "[http] "

/* Réponse de secours si la fiche est vide (oubli ou mémoire pleine) */
#define JSON_ERREUR_INTERNE "{\"erreur\":\"erreur interne du serveur\"}"


/* ------------------------------------------------------------
   Outils internes
   ------------------------------------------------------------ */

/* Le petit texte qui accompagne chaque code dans la ligne de statut */
static const char *message_du_code(int code)
{
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

/* send peut n'envoyer qu'une PARTIE des octets demandés :
   on recommence jusqu'à ce que tout soit parti. */
static int envoyer_tout(int client, const char *donnees, size_t taille)
{
    size_t envoyes = 0;
    long n;

    while (envoyes < taille) {
        n = (long)send(client, donnees + envoyes, taille - envoyes, 0);
        if (n <= 0) {
            return -1;
        }
        envoyes += (size_t)n;
    }
    return 0;
}


/* ------------------------------------------------------------
   Fonctions publiques
   ------------------------------------------------------------ */

int http_reponse_json(Reponse *rep, int code, const char *json)
{
    size_t taille = strlen(json) + 1;           /* +1 pour le '\0' */

    http_reponse_liberer(rep);                  /* au cas où la fiche était déjà remplie */
    rep->code = code;
    rep->json = malloc(taille);
    if (rep->json == NULL) {
        return -1;
    }
    memcpy(rep->json, json, taille);
    return 0;
}

int http_reponse_erreur(Reponse *rep, int code, const char *message)
{
    cJSON *objet;

    http_reponse_liberer(rep);
    rep->code = code;

    /* On passe par cJSON plutôt que par snprintf : si le message contient
       un guillemet, cJSON l'échappe (\") et le JSON reste valide. */
    objet = cJSON_CreateObject();
    if (objet == NULL || cJSON_AddStringToObject(objet, "erreur", message) == NULL) {
        cJSON_Delete(objet);
        return -1;
    }
    rep->json = cJSON_PrintUnformatted(objet);  /* texte alloué par cJSON (malloc) */
    cJSON_Delete(objet);
    return rep->json != NULL ? 0 : -1;
}

int http_reponse_envoyer(int client, const Reponse *rep)
{
    char entetes[256];
    const char *json = rep->json;
    int code = rep->code;
    int n;

    if (json == NULL) {                         /* fiche vide : erreur 500 */
        json = JSON_ERREUR_INTERNE;
        code = 500;
    }

    n = snprintf(entetes, sizeof entetes,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: application/json; charset=utf-8\r\n"
                 "Content-Length: %zu\r\n"
                 "Cache-Control: no-store\r\n"     /* données de la maison : jamais en cache */
                 "Connection: close\r\n"
                 "\r\n",
                 code, message_du_code(code), strlen(json));

    if (envoyer_tout(client, entetes, (size_t)n) != 0
        || envoyer_tout(client, json, strlen(json)) != 0) {
        perror(PREFIXE "envoi de la réponse");
        return -1;
    }
    return 0;
}

void http_reponse_liberer(Reponse *rep)
{
    free(rep->json);        /* free(NULL) ne fait rien : pas besoin de tester */
    rep->json = NULL;
}