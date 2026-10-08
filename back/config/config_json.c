/* ============================================================
   config_json.c – Lecture d'une valeur dans un bloc JSON (cJSON)
   ============================================================ */

#include <stdio.h>          /* fprintf        */
#include <string.h>         /* strlen, memcpy */

#include "config_json.h"

#define PREFIXE "[config] "

int config_json_texte(const cJSON *bloc, const char *nom_bloc, const char *cle,
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

int config_json_entier(const cJSON *bloc, const char *nom_bloc, const char *cle,
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