/*
 * json.h - Écriture des fichiers pieces.json et equipements.json.
 * Chaque fichier est un tableau JSON : [ objet, objet, ... ]
 */
#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include "format_equipement.h"

/* Écrit une chaîne entre guillemets, en échappant " et \ */
void json_ecrire_chaine(FILE *f, const char *texte);

/* Ouvre / ferme le tableau JSON : « [ » et « ] » */
void json_debut_tableau(FILE *f);
void json_fin_tableau(FILE *f);

/*
 * Écrit une pièce : {"id": 1, "nom": "Terrasse", "niveau": "Extérieur"}
 * champs[0] = identifiant, champs[1] = "Niveau - Nom" (modifié : coupé en deux).
 * « premier » vaut 1 pour le premier objet du tableau (pas de virgule avant).
 * Renvoie 1 si la pièce a été écrite, 0 si la ligne est invalide.
 */
int json_ecrire_piece(FILE *f, char *champs[], int premier);

/*
 * Écrit un équipement :
 * {"ip": ..., "numero": ..., "etat": ..., "nom": ..., "piece": ...,
 *  "puissance": ..., "mode": ..., "type": ...}
 * nb = nombre de champs de la ligne, s = section (donne le type).
 */
void json_ecrire_equipement(FILE *f, char *champs[], int nb, Section s, int premier);

#endif /* JSON_H */