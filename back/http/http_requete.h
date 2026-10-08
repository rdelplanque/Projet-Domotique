/* ============================================================
   http_requete.h – Lecture et découpage d'une requête HTTP
   Ne vérifie que l'ENVELOPPE HTTP. Le corps (JSON) n'est pas lu :
   c'est le module qui le reçoit (auth...) qui le vérifiera.
   ============================================================ */

#ifndef HTTP_REQUETE_H
#define HTTP_REQUETE_H

#include <stddef.h>         /* size_t */

/* Taille maximale d'une requête complète (en-têtes + corps).
   Largement assez pour un login ou un scénario LDSD. */
#define HTTP_TAILLE_MAX 16384

/* La fiche d'une requête découpée.
   Tout le texte reçu est gardé dans tampon ; corps POINTE dedans
   (rien n'est recopié, comme champs[] dans lecture_equipement). */
typedef struct {
    char   methode[8];      /* "GET", "POST"...                       */
    char   chemin[256];     /* "/api/login" (sans la partie ?x=...)    */
    char  *corps;           /* début du corps dans tampon, NULL si vide */
    size_t taille_corps;    /* nombre d'octets du corps                */
    char   tampon[HTTP_TAILLE_MAX + 1];   /* +1 pour un '\0' final   */
} Requete;

/* Lit une requête complète sur la connexion client et la découpe dans *req.
   Renvoie :
     0   requête correcte, *req est rempli ;
    -1   client parti ou muet trop longtemps : on ferme sans répondre ;
    400  requête mal formée     (à renvoyer au client tel quel) ;
    413  requête trop grosse    (idem). */
int http_requete_lire(int client, Requete *req);

/* Cherche l'en-tête nom (ex. "Authorization", majuscules ignorées)
   et copie sa valeur dans dest. Renvoie 0 si trouvé, -1 sinon. */
int http_requete_entete(const Requete *req, const char *nom,
                        char *dest, size_t taille);

#endif /* HTTP_REQUETE_H */