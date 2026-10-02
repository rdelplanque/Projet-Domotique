/*
 * texte.h - Petits outils de manipulation de chaînes de caractères.
 * Rien ici ne dépend du format de equipement.txt : ces fonctions
 * pourraient servir dans n'importe quel programme.
 */
#ifndef TEXTE_H
#define TEXTE_H

/* Retire le saut de ligne de fin ("\n" Linux ou "\r\n" Windows). La chaîne est modifiée. */
void retirer_fin_de_ligne(char *ligne);

/* Renvoie 1 si « ligne » commence par « debut », 0 sinon. */
int commence_par(const char *ligne, const char *debut);

/*
 * Découpe la ligne aux virgules : chaque virgule est remplacée par '\0'
 * et champs[i] pointe sur le début du i-ème champ.
 * Renvoie le nombre de champs trouvés (au plus « max »).
 */
int decouper(char *ligne, char *champs[], int max);

/* Renvoie un pointeur sur ce qui suit le DERNIER " - " (ou le texte entier s'il n'y en a pas). */
const char *apres_dernier_tiret(const char *texte);

#endif /* TEXTE_H */