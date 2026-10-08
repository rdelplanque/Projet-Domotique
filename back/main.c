/* ============================================================
   main.c – Point d'entrée du serveur R-Domotik
   Ordre de démarrage : config → bdd → écoute HTTP → SimDom
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>

#include "config/config.h"
#include "bdd/bdd.h"
#include "http/http_serveur.h"
#include "auth/auth.h"

int main(int argc, char *argv[])
{
    Config cfg;          /* la fiche des réglages, remplie par config  */
    PGconn *conn;        /* la connexion à PostgreSQL, ouverte par bdd */
    int ecoute;          /* la prise réseau du guichet HTTP            */

    /* 1. Configuration : arguments + config.json. Échec = arrêt. */
    if (config_charger(argc, argv, &cfg) != 0) {
        fprintf(stderr, "Arrêt du serveur : configuration invalide.\n");
        return EXIT_FAILURE;
    }
    config_afficher(&cfg);

    /* 2. Base de données : connexion + vérification. Échec = arrêt. */
    conn = bdd_connecter(&cfg);
    if (conn == NULL) {
        fprintf(stderr, "Arrêt du serveur : base de données inaccessible.\n");
        return EXIT_FAILURE;
    }
    if (bdd_verifier(conn) != 0) {
        fprintf(stderr, "Arrêt du serveur : base de données incomplète.\n");
        bdd_fermer(conn);
        return EXIT_FAILURE;
    }

    /* Les modules qui ont besoin de la base reçoivent la connexion */
    auth_initialiser(conn);

    /* 3. Écoute HTTP de l'API. Échec = arrêt. */
    ecoute = http_ouvrir(cfg.port_api);
    if (ecoute < 0) {
        fprintf(stderr, "Arrêt du serveur : port de l'API indisponible.\n");
        bdd_fermer(conn);
        return EXIT_FAILURE;
    }

    /* 4. Connexion à SimDom : à venir (avant la boucle) */

    printf("Serveur prêt. Ctrl+C pour l'arrêter.\n");
    fflush(stdout);
    http_boucle(ecoute);     /* ne revient jamais */

    bdd_fermer(conn);        /* jamais atteint pour l'instant (arrêt propre : plus tard) */
    return EXIT_SUCCESS;
}