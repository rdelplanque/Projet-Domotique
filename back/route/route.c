/* ============================================================
   route.c – L'aiguilleur de l'API R-Domotik

   Pour ajouter une route : écrire sa fonction (ici ou dans son
   module : auth, simdom...) et ajouter UNE ligne dans ROUTES[].
   Toute nouvelle route est PROTEGEE, sauf raison précise.
   ============================================================ */

#include <string.h>         /* strcmp */

#include "route.h"
#include "../auth/auth.h"


/* ------------------------------------------------------------
   Les fonctions de traitement
   Elles ont toutes la même forme : (requête reçue, réponse à remplir).
   ------------------------------------------------------------ */

/* GET /api/ping : « le serveur est-il vivant ? » */
static void traiter_ping(const Requete *req, Reponse *rep)
{
    (void)req;      /* paramètre pas utilisé ici : on le dit au compilateur */
    http_reponse_json(rep, 200, "{\"ok\":true}");
}


/* ------------------------------------------------------------
   La table des routes
   ------------------------------------------------------------ */

/* Traitement = « l'adresse d'une fonction qui prend (req, rep) ».
   Une case de ce type contient donc le NOM d'une fonction à appeler,
   comme un numéro de téléphone noté dans un répertoire. */
typedef void (*Traitement)(const Requete *req, Reponse *rep);

/* Qui a le droit d'entrer ?
     PUBLIQUE : tout le monde (pas besoin de bracelet) ;
     PROTEGEE : seulement avec un jeton valable, sinon 401. */
typedef enum { PUBLIQUE, PROTEGEE } Acces;

typedef struct {
    const char *methode;        /* "GET", "POST"...  */
    const char *chemin;         /* "/api/ping"       */
    Acces       acces;          /* PUBLIQUE / PROTEGEE */
    Traitement  traitement;     /* fonction à appeler */
} Route;

/* Le répertoire de l'API. La ligne {NULL...} marque la fin. */
static const Route ROUTES[] = {
    { "GET",  "/api/ping",   PUBLIQUE, traiter_ping },
    { "POST", "/api/login",  PUBLIQUE, auth_login   },  /* dans auth/auth.c */
    { "POST", "/api/logout", PROTEGEE, auth_logout  },
    { "GET",  "/api/moi",    PROTEGEE, auth_moi     },
    { NULL, NULL, PUBLIQUE, NULL }
};


/* ------------------------------------------------------------
   Fonction publique
   ------------------------------------------------------------ */

void route_traiter(Requete *req, Reponse *rep)
{
    int i;
    int chemin_connu = 0;

    for (i = 0; ROUTES[i].chemin != NULL; i++) {
        if (strcmp(req->chemin, ROUTES[i].chemin) != 0) {
            continue;                           /* pas ce chemin : suivante */
        }
        chemin_connu = 1;
        if (strcmp(req->methode, ROUTES[i].methode) != 0) {
            continue;                           /* pas cette méthode : suivante */
        }

        /* Route trouvée. Protégée ? Alors on contrôle le bracelet,
           ICI et une seule fois, pour toutes les routes protégées. */
        if (ROUTES[i].acces == PROTEGEE) {
            req->session = auth_session(req);
            if (req->session == NULL) {
                http_reponse_erreur(rep, 401, "connexion requise");
                return;
            }
        }
        ROUTES[i].traitement(req, rep);         /* appel de la fonction notée */
        return;
    }

    if (chemin_connu) {
        /* ex. GET /api/login alors que seul POST existe */
        http_reponse_erreur(rep, 405, "méthode non autorisée pour cette route");
    } else {
        http_reponse_erreur(rep, 404, "route inconnue");
    }
}