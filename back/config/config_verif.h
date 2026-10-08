/* ============================================================
   config_verif.h – Vérification des arguments (IP, port)
   ============================================================ */

#ifndef CONFIG_VERIF_H
#define CONFIG_VERIF_H

/* Renvoie 1 si texte est une adresse IPv4 valide ("192.168.56.1"), 0 sinon. */
int config_ip_valide(const char *texte);

/* Convertit texte en numéro de port (1 à 65535) et le range dans *port.
   Renvoie 0 si OK, -1 si le texte n'est pas un port valide. */
int config_texte_vers_port(const char *texte, int *port);

#endif /* CONFIG_VERIF_H */