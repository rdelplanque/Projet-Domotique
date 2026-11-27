/* ============================================================
   ldsd.h – Lecture (analyse) d'un scénario écrit en LDSD

   Le texte :
       En-tête :
       chaque samedi et dimanche à 08:00
       Liste :
       ouvrir tous les volets de toutes les pièces de "Premier étage"
       allumer "Climatisation réversible" de "Salon" de "Rez-de-chaussée" en chauffage

   devient une fiche Programme : un en-tête (quand ?) et une liste
   d'actions (quoi, sur quoi). Les noms sont cherchés dans la maison
   (Maison, lue dans la base) : la fiche ne contient que des numéros.

   À la première erreur, la lecture s'arrête : numéro de ligne + message.
   ============================================================ */

#ifndef LDSD_H
#define LDSD_H

#include <stddef.h>                 /* size_t */
#include "../bdd/bdd_maison.h"      /* Maison, VALEUR_ABSENTE */

#define LDSD_MAX_ACTIONS    100     /* lignes d'action au plus   */
#define LDSD_TAILLE_MESSAGE 256     /* message d'erreur          */

/* ---------- L'en-tête : quand le scénario se lance ---------- */

typedef enum {
    DECLENCHEUR_DIRECT,     /* au clic                                 */
    DECLENCHEUR_UNE_FOIS,   /* le 24/12/2026 à 19:00                   */
    DECLENCHEUR_CHAQUE      /* chaque samedi et dimanche à 08:00       */
} Declencheur;

/* Jours de la semaine : un bit par jour (comme les entrées SimDom).
   « samedi et dimanche » = JOUR_SAMEDI | JOUR_DIMANCHE = 0110 0000 */
#define JOUR_LUNDI      (1u << 0)
#define JOUR_MARDI      (1u << 1)
#define JOUR_MERCREDI   (1u << 2)
#define JOUR_JEUDI      (1u << 3)
#define JOUR_VENDREDI   (1u << 4)
#define JOUR_SAMEDI     (1u << 5)
#define JOUR_DIMANCHE   (1u << 6)

typedef struct {
    Declencheur   type;
    int           jour, mois, annee;  /* DECLENCHEUR_UNE_FOIS seulement */
    unsigned char jours;              /* DECLENCHEUR_CHAQUE seulement   */
    int           heure, minute;      /* sauf DECLENCHEUR_DIRECT        */
} EnTete;

/* ---------- Les actions ---------- */

typedef enum { 
    VERBE_ALLUMER, 
    VERBE_ETEINDRE, 
    VERBE_OUVRIR, 
    VERBE_FERMER 
} Verbe;

/* Les 4 types d'équipements (nom_type_equipement dans la base) */
typedef enum {
    TYPE_LUMINAIRE, 
    TYPE_PRISE, 
    TYPE_CLIMATISATION, 
    TYPE_VOLET,
    TYPE_NOMBRE                     /* pas un type : le nombre de types */
} TypeEquipement;

/* Sur quoi agit l'action :
     un équipement      → id_equipement > 0 (le reste ne sert pas) ;
     un groupe          → id_equipement = 0, type = le type du groupe,
                          id_piece = 0 si « toutes les pièces »,
                          id_etage = 0 si « tous les étages ». */
typedef struct {
    long long      id_equipement;
    TypeEquipement type;
    long long      id_piece;
    long long      id_etage;
} Cible;

typedef struct {
    int   ligne;            /* numéro de ligne dans le texte (pour le journal) */
    Verbe verbe;
    Cible cible;
    int   mode;             /* 0 climatisation, 1 chauffage, VALEUR_ABSENTE = inchangé */
    int   nb_equipements;   /* combien d'équipements sont visés aujourd'hui */
} Action;

/* ---------- Le scénario lu ---------- */

typedef struct {
    EnTete en_tete;
    int    nb_actions;
    Action actions[LDSD_MAX_ACTIONS];
} Programme;

typedef struct {
    int  ligne;                         /* 0 = pas de ligne précise */
    char message[LDSD_TAILLE_MESSAGE];
} ErreurLdsd;

/* Lit le texte et remplit *p, en cherchant les noms dans *m.
   Renvoie 0 si le scénario est correct, -1 sinon (*err est rempli). */
int ldsd_lire(const char *texte, const Maison *m, Programme *p, ErreurLdsd *err);

/* Lit seulement l'en-tête (quand le scénario se lance), sans la maison :
   pour la liste (type direct / programmé) et pour le planificateur.
   Renvoie 0 et remplit *e, ou -1 (*err est rempli). Le reste du texte
   n'est pas vérifié : ldsd_lire le fera au lancement. */
int ldsd_lire_en_tete(const char *texte, EnTete *e, ErreurLdsd *err);

/* Nom d'un verbe (« allumer »...), d'un type au pluriel (« volets »...),
   d'un jour (0 = « lundi »...) : pour fabriquer des textes lisibles. */
const char *ldsd_nom_verbe(Verbe v);
const char *ldsd_nom_type_pluriel(TypeEquipement t);
const char *ldsd_nom_jour(int numero);

/* Ce que le serveur a compris, en clair (pour la console « Vérifier ») :
     « chaque samedi et dimanche à 08:00 »
     « fermer 2 volets dans "Salon" »
     « allumer "Climatisation réversible" de "Salon" en chauffage » */
void ldsd_decrire_en_tete(const EnTete *e, char *dest, size_t taille);
void ldsd_decrire_action(const Maison *m, const Action *a, char *dest, size_t taille);

/* L'équipement e fait-il partie de la cible c ? (pièce de e cherchée dans m)
   Servira aussi à l'exécution, pour trouver les équipements d'un groupe. */
int ldsd_dans_cible(const Maison *m, const Cible *c, const Equipement *e);

#endif /* LDSD_H */
