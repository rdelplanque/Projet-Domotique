/* ============================================================
   config_json.h – Lecture d'une valeur dans un bloc JSON (cJSON)
   avec vérification du type, des limites et valeur par défaut
   ============================================================ */

#ifndef CONFIG_JSON_H
#define CONFIG_JSON_H

#include <stddef.h>         /* size_t */
#include <cjson/cJSON.h>    /* cJSON  */

/* Copie le texte bloc[cle] dans dest (taille = place disponible).
   Clé absente : on prend defaut ; si defaut vaut NULL, la clé est obligatoire.
   nom_bloc sert seulement aux messages d'erreur ("bdd", "serveur"...).
   Renvoie 0 si OK, 1 si erreur (pour pouvoir additionner les erreurs). */
int config_json_texte(const cJSON *bloc, const char *nom_bloc, const char *cle,
                      const char *defaut, char *dest, size_t taille);

/* Lit l'entier bloc[cle] dans *dest, en vérifiant min <= valeur <= max.
   Clé absente : on prend defaut.
   Renvoie 0 si OK, 1 si erreur. */
int config_json_entier(const cJSON *bloc, const char *nom_bloc, const char *cle,
                       int defaut, int min, int max, int *dest);

#endif /* CONFIG_JSON_H */