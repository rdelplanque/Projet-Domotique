/*
 * texte.c - Outils de manipulation de chaînes (voir texte.h).
 */
#include <string.h>
#include "texte.h"

void retirer_fin_de_ligne(char *ligne)
{
    /* strcspn donne la position du premier '\r' ou '\n' : on coupe la chaîne là */
    ligne[strcspn(ligne, "\r\n")] = '\0';
}

int commence_par(const char *ligne, const char *debut)
{
    return strncmp(ligne, debut, strlen(debut)) == 0;
}

/* On n'utilise pas strtok, qui sauterait les champs vides (« a,,b »). */
int decouper(char *ligne, char *champs[], int max)
{
    int n = 0;
    char *debut = ligne;

    while (n < max) {
        char *virgule = strchr(debut, ',');
        champs[n++] = debut;
        if (virgule == NULL)
            break;              /* dernier champ */
        *virgule = '\0';        /* termine le champ courant  en faisant un saut de ligne*/
        debut = virgule + 1;    /* le suivant commence après la virgule */
    }
    return n;
}

const char *apres_dernier_tiret(const char *texte)
{
    const char *resultat = texte;
    const char *p = texte;

    while ((p = strstr(p, " - ")) != NULL) {
        p += 3;                 /* on saute " - " */
        resultat = p;
    }
    return resultat;
}