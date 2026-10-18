/* ============================================================
   droits.h – Qui a le droit de faire quoi (tableau du CDC § 4.1)
   Le tableau n'existe qu'ICI : le serveur s'en sert pour
   refuser (403) et /api/moi le donne au navigateur pour
   cacher les boutons interdits.
   ============================================================ */

#ifndef DROITS_H
#define DROITS_H

/* Les droits : une ligne du tableau du cahier des charges chacun. */
typedef enum {
    DROIT_AUCUN,            /* être connecté suffit (moi, logout) */
    DROIT_CONSULTER,        /* voir l'état des équipements        */
    DROIT_PILOTER,          /* allumer / éteindre                 */
    DROIT_SCENARIOS,        /* gérer les scénarios                */
    DROIT_UTILISATEURS,     /* gérer les utilisateurs             */
    DROIT_STATISTIQUES,     /* voir les statistiques              */
    DROIT_CONFIGURATION,    /* configurer l'application           */
    DROIT_AIDE,             /* manuel + à propos                  */
    DROIT_NOMBRE            /* pas un droit : le nombre de droits */
} Droit;

/* 1 si ce profil a ce droit, 0 sinon (profil inconnu : 0). */
int a_le_droit(const char *profil, Droit droit);

/* Nom du droit tel qu'envoyé au navigateur : "consulter", "piloter"... */
const char *droit_nom(Droit droit);

#endif