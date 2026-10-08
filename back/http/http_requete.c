/* ============================================================
   http_requete.c – Lecture et découpage d'une requête HTTP

   Rappel de ce qui arrive (chaque ligne finit par \r\n) :
     POST /api/login HTTP/1.0          ← ligne 1
     Host: localhost                   ← en-têtes
     Content-Length: 52
                                       ← ligne vide : fin des en-têtes
     {"email":"...","mdp":"..."}       ← corps (52 octets)
   ============================================================ */

#include <stdio.h>          /* sscanf, fprintf          */
#include <stdlib.h>         /* strtol                   */
#include <string.h>         /* strstr, strchr, strlen   */
#include <strings.h>        /* strncasecmp (POSIX)      */
#include <errno.h>
#include <sys/socket.h>     /* recv                     */

#include "http_requete.h"

#define PREFIXE "[http] "
#define FIN_ENTETES "\r\n\r\n"


/* ------------------------------------------------------------
   Outils internes
   ------------------------------------------------------------ */

/* Lit ce qui arrive et l'AJOUTE à la suite de ce qu'on a déjà (deja octets).
   Renvoie le nouveau total, ou -1 si le client est parti / muet. */
static long recevoir_suite(int client, char *tampon, size_t deja, size_t max)
{
    long lus = (long)recv(client, tampon + deja, max - deja, 0);

    if (lus <= 0) {                     /* 0 = client parti, -1 = délai dépassé */
        return -1;
    }
    deja += (size_t)lus;
    tampon[deja] = '\0';                /* pour pouvoir utiliser strstr */
    return (long)deja;
}

/* Découpe la ligne 1 : "METHODE /chemin HTTP/1.x".
   Renvoie 0 si correcte, -1 sinon. */
static int decouper_ligne1(Requete *req)
{
    char version[16];
    char *point_interrogation;

    /* %7s, %255s, %15s : on ne lit jamais plus que la place disponible
       (taille du tableau - 1 pour le '\0'). sscanf renvoie le nombre
       de morceaux lus : il en faut exactement 3. */
    if (sscanf(req->tampon, "%7s %255s %15s",
               req->methode, req->chemin, version) != 3) {
        return -1;
    }
    if (strncmp(version, "HTTP/1.", 7) != 0 || req->chemin[0] != '/') {
        return -1;
    }

    /* "/api/maison?piece=3" → on garde "/api/maison" (paramètres pas utilisés) */
    point_interrogation = strchr(req->chemin, '?');
    if (point_interrogation != NULL) {
        *point_interrogation = '\0';
    }
    return 0;
}


/* ------------------------------------------------------------
   Fonctions publiques
   ------------------------------------------------------------ */

int http_requete_lire(int client, Requete *req)
{
    long recu = 0;              /* octets reçus jusqu'ici */
    char *fin_entetes;
    size_t taille_entetes;
    char valeur[32];
    char *fin_nombre;
    long longueur;

    req->tampon[0] = '\0';
    req->corps = NULL;
    req->taille_corps = 0;

    /* 1. Recevoir jusqu'à la ligne vide (\r\n\r\n).
          Les données peuvent arriver en plusieurs morceaux : on boucle. */
    while ((fin_entetes = strstr(req->tampon, FIN_ENTETES)) == NULL) {
        if ((size_t)recu >= HTTP_TAILLE_MAX) {
            return 413;                 /* en-têtes interminables */
        }
        recu = recevoir_suite(client, req->tampon, (size_t)recu, HTTP_TAILLE_MAX);
        if (recu < 0) {
            return -1;
        }
    }

    /* On coupe le texte à la ligne vide : la zone des en-têtes devient
       une chaîne C à elle seule ; le corps commence 4 octets plus loin. */
    *fin_entetes = '\0';
    taille_entetes = (size_t)(fin_entetes - req->tampon) + strlen(FIN_ENTETES);

    /* 2. Ligne 1 */
    if (decouper_ligne1(req) != 0) {
        fprintf(stderr, PREFIXE "ligne 1 invalide\n");
        return 400;
    }

    /* 3. Taille du corps (en-tête Content-Length). Absent = pas de corps. */
    if (http_requete_entete(req, "Content-Length", valeur, sizeof valeur) == 0) {
        errno = 0;
        longueur = strtol(valeur, &fin_nombre, 10);
        if (fin_nombre == valeur || *fin_nombre != '\0' || errno != 0 || longueur < 0) {
            fprintf(stderr, PREFIXE "Content-Length invalide : '%s'\n", valeur);
            return 400;
        }
        if (taille_entetes + (size_t)longueur > HTTP_TAILLE_MAX) {
            return 413;
        }
        req->taille_corps = (size_t)longueur;
    }

    /* 4. Recevoir la suite du corps si tout n'est pas encore arrivé */
    while ((size_t)recu < taille_entetes + req->taille_corps) {
        recu = recevoir_suite(client, req->tampon, (size_t)recu, HTTP_TAILLE_MAX);
        if (recu < 0) {
            return -1;                  /* corps annoncé mais jamais reçu */
        }
    }

    if (req->taille_corps > 0) {
        req->corps = req->tampon + taille_entetes;
        req->corps[req->taille_corps] = '\0';   /* corps = chaîne C (pour cJSON) */
    }
    return 0;
}

int http_requete_entete(const Requete *req, const char *nom,
                        char *dest, size_t taille)
{
    size_t longueur_nom = strlen(nom);
    const char *ligne = strstr(req->tampon, "\r\n");    /* fin de la ligne 1 */
    const char *valeur;
    size_t longueur;

    /* On parcourt les en-têtes ligne par ligne (ils s'arrêtent au '\0'
       posé à la place de la ligne vide). */
    while (ligne != NULL) {
        ligne += 2;                                     /* saute le \r\n */

        /* "Content-Length: 52" : le nom, puis ':' (majuscules ignorées,
           car en HTTP "content-length" et "Content-Length" sont pareils) */
        if (strncasecmp(ligne, nom, longueur_nom) == 0 && ligne[longueur_nom] == ':') {
            valeur = ligne + longueur_nom + 1;
            while (*valeur == ' ' || *valeur == '\t') {
                valeur++;                               /* espaces après ':' */
            }
            longueur = strcspn(valeur, "\r");           /* jusqu'à la fin de ligne */
            if (longueur >= taille) {
                return -1;                              /* trop long pour dest */
            }
            memcpy(dest, valeur, longueur);
            dest[longueur] = '\0';
            return 0;
        }
        ligne = strstr(ligne, "\r\n");                  /* ligne suivante */
    }
    return -1;
}