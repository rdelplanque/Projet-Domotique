/*
 * lecture_equipement.c
 *
 * Lit le fichier equipement.txt fourni par le simulateur SimDom et produit :
 *   - pieces.json      : la liste des pièces / emplacements
 *   - equipements.json : la liste des équipements
 * Ces deux fichiers sont ensuite chargés en base par les scripts
 * sql/peuplement_de_la_bdd/03_piece.sql et 06_equipement.sql.
 *
 *
 * Compilation et lancement (depuis le dossier lecture_equipement) :
 *   gcc -Wall -Wextra -std=c11 -o lecture_equipement lecture_equipement.c
 *   ./lecture_equipement equipement.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAILLE_LIGNE 512   /* longueur maximale d'une ligne du fichier */
#define MAX_CHAMPS   8     /* nombre maximal de champs séparés par des virgules */

/* Section du fichier en cours de lecture */
typedef enum {
    SECTION_AUCUNE,
    SECTION_PIECES,
    SECTION_LUMINAIRES,
    SECTION_PRISES,
    SECTION_CLIMATISATIONS,
    SECTION_VOLETS
} Section;

/* Nom du type d'équipement, identique à sql/peuplement_de_la_bdd/04_type_equipement.sql */
static const char *nom_type(Section s)
{
    switch (s) {
    case SECTION_LUMINAIRES:     return "Luminaire";
    case SECTION_PRISES:         return "Prise commandée";
    case SECTION_CLIMATISATIONS: return "Climatisation réversible";
    case SECTION_VOLETS:         return "Volet roulant / porte basculante";
    default:                     return NULL;
    }
}

/* Retire le saut de ligne de fin ("\n" sous Linux, "\r\n" pour un fichier Windows) */
static void retirer_fin_de_ligne(char *ligne)
{
    ligne[strcspn(ligne, "\r\n")] = '\0';
}

/* Vrai si la ligne commence par le texte donné */
static int commence_par(const char *ligne, const char *debut)
{
    return strncmp(ligne, debut, strlen(debut)) == 0;
}

/*
 * Découpe la ligne aux virgules (la ligne est modifiée : chaque virgule
 * devient une fin de chaîne). Remplit champs[] et renvoie le nombre de champs.
 * On n'utilise pas strtok car il ignorerait les champs vides.
 */
static int decouper(char *ligne, char *champs[], int max)
{
    int n = 0;
    char *debut = ligne;

    while (n < max) {
        char *virgule = strchr(debut, ',');
        champs[n++] = debut;
        if (virgule == NULL)
            break;
        *virgule = '\0';
        debut = virgule + 1;
    }
    return n;
}

/*
 * Renvoie la partie située après le DERNIER " - " de la description :
 * "Salon - Luminaire salon nord" -> "Luminaire salon nord".
 * Sans " - ", renvoie la description entière.
 */
static const char *apres_dernier_tiret(const char *texte)
{
    const char *resultat = texte;
    const char *p = texte;

    while ((p = strstr(p, " - ")) != NULL) {
        p += 3;            /* on saute " - " */
        resultat = p;
    }
    return resultat;
}

/* Convertit l'état texte du fichier en nombre : 0, 1, 2 ou -1 si inconnu */
static int convertir_etat(const char *etat)
{
    if (strcmp(etat, "éteint") == 0)   return 0;
    if (strcmp(etat, "allumé") == 0)   return 1;
    if (strcmp(etat, "en panne") == 0) return 2;
    return -1;
}

/* Écrit une chaîne JSON entre guillemets, en échappant " et \ */
static void ecrire_chaine_json(FILE *f, const char *texte)
{
    fputc('"', f);
    for (const char *p = texte; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\')
            fputc('\\', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

/* Écrit un objet pièce : {"id":1,"nom":"Terrasse","niveau":"Extérieur"} */
static void ecrire_piece(FILE *f, char *champs[], int premier)
{
    /* champs[0] = identifiant, champs[1] = "Niveau - Nom de la pièce" */
    char *description = champs[1];
    char *separateur = strstr(description, " - ");

    if (separateur == NULL) {
        fprintf(stderr, "Pièce ignorée (pas de niveau) : %s\n", description);
        return;
    }
    *separateur = '\0';                    /* coupe la description en deux */

    fprintf(f, "%s\n  {\"id\": %d, \"nom\": ", premier ? "" : ",", atoi(champs[0]));
    ecrire_chaine_json(f, separateur + 3);  /* après " - " : le nom */
    fprintf(f, ", \"niveau\": ");
    ecrire_chaine_json(f, description);     /* avant " - " : le niveau */
    fprintf(f, "}");
}

/*
 * Écrit un objet équipement. Champs du fichier :
 *   0 IP automate, 1 entrée, 2 état, 3 description, 4 pièce,
 *   5 puissance (luminaires) ou mode (climatisations)
 */
static void ecrire_equipement(FILE *f, char *champs[], int nb, Section s, int premier)
{
    int etat = convertir_etat(champs[2]);

    fprintf(f, "%s\n  {\"ip\": ", premier ? "" : ",");
    ecrire_chaine_json(f, champs[0]);
    fprintf(f, ", \"numero\": ");
    ecrire_chaine_json(f, champs[1]);

    if (etat < 0) {
        fprintf(stderr, "État inconnu « %s » pour : %s\n", champs[2], champs[3]);
        fprintf(f, ", \"etat\": null");
    } else {
        fprintf(f, ", \"etat\": %d", etat);
    }

    fprintf(f, ", \"nom\": ");
    ecrire_chaine_json(f, apres_dernier_tiret(champs[3]));
    fprintf(f, ", \"piece\": %d", atoi(champs[4]));

    /* puissance : "500.0" -> 500 (luminaires seulement) */
    if (s == SECTION_LUMINAIRES && nb > 5)
        fprintf(f, ", \"puissance\": %d", (int) atof(champs[5]));
    else
        fprintf(f, ", \"puissance\": null");

    /* mode : 0 climatisation, 1 chauffage (climatisations seulement) */
    if (s == SECTION_CLIMATISATIONS && nb > 5)
        fprintf(f, ", \"mode\": %d", atoi(champs[5]));
    else
        fprintf(f, ", \"mode\": null");

    fprintf(f, ", \"type\": ");
    ecrire_chaine_json(f, nom_type(s));
    fprintf(f, "}");
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage : %s equipement.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *entree = fopen(argv[1], "r");
    if (entree == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    FILE *pieces = fopen("pieces.json", "w");
    FILE *equipements = fopen("equipements.json", "w");
    if (pieces == NULL || equipements == NULL) {
        perror("Création des fichiers JSON");
        return EXIT_FAILURE;
    }

    char ligne[TAILLE_LIGNE];
    char *champs[MAX_CHAMPS];
    Section section = SECTION_AUCUNE;
    int nb_pieces = 0, nb_equipements = 0;

    fprintf(pieces, "[");
    fprintf(equipements, "[");

    while (fgets(ligne, sizeof ligne, entree) != NULL) {
        retirer_fin_de_ligne(ligne);

        /* Les titres de section indiquent ce qui suit */
        if (commence_par(ligne, "Localisations"))  { section = SECTION_PIECES;         continue; }
        if (commence_par(ligne, "Luminaires"))     { section = SECTION_LUMINAIRES;     continue; }
        if (commence_par(ligne, "Prises"))         { section = SECTION_PRISES;         continue; }
        if (commence_par(ligne, "Climatisations")) { section = SECTION_CLIMATISATIONS; continue; }
        if (commence_par(ligne, "Volets"))         { section = SECTION_VOLETS;         continue; }

        /* Les lignes de données commencent par un chiffre ; on ignore le reste
           (lignes vides, lignes d'en-tête « Identifiant, ... », « IP automate, ... ») */
        if (ligne[0] < '0' || ligne[0] > '9')
            continue;

        int nb = decouper(ligne, champs, MAX_CHAMPS);

        if (section == SECTION_PIECES) {
            if (nb < 2) {
                fprintf(stderr, "Ligne de pièce incomplète ignorée\n");
                continue;
            }
            ecrire_piece(pieces, champs, nb_pieces == 0);
            nb_pieces++;
        } else if (section != SECTION_AUCUNE) {
            if (nb < 5) {
                fprintf(stderr, "Ligne d'équipement incomplète ignorée : %s\n", champs[0]);
                continue;
            }
            ecrire_equipement(equipements, champs, nb, section, nb_equipements == 0);
            nb_equipements++;
        }
    }

    fprintf(pieces, "\n]\n");
    fprintf(equipements, "\n]\n");

    fclose(entree);
    fclose(pieces);
    fclose(equipements);

    printf("%d pièces écrites dans pieces.json\n", nb_pieces);
    printf("%d équipements écrits dans equipements.json\n", nb_equipements);
    return EXIT_SUCCESS;
}