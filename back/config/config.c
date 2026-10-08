/* ============================================================
   config.c – Module de configuration du serveur R-Domotik
   Le chef d'orchestre : il appelle les outils des autres fichiers
     config_verif.c   : IP et port valides ?
     config_fichier.c : lire config.json en mémoire
     config_json.c    : lire une valeur du JSON avec vérifications
   ============================================================ */

#include <stdio.h>          /* printf, fprintf */
#include <stdlib.h>         /* free            */
#include <string.h>         /* memset, memcpy, strlen */
#include <cjson/cJSON.h>

#include "config.h"
#include "config_verif.h"
#include "config_fichier.h"
#include "config_json.h"

/* Valeurs par défaut des réglages facultatifs */
#define DEFAUT_BDD_HOTE      "localhost"
#define DEFAUT_BDD_PORT      5432
#define DEFAUT_PORT_API      8080
#define DEFAUT_INTERVALLE_S  5
#define DEFAUT_DELAI_MS      2000

#define PREFIXE "[config] "


/* ------------------------------------------------------------
   Étapes internes de config_charger (static = ce fichier seulement)
   Chacune renvoie le NOMBRE d'erreurs trouvées.
   ------------------------------------------------------------ */

/* Arguments 1 et 2 : IP et port de SimDom */
static int charger_arguments(char *argv[], Config *cfg)
{
    int erreurs = 0;

    if (!config_ip_valide(argv[1])) {
        fprintf(stderr, PREFIXE "IP de SimDom invalide : '%s'\n", argv[1]);
        erreurs++;
    } else {
        /* longueur <= 15 garantie : l'adresse a été acceptée */
        memcpy(cfg->simdom_ip, argv[1], strlen(argv[1]) + 1);
    }

    if (config_texte_vers_port(argv[2], &cfg->simdom_port) != 0) {
        fprintf(stderr, PREFIXE "port de SimDom invalide : '%s' (1 à 65535)\n", argv[2]);
        erreurs++;
    }
    return erreurs;
}

/* Bloc "bdd" : obligatoire (nom, utilisateur, mot de passe sans défaut) */
static int charger_bdd(const cJSON *racine, Config *cfg)
{
    const cJSON *bdd = cJSON_GetObjectItemCaseSensitive(racine, "bdd");
    int erreurs = 0;

    if (!cJSON_IsObject(bdd)) {
        fprintf(stderr, PREFIXE "bloc \"bdd\" absent ou invalide\n");
        return 1;
    }

    erreurs += config_json_texte(bdd, "bdd", "hote", DEFAUT_BDD_HOTE,
                                 cfg->bdd_hote, sizeof cfg->bdd_hote);
    erreurs += config_json_entier(bdd, "bdd", "port", DEFAUT_BDD_PORT, 1, 65535,
                                  &cfg->bdd_port);
    erreurs += config_json_texte(bdd, "bdd", "nom", NULL,
                                 cfg->bdd_nom, sizeof cfg->bdd_nom);
    erreurs += config_json_texte(bdd, "bdd", "utilisateur", NULL,
                                 cfg->bdd_utilisateur, sizeof cfg->bdd_utilisateur);
    erreurs += config_json_texte(bdd, "bdd", "mot_de_passe", NULL,
                                 cfg->bdd_mot_de_passe, sizeof cfg->bdd_mot_de_passe);
    return erreurs;
}

/* Bloc "serveur" : facultatif. S'il est absent, serveur vaut NULL,
   cJSON ne trouve donc aucune clé, et les valeurs par défaut s'appliquent. */
static int charger_serveur(const cJSON *racine, Config *cfg)
{
    const cJSON *serveur = cJSON_GetObjectItemCaseSensitive(racine, "serveur");
    int erreurs = 0;

    if (serveur != NULL && !cJSON_IsObject(serveur)) {
        fprintf(stderr, PREFIXE "bloc \"serveur\" invalide\n");
        return 1;
    }

    erreurs += config_json_entier(serveur, "serveur", "port_api",
                                  DEFAUT_PORT_API, 1, 65535, &cfg->port_api);
    erreurs += config_json_entier(serveur, "serveur", "intervalle_relecture_s",
                                  DEFAUT_INTERVALLE_S, 1, 3600,
                                  &cfg->intervalle_relecture_s);
    erreurs += config_json_entier(serveur, "serveur", "delai_reponse_ms",
                                  DEFAUT_DELAI_MS, 100, 60000,
                                  &cfg->delai_reponse_ms);
    return erreurs;
}

/* Transforme le texte en arbre JSON. Renvoie NULL si la syntaxe est fausse,
   avec le numéro de ligne de l'erreur. */
static cJSON *analyser_json(const char *texte, const char *chemin)
{
    cJSON *racine = cJSON_Parse(texte);

    if (racine == NULL) {
        /* cJSON indique où il s'est arrêté (un pointeur DANS texte) :
           on compte les '\n' avant cet endroit pour trouver la ligne. */
        const char *position = cJSON_GetErrorPtr();
        const char *p;
        int ligne = 1;

        for (p = texte; position != NULL && p < position; p++) {
            if (*p == '\n') {
                ligne++;
            }
        }
        fprintf(stderr, PREFIXE "'%s' n'est pas un JSON valide (vers la ligne %d)\n",
                chemin, ligne);
    }
    return racine;
}


/* ------------------------------------------------------------
   Fonctions publiques (promises dans config.h)
   ------------------------------------------------------------ */

int config_charger(int argc, char *argv[], Config *cfg)
{
    char *texte;
    cJSON *racine;
    int erreurs = 0;

    memset(cfg, 0, sizeof *cfg);        /* on part d'une fiche toute à zéro */

    if (argc != 4) {
        fprintf(stderr, "Usage : %s <ip_simdom> <port_simdom> <fichier_config.json>\n",
                argv[0]);
        return -1;
    }

    /* 1. Arguments */
    erreurs += charger_arguments(argv, cfg);

    /* 2. Fichier → texte → arbre JSON */
    texte = config_lire_fichier(argv[3]);
    if (texte == NULL) {
        return -1;
    }
    racine = analyser_json(texte, argv[3]);
    free(texte);                        /* cJSON a fait sa propre copie */
    if (racine == NULL) {
        return -1;
    }

    /* 3. Les deux blocs du JSON */
    erreurs += charger_bdd(racine, cfg);
    erreurs += charger_serveur(racine, cfg);

    cJSON_Delete(racine);               /* libère tout l'arbre JSON */

    if (erreurs > 0) {
        fprintf(stderr, PREFIXE "%d erreur(s) dans la configuration\n", erreurs);
        return -1;
    }
    return 0;
}

void config_afficher(const Config *cfg)
{
    printf("=== Configuration ===\n");
    printf("SimDom   : %s:%d\n", cfg->simdom_ip, cfg->simdom_port);
    printf("BDD      : %s@%s:%d/%s (mot de passe : ****)\n",
           cfg->bdd_utilisateur, cfg->bdd_hote, cfg->bdd_port, cfg->bdd_nom);
    printf("API      : port %d\n", cfg->port_api);
    printf("Relecture: toutes les %d s, délai de réponse %d ms\n",
           cfg->intervalle_relecture_s, cfg->delai_reponse_ms);
}