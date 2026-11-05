/* ============================================================
   simdom_interne.h – Ce que les fichiers du module simdom partagent
   entre eux. Le reste du serveur n'inclut que simdom.h.
   ============================================================ */

#ifndef SIMDOM_INTERNE_H
#define SIMDOM_INTERNE_H

#include <stddef.h>         /* size_t */
#include "simdom.h"

#define PREFIXE "[simdom] "

#define TAILLE_MESSAGE   13     /* luminaires, prises, volets */
#define TAILLE_CLIM      14     /* climatisations : + 1 octet de mode */
#define TAILLE_ADRESSE   12     /* IP (4) + entrée (8) : identifie l'équipement */

#define CODE_ETEINDRE    0x00
#define CODE_ALLUMER     0x01
#define CODE_LIRE        0x04

/* La connexion (une seule pour tout le serveur) : simdom_connexion.c */
extern int prise;           /* -1 = pas connecté */
extern int delai;           /* attente maximale d'une réponse, en ms */

/* simdom_connexion.c */
void couper(const char *raison);
void accuser_reception(void);
int  envoyer(const unsigned char *message, size_t taille);
int  vider(void);
int  verifier_connexion(void);

/* simdom.c */
int  ecrire_adresse(unsigned char *message, const AdresseSimdom *a);

#endif /* SIMDOM_INTERNE_H */
