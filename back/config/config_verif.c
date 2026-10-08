/* ============================================================
   config_verif.c – Vérification des arguments (IP, port)
   ============================================================ */

#include <stdlib.h>         /* strtol                        */
#include <errno.h>          /* errno (débordement de strtol) */
#include <arpa/inet.h>      /* inet_pton                     */

#include "config_verif.h"

/* inet_pton essaie de convertir le texte en adresse binaire :
   il renvoie 1 seulement si le texte est une IPv4 correcte. */
int config_ip_valide(const char *texte)
{
    struct in_addr adresse;
    return inet_pton(AF_INET, texte, &adresse) == 1;
}

/* strtol au lieu de atoi : atoi("abc") renvoie 0 sans prévenir,
   strtol nous dit où la lecture s'est arrêtée (fin). */
int config_texte_vers_port(const char *texte, int *port)
{
    char *fin;
    long valeur;

    errno = 0;
    valeur = strtol(texte, &fin, 10);

    if (fin == texte              /* aucun chiffre lu        */
        || *fin != '\0'           /* il reste du texte après */
        || errno != 0             /* nombre beaucoup trop grand */
        || valeur < 1 || valeur > 65535) {
        return -1;
    }
    *port = (int)valeur;
    return 0;
}