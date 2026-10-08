/* ============================================================
   config_fichier.c – Lecture d'un fichier texte en mémoire
   ============================================================ */

#include <stdio.h>          /* fopen, fseek, ftell, fread, perror */
#include <stdlib.h>         /* malloc                             */

#include "config_fichier.h"

/* Un config.json fait quelques centaines d'octets :
   au-delà de 64 Ko, ce n'est sûrement pas le bon fichier. */
#define TAILLE_MAX_FICHIER (64 * 1024)

#define PREFIXE "[config] "

char *config_lire_fichier(const char *chemin)
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