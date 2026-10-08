/* ============================================================
   config_fichier.h – Lecture d'un fichier texte en mémoire
   ============================================================ */

#ifndef CONFIG_FICHIER_H
#define CONFIG_FICHIER_H

/* Lit tout le fichier chemin en mémoire.
   Renvoie un texte terminé par '\0' (à libérer avec free),
   ou NULL en cas d'erreur (message déjà affiché sur stderr). */
char *config_lire_fichier(const char *chemin);

#endif /* CONFIG_FICHIER_H */