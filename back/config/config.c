/* ============================================================
   config.c – Module de configuration du serveur R-Domotik
   (le « travail » : lecture et vérification des réglages)
   ============================================================ */

#include <stdio.h>          /* fprintf, fopen, fread...      */
#include <stdlib.h>         /* malloc, free, strtol          */
#include <string.h>         /* strlen, memcpy, memset        */
#include <errno.h>          /* errno (débordement de strtol) */
#include <arpa/inet.h>      /* inet_pton (vérification d'IP) */
#include <cjson/cJSON.h>    /* lecture du JSON (libcjson)    */

#include "config.h"

/* Valeurs par défaut des réglages facultatifs */
#define DEFAUT_BDD_HOTE            "localhost"
#define DEFAUT_BDD_PORT            5432
#define DEFAUT_PORT_API            8080
#define DEFAUT_INTERVALLE_S        5
#define DEFAUT_DELAI_MS            2000

/* Un config.json fait quelques centaines d'octets :
   au-delà de 64 Ko, ce n'est sûrement pas le bon fichier. */
#define TAILLE_MAX_FICHIER         (64 * 1024)

/* Préfixe de tous les messages de ce module */
#define PREFIXE "[config] "


/* ------------------------------------------------------------
   Outils internes (static = visibles dans ce fichier seulement)
   ------------------------------------------------------------ */

/* Vérifie qu'un texte est une adresse IPv4 valide (ex. "192.168.56.1").
   inet_pton renvoie 1 si l'adresse est correcte. */
static int ip_valide(const char *texte)
{
    struct in_addr adresse;
    return inet_pton(AF_INET, texte, &adresse) == 1;
}

/* Convertit un texte en numéro de port (1 à 65535).
   strtol au lieu de atoi : atoi("abc") renvoie 0 sans prévenir,
   strtol nous dit où la lecture s'est arrêtée (fin).
   Renvoie 0 si OK, -1 sinon. */
static int texte_vers_port(const char *texte, int *port)
{
    char *fin;
    long valeur;

    errno = 0;
    valeur = strtol(texte, &fin, 10);

    if (fin == texte || *fin != '\0' || errno != 0
        || valeur < 1 || valeur > 65535) {
        return -1;
    }
    *port = (int)valeur;
    return 0;
}

/* Lit tout un fichier texte en mémoire.
   Renvoie un texte terminé par '\0' (à libérer avec free),
   ou NULL en cas d'erreur (message déjà affiché). */
static char *lire_fichier(const char *chemin)
{
    FILE *fichier;
    long taille;
    char *texte;
    size_t lus;

    fichier = fopen(chemin, "r");
    if (fichier == NULL) {
        fprintf(stderr, PREFIXE "impossible d'ouvrir '%s' : ", chemin);
        perror(NULL);                   /* ajoute la raison : No such file... */
        return NULL;
    }

    /* Taille du fichier : on va à la fin, on lit la position, on revient */
    fseek(fichier, 0, SEEK_END);
    taille = ftell(fichier);
    rewind(fichier);

    if (taille <= 0 || taille > TAILLE_MAX_FICHIER) {
        fprintf(stderr, PREFIXE "'%s' est vide ou trop gros (%ld octets)\n",
                chemin, taille);
        fclose(fichier);
        return NULL;
    }

    texte = malloc((size_t)taille + 1);  /* +1 pour le '\0' final */
    if (texte == NULL) {
        fprintf(stderr, PREFIXE "mémoire insuffisante\n");
        fclose(fichier);
        return NULL;
    }

    lus = fread(texte, 1, (size_t)taille, fichier);
    texte[lus] = '\0';
    fclose(fichier);
    return texte;
}

/* Copie le texte bloc[cle] dans dest (taille = place disponible).
   - clé absente : on prend defaut ; si defaut vaut NULL, le champ est obligatoire → erreur ;
   - clé présente mais pas un texte, vide ou trop long → erreur.
   Renvoie 0 si OK, 1 si erreur (on additionne les erreurs dans config_charger). */
static int lire_texte(const cJSON *bloc, const char *nom_bloc, const char *cle,
                      const char *defaut, char *dest, size_t taille)
{
    const cJSON *element = cJSON_GetObjectItemCaseSensitive(bloc, cle);
    const char *valeur;
    size_t longueur;

    if (element == NULL) {
        if (defaut == NULL) {
            fprintf(stderr, PREFIXE "\"%s.%s\" est obligatoire\n", nom_bloc, cle);
            return 1;
        }
        valeur = defaut;
    } else if (!cJSON_IsString(element)) {
        fprintf(stderr, PREFIXE "\"%s.%s\" doit être un texte entre guillemets\n",
                nom_bloc, cle);
        return 1;
    } else {
        valeur = element->valuestring;
    }

    longueur = strlen(valeur);
    if (longueur == 0) {
        fprintf(stderr, PREFIXE "\"%s.%s\" est vide\n", nom_bloc, cle);
        return 1;
    }
    if (longueur >= taille) {
        fprintf(stderr, PREFIXE "\"%s.%s\" est trop long (max %zu caractères)\n",
                nom_bloc, cle, taille - 1);
        return 1;
    }

    memcpy(dest, valeur, longueur + 1);  /* +1 : on copie aussi le '\0' */
    return 0;
}

/* Lit l'entier bloc[cle] dans *dest, en vérifiant min <= valeur <= max.
   Clé absente → valeur par défaut (tous les entiers ont un défaut).
   Renvoie 0 si OK, 1 si erreur. */
static int lire_entier(const cJSON *bloc, const char *nom_bloc, const char *cle,
                       int defaut, int min, int max, int *dest)
{
    const cJSON *element = cJSON_GetObjectItemCaseSensitive(bloc, cle);
    double valeur;

    if (element == NULL) {
        *dest = defaut;
        return 0;
    }
    if (!cJSON_IsNumber(element)) {
        fprintf(stderr, PREFIXE "\"%s.%s\" doit être un nombre (sans guillemets)\n",
                nom_bloc, cle);
        return 1;
    }

    /* cJSON range les nombres en double : 5.5 doit être refusé */
    valeur = element->valuedouble;
    if (valeur != (double)(int)valeur || valeur < min || valeur > max) {
        fprintf(stderr, PREFIXE "\"%s.%s\" doit être un entier entre %d et %d\n",
                nom_bloc, cle, min, max);
        return 1;
    }

    *dest = (int)valeur;
    return 0;
}


/* ------------------------------------------------------------
   Fonctions publiques (promises dans config.h)
   ------------------------------------------------------------ */

int config_charger(int argc, char *argv[], Config *cfg)
{
    char *texte;
    cJSON *racine;
    const cJSON *bdd;
    const cJSON *serveur;
    int erreurs = 0;

    memset(cfg, 0, sizeof *cfg);        /* on part d'une structure toute à zéro */

    /* --- 1. Les arguments --- */
    if (argc != 4) {
        fprintf(stderr, "Usage : %s <ip_simdom> <port_simdom> <fichier_config.json>\n",
                argv[0]);
        return -1;
    }

    if (!ip_valide(argv[1])) {
        fprintf(stderr, PREFIXE "IP de SimDom invalide : '%s'\n", argv[1]);
        erreurs++;
    } else {
        /* strlen(argv[1]) <= 15 est garanti : inet_pton a accepté l'adresse */
        memcpy(cfg->simdom_ip, argv[1], strlen(argv[1]) + 1);
    }

    if (texte_vers_port(argv[2], &cfg->simdom_port) != 0) {
        fprintf(stderr, PREFIXE "port de SimDom invalide : '%s' (1 à 65535)\n", argv[2]);
        erreurs++;
    }

    /* --- 2. Le fichier config.json --- */
    texte = lire_fichier(argv[3]);
    if (texte == NULL) {
        return -1;
    }

    racine = cJSON_Parse(texte);
    if (racine == NULL) {
        /* cJSON indique où il s'est arrêté (un pointeur DANS texte) :
           on compte les '\n' avant cet endroit pour donner le numéro de ligne.
           À faire AVANT free(texte), sinon le pointeur ne vaut plus rien. */
        const char *position = cJSON_GetErrorPtr();
        int ligne = 1;
        const char *p;

        for (p = texte; position != NULL && p < position; p++) {
            if (*p == '\n') {
                ligne++;
            }
        }
        fprintf(stderr, PREFIXE "'%s' n'est pas un JSON valide (vers la ligne %d)\n",
                argv[3], ligne);
        free(texte);
        return -1;
    }
    free(texte);                        /* cJSON a fait sa propre copie */

    /* Bloc "bdd" : obligatoire (nom, utilisateur, mot de passe n'ont pas de défaut) */
    bdd = cJSON_GetObjectItemCaseSensitive(racine, "bdd");
    if (!cJSON_IsObject(bdd)) {
        fprintf(stderr, PREFIXE "bloc \"bdd\" absent ou invalide\n");
        erreurs++;
    } else {
        erreurs += lire_texte(bdd, "bdd", "hote", DEFAUT_BDD_HOTE,
                              cfg->bdd_hote, sizeof cfg->bdd_hote);
        erreurs += lire_entier(bdd, "bdd", "port", DEFAUT_BDD_PORT, 1, 65535,
                               &cfg->bdd_port);
        erreurs += lire_texte(bdd, "bdd", "nom", NULL,
                              cfg->bdd_nom, sizeof cfg->bdd_nom);
        erreurs += lire_texte(bdd, "bdd", "utilisateur", NULL,
                              cfg->bdd_utilisateur, sizeof cfg->bdd_utilisateur);
        erreurs += lire_texte(bdd, "bdd", "mot_de_passe", NULL,
                              cfg->bdd_mot_de_passe, sizeof cfg->bdd_mot_de_passe);
    }

    /* Bloc "serveur" : facultatif. S'il est absent, serveur vaut NULL,
       cJSON_GetObjectItemCaseSensitive(NULL, ...) renvoie NULL,
       et lire_entier prend donc les valeurs par défaut. */
    serveur = cJSON_GetObjectItemCaseSensitive(racine, "serveur");
    if (serveur != NULL && !cJSON_IsObject(serveur)) {
        fprintf(stderr, PREFIXE "bloc \"serveur\" invalide\n");
        erreurs++;
    } else {
        erreurs += lire_entier(serveur, "serveur", "port_api", DEFAUT_PORT_API,
                               1, 65535, &cfg->port_api);
        erreurs += lire_entier(serveur, "serveur", "intervalle_relecture_s",
                               DEFAUT_INTERVALLE_S, 1, 3600,
                               &cfg->intervalle_relecture_s);
        erreurs += lire_entier(serveur, "serveur", "delai_reponse_ms",
                               DEFAUT_DELAI_MS, 100, 60000,
                               &cfg->delai_reponse_ms);
    }

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