/* ============================================================
   bdd_maison.h – Lecture de la maison dans la base
   Étages, pièces et équipements, avec l'état de chacun ;
   mise à jour de l'état d'un équipement.
   (Les tables localisation, piece, equipement, type_equipement
   et acces_piece.)
   ============================================================ */

#ifndef BDD_MAISON_H
#define BDD_MAISON_H

#include <libpq-fe.h>

/* Tailles maximales (la maison SimDom : 3 étages, 46 pièces,
   211 équipements ; on garde de la marge). */
#define MAISON_MAX_ETAGES        8
#define MAISON_MAX_PIECES        128
#define MAISON_MAX_EQUIPEMENTS   512

/* Valeur rangée dans etat ou mode quand la base contient NULL */
#define VALEUR_ABSENTE  (-1)

/* Un étage = une ligne de la table localisation */
typedef struct {
    long long id;
    char      nom[51];          /* "Rez-de-chaussée"...           */
} Etage;

/* Une pièce */
typedef struct {
    long long id;
    char      nom[101];         /* "Salon"...                     */
    long long id_etage;
    int       autorisee;        /* 1 si une ligne acces_piece existe
                                   pour CET utilisateur (utile pour un Invité) */
} Piece;

/* Un équipement */
typedef struct {
    long long id;
    char      nom[151];         /* "Luminaire salon nord"...      */
    char      type[51];         /* nom_type_equipement : "Luminaire"... */
    long long id_piece;
    int       etat;             /* 0 éteint, 1 allumé, 2 en panne,
                                   VALEUR_ABSENTE = indéterminé    */
    int       mode;             /* 0 clim, 1 chauffage,
                                   VALEUR_ABSENTE = pas une climatisation */
    char      ip[16];           /* automate : "192.168.0.100" (pour SimDom) */
    char      entree[9];        /* numero_equipement : "00010111"           */
} Equipement;

/* La maison entière, telle que lue dans la base */
typedef struct {
    int        nb_etages;
    Etage      etages[MAISON_MAX_ETAGES];
    int        nb_pieces;
    Piece      pieces[MAISON_MAX_PIECES];
    int        nb_equipements;
    Equipement equipements[MAISON_MAX_EQUIPEMENTS];
} Maison;

/* Remplit *m avec tout le contenu de la base.
   id_utilisateur sert à savoir quelles pièces lui sont autorisées
   (champ autorisee de chaque pièce).
   Renvoie 0 si OK, -1 en cas d'erreur de la base. */
int bdd_lire_maison(PGconn *conn, long long id_utilisateur, Maison *m);

/* Enregistre l'état (et le mode) d'un équipement.
   etat ou mode = VALEUR_ABSENTE → NULL dans la base (indéterminé / pas de mode).
   Renvoie 0 si OK, -1 en cas d'erreur de la base. */
int bdd_ecrire_etat(PGconn *conn, long long id_equipement, int etat, int mode);

#endif /* BDD_MAISON_H */
