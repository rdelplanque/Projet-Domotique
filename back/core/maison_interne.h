/* ============================================================
   maison_interne.h – Ce que les fichiers du module maison
   partagent entre eux. Le reste du serveur n'inclut que maison.h.
   ============================================================ */

#ifndef MAISON_INTERNE_H
#define MAISON_INTERNE_H

#include <cjson/cJSON.h>

#include "maison.h"
#include "../bdd/bdd_maison.h"
#include "../simdom/simdom.h"

#define PREFIXE "[maison] "

/* maison.c : la connexion à la base et la maison lue */
extern PGconn *connexion;
extern Maison  maison;

/* maison.c : les règles, écrites une seule fois */
int  peut_voir_piece(const char *profil, const Piece *piece);
int  peut_piloter(const char *profil, const Piece *piece);
void ajouter_entier_ou_null(cJSON *objet, const char *cle, int valeur);

/* maison_relecture.c */
AdresseSimdom adresse_de(const Equipement *e);
int  relire_piece(long long id_piece);
void maison_noter_piece(long long id_piece);    /* pièce affichée par la page */

#endif /* MAISON_INTERNE_H */
