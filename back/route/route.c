/* ============================================================
   route.c – L'aiguilleur de l'API R-Domotik

   Pour ajouter une route : écrire sa fonction (ici ou dans son
   module : auth, simdom...) et ajouter UNE ligne dans ROUTES[].
   ============================================================ */

#include <string.h>         /* strcmp */

#include "route.h"


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

typedef struct {
    const char *methode;        /* "GET", "POST"...  */
    const char *chemin;         /* "/api/ping"       */
    Traitement  traitement;     /* fonction à appeler */
} Route;

/* Le répertoire de l'API. La ligne {NULL...} marque la fin. */
static const Route ROUTES[] = {
    { "GET",  "/api/ping",  traiter_ping },
    /* { "POST", "/api/login", auth_login },   ← prochaine étape */
    { NULL, NULL, NULL }
};


/* ------------------------------------------------------------
   Fonction publique
   ------------------------------------------------------------ */

void route_traiter(const Requete *req, Reponse *rep)
{
    int i;
    int chemin_connu = 0;

    for (i = 0; ROUTES[i].chemin != NULL; i++) {
        if (strcmp(req->chemin, ROUTES[i].chemin) != 0) {
            continue;                           /* pas ce chemin : suivante */
        }
        chemin_connu = 1;
        if (strcmp(req->methode, ROUTES[i].methode) == 0) {
            ROUTES[i].traitement(req, rep);     /* appel de la fonction notée */
            return;
        }
    }

    if (chemin_connu) {
        /* ex. GET /api/login alors que seul POST existe */
        http_reponse_erreur(rep, 405, "méthode non autorisée pour cette route");
    } else {
        http_reponse_erreur(rep, 404, "route inconnue");
    }
}