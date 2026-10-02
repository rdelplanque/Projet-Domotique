/*
 * main.c - Programme lecture_equipement
 *
 * Lit equipement.txt (fourni par le simulateur SimDom) et produit :
 *   - pieces.json      : les pièces / emplacements
 *   - equipements.json : les équipements
 * chargés ensuite par sql/peuplement_de_la_bdd/03_piece.sql et 06_equipement.sql.
 * Aucune correction des données : seule leur forme change.
 *
 * Organisation :
 *   main.c               lecture ligne par ligne et aiguillage (ce fichier)
 *   texte.c/.h           outils sur les chaînes (découpage, fin de ligne...)
 *   format_equipement.c/.h   connaissance de equipement.txt (sections, états, types)
 *   json.c/.h            écriture des objets JSON
 *
 * Compilation et lancement (depuis le dossier lecture_equipement) :
 *   gcc -Wall -Wextra -std=c11 -o lecture_equipement *.c
 *   ./lecture_equipement equipement.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include "texte.h"
#include "format_equipement.h"
#include "json.h"

#define TAILLE_LIGNE 512    /* longueur maximale d'une ligne du fichier */
#define MAX_CHAMPS   8      /* nombre maximal de champs par ligne */

int main(int argc, char *argv[])  /* argc = nb de mots tapés (programme compris), argv[i] = i-ème mot (argv[0] = le programme) */
{
    if (argc != 2) {
        fprintf(stderr, "Usage : %s equipement.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- Ouverture des fichiers --- */
    FILE *entree = fopen(argv[1], "r");
    if (entree == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    FILE *pieces = fopen("pieces.json", "w");
    FILE *equipements = fopen("equipements.json", "w");
    if (pieces == NULL || equipements == NULL) {
        perror("Création des fichiers JSON");
        fclose(entree);
        return EXIT_FAILURE;
    }

    /* --- Lecture ligne par ligne --- */
    char ligne[TAILLE_LIGNE];
    char *champs[MAX_CHAMPS];
    Section section = SECTION_AUCUNE;
    int nb_pieces = 0, nb_equipements = 0;

    json_debut_tableau(pieces);
    json_debut_tableau(equipements);

    while (fgets(ligne, sizeof ligne, entree) != NULL) {
        retirer_fin_de_ligne(ligne);

        /* Un titre de section indique ce que contiennent les lignes suivantes */
        Section nouvelle = detecter_section(ligne);
        if (nouvelle != SECTION_AUCUNE) {
            section = nouvelle;
            continue;
        }

        /* Lignes vides et lignes d'en-tête (« Identifiant, ... ») : ignorées */
        if (!est_ligne_de_donnees(ligne))
            continue;

        int nb = decouper(ligne, champs, MAX_CHAMPS);

        if (section == SECTION_PIECES) {
            if (nb < 2) {
                fprintf(stderr, "Ligne de pièce incomplète ignorée\n");
                continue;
            }
            nb_pieces += json_ecrire_piece(pieces, champs, nb_pieces == 0);
        } else if (section != SECTION_AUCUNE) {
            if (nb <= CHAMP_PIECE) {
                fprintf(stderr, "Ligne d'équipement incomplète ignorée : %s\n", champs[0]);
                continue;
            }
            json_ecrire_equipement(equipements, champs, nb, section, nb_equipements == 0);
            nb_equipements++;
        }
    }

    json_fin_tableau(pieces);
    json_fin_tableau(equipements);

    /* --- Fermeture et bilan --- */
    fclose(entree);
    fclose(pieces);
    fclose(equipements);

    printf("%d pièces écrites dans pieces.json\n", nb_pieces);
    printf("%d équipements écrits dans equipements.json\n", nb_equipements);
    return EXIT_SUCCESS;
}