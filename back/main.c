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
#include "core/maison.h"
#include "simdom/simdom.h"

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
    maison_initialiser(conn);

    /* 3. Écoute HTTP de l'API. Échec = arrêt. */
    ecoute = http_ouvrir(cfg.port_api);
    if (ecoute < 0) {
        fprintf(stderr, "Arrêt du serveur : port de l'API indisponible.\n");
        bdd_fermer(conn);
        return EXIT_FAILURE;
    }

    /* 4. Connexion à SimDom, puis lecture de l'état réel de la maison.
          Échec = PAS d'arrêt : le serveur garde les états de la base. */
    simdom_initialiser(cfg.simdom_ip, cfg.simdom_port, cfg.delai_reponse_ms);
    if (simdom_connecter() == 0) {
        maison_synchroniser();
    } else {
        printf("SimDom injoignable : la page « Ma maison » montre les états de la base.\n");
    }

    printf("Serveur prêt. Ctrl+C pour l'arrêter.\n");
    fflush(stdout);
    /* Boucle du guichet, avec une « ronde » régulière : relire la maison */
    http_boucle(ecoute, cfg.intervalle_relecture_s, maison_ronde);  /* ne revient jamais */

    simdom_fermer();         /* jamais atteint pour l'instant */
    bdd_fermer(conn);        /* jamais atteint pour l'instant (arrêt propre : plus tard) */
    return EXIT_SUCCESS;
}