/* ============================================================
   droits.c – Le tableau des droits par profil
   Recopie du cahier des charges (§ 4.1). Pour changer un droit,
   on change un 0 ou un 1 ici, et c'est tout : le serveur ET
   l'affichage des pages suivent.
   ============================================================ */

#include <string.h>         /* strcmp */

#include "droits.h"

/* Noms envoyés au navigateur, dans l'ordre de l'enum Droit.
   Ce sont les valeurs des attributs data-droit="..." des pages. */
static const char *NOMS[DROIT_NOMBRE] = {
    "aucun", "consulter", "piloter", "scenarios",
    "utilisateurs", "statistiques", "configuration", "aide"
};

/* Une ligne par profil (noms exacts de la table type_utilisateur).
   Les colonnes suivent l'ordre de l'enum Droit. */
static const struct {
    const char *profil;
    int droits[DROIT_NOMBRE];
} TABLEAU[] = {
    /*                  aucun consulter piloter scenario utilateurs statistiques configuration aide */
    { "Administrateur", {  1,   1,   1,   1,   1,   1,   1,   1 } },
    { "Propriétaire",   {  1,   1,   1,   1,   0,   1,   0,   1 } },
    { "Utilisateur",    {  1,   1,   1,   0,   0,   1,   0,   1 } },
    { "Invité",         {  1,   1,   1,   0,   0,   0,   0,   1 } },
    { "Surveillance",   {  1,   1,   0,   0,   0,   1,   0,   0 } },
};

#define NB_PROFILS (sizeof TABLEAU / sizeof TABLEAU[0])


int a_le_droit(const char *profil, Droit droit)
{
    size_t i;

    if (profil == NULL || droit < 0 || droit >= DROIT_NOMBRE) {
        return 0;
    }
    for (i = 0; i < NB_PROFILS; i++) {
        if (strcmp(TABLEAU[i].profil, profil) == 0) {
            return TABLEAU[i].droits[droit];
        }
    }
    return 0;               /* profil inconnu : aucun droit */
}

const char *droit_nom(Droit droit)
{
    if (droit < 0 || droit >= DROIT_NOMBRE) {
        return "";
    }
    return NOMS[droit];
}