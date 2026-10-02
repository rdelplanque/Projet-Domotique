/*
 * format_equipement.h - Ce que l'on sait du fichier equipement.txt :
 * ses sections, le type d'équipement de chaque section, ses états.
 */
#ifndef FORMAT_EQUIPEMENT_H
#define FORMAT_EQUIPEMENT_H

/* Section du fichier en cours de lecture */
typedef enum {
    SECTION_AUCUNE,
    SECTION_PIECES,
    SECTION_LUMINAIRES,
    SECTION_PRISES,
    SECTION_CLIMATISATIONS,
    SECTION_VOLETS
} Section;

/* Index des champs d'une ligne d'équipement (après découpage aux virgules) */
enum {
    CHAMP_IP = 0,       /* 192.168.0.100                         */
    CHAMP_NUMERO,       /* 00010111                              */
    CHAMP_ETAT,         /* éteint / allumé / en panne            */
    CHAMP_DESCRIPTION,  /* Salon - Luminaire salon nord          */
    CHAMP_PIECE,        /* 23                                    */
    CHAMP_EXTRA         /* puissance (luminaires) ou mode (clims) */
};

/*
 * Si la ligne est un titre de section (« Luminaires / appliques... »),
 * renvoie la section correspondante ; sinon renvoie SECTION_AUCUNE.
 */
Section detecter_section(const char *ligne);

/* Nom du type d'équipement, identique à sql/peuplement_de_la_bdd/04_type_equipement.sql
   (NULL pour SECTION_AUCUNE et SECTION_PIECES). */
const char *nom_type(Section s);

/* « éteint » -> 0, « allumé » -> 1, « en panne » -> 2, autre -> -1 */
int convertir_etat(const char *etat);

/* Renvoie 1 si la ligne contient des données (elle commence par un chiffre), 0 sinon. */
int est_ligne_de_donnees(const char *ligne);

#endif /* FORMAT_EQUIPEMENT_H */