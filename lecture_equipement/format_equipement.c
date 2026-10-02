/*
 * format_equipement.c - Connaissance du format de equipement.txt (voir format_equipement.h).
 */
#include <string.h>
#include "format_equipement.h"
#include "texte.h"

Section detecter_section(const char *ligne)
{
    if (commence_par(ligne, "Localisations"))  return SECTION_PIECES;
    if (commence_par(ligne, "Luminaires"))     return SECTION_LUMINAIRES;
    if (commence_par(ligne, "Prises"))         return SECTION_PRISES;
    if (commence_par(ligne, "Climatisations")) return SECTION_CLIMATISATIONS;
    if (commence_par(ligne, "Volets"))         return SECTION_VOLETS;
    return SECTION_AUCUNE;
}

const char *nom_type(Section s)
{
    switch (s) {
    case SECTION_LUMINAIRES:     return "Luminaire";
    case SECTION_PRISES:         return "Prise commandée";
    case SECTION_CLIMATISATIONS: return "Climatisation réversible";
    case SECTION_VOLETS:         return "Volet roulant / porte basculante";
    default:                     return NULL;
    }
}

int convertir_etat(const char *etat)
{
    if (strcmp(etat, "éteint") == 0)   return 0;
    if (strcmp(etat, "allumé") == 0)   return 1;
    if (strcmp(etat, "en panne") == 0) return 2;
    return -1;
}

int est_ligne_de_donnees(const char *ligne)
{
    return ligne[0] >= '0' && ligne[0] <= '9';
}