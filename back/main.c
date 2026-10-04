/* ============================================================
   main.c – Point d'entrée du serveur R-Domotik
   Ordre de démarrage : config → bdd → écoute HTTP → SimDom
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>

#include "config/config.h"

int main(int argc, char *argv[])
{
    Config cfg;

    /* 1. Configuration : arguments + config.json. Échec = arrêt. */
    if (config_charger(argc, argv, &cfg) != 0) {
        fprintf(stderr, "Arrêt du serveur : configuration invalide.\n");
        return EXIT_FAILURE;
    }
    config_afficher(&cfg);

    /* 2. Base de données (module bdd) : à venir */
    /* 3. Écoute HTTP de l'API : à venir */
    /* 4. Connexion à SimDom : à venir */

    return EXIT_SUCCESS;
}