/*
 * json.c - Écriture des objets JSON (voir json.h).
 */
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include "texte.h"

void json_ecrire_chaine(FILE *f, const char *texte)
{
    fputc('"', f);
    for (const char *p = texte; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\')
            fputc('\\', f);     /* " devient \" et \ devient \\ */
        fputc(*p, f);
    }
    fputc('"', f);
}

void json_debut_tableau(FILE *f)
{
    fprintf(f, "[");
}

void json_fin_tableau(FILE *f)
{
    fprintf(f, "\n]\n");
}

/* Sépare les objets : virgule avant chaque objet sauf le premier, puis retour à la ligne */
static void separateur(FILE *f, int premier)
{
    fprintf(f, "%s\n  ", premier ? "" : ",");
}

int json_ecrire_piece(FILE *f, char *champs[], int premier)
{
    char *description = champs[1];                  /* "Extérieur - Terrasse" */
    char *tiret = strstr(description, " - ");

    if (tiret == NULL) {
        fprintf(stderr, "Pièce ignorée (pas de niveau) : %s\n", description);
        return 0;
    }
    *tiret = '\0';      /* description = "Extérieur", tiret + 3 = "Terrasse" */

    separateur(f, premier);
    fprintf(f, "{\"id\": %d, \"nom\": ", atoi(champs[0]));
    json_ecrire_chaine(f, tiret + 3);
    fprintf(f, ", \"niveau\": ");
    json_ecrire_chaine(f, description);
    fprintf(f, "}");
    return 1;
}

void json_ecrire_equipement(FILE *f, char *champs[], int nb, Section s, int premier)
{
    int etat = convertir_etat(champs[CHAMP_ETAT]);

    separateur(f, premier);
    fprintf(f, "{\"ip\": ");
    json_ecrire_chaine(f, champs[CHAMP_IP]);
    fprintf(f, ", \"numero\": ");
    json_ecrire_chaine(f, champs[CHAMP_NUMERO]);

    if (etat < 0) {
        fprintf(stderr, "État inconnu « %s » pour : %s\n",
                champs[CHAMP_ETAT], champs[CHAMP_DESCRIPTION]);
        fprintf(f, ", \"etat\": null");
    } else {
        fprintf(f, ", \"etat\": %d", etat);
    }

    fprintf(f, ", \"nom\": ");
    json_ecrire_chaine(f, apres_dernier_tiret(champs[CHAMP_DESCRIPTION]));
    fprintf(f, ", \"piece\": %d", atoi(champs[CHAMP_PIECE]));

    /* puissance : "500.0" -> 500, seulement pour les luminaires */
    if (s == SECTION_LUMINAIRES && nb > CHAMP_EXTRA)
        fprintf(f, ", \"puissance\": %d", (int) atof(champs[CHAMP_EXTRA]));
    else
        fprintf(f, ", \"puissance\": null");

    /* mode : 0 climatisation, 1 chauffage, seulement pour les climatisations */
    if (s == SECTION_CLIMATISATIONS && nb > CHAMP_EXTRA)
        fprintf(f, ", \"mode\": %d", atoi(champs[CHAMP_EXTRA]));
    else
        fprintf(f, ", \"mode\": null");

    fprintf(f, ", \"type\": ");
    json_ecrire_chaine(f, nom_type(s));
    fprintf(f, "}");
}