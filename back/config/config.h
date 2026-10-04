/* ============================================================
   config.h – Module de configuration du serveur R-Domotik
   (la « promesse » : ce que le module offre aux autres)

   Deux sources de réglages :
     - arguments : IP et port de SimDom (le port change à chaque
       lancement du simulateur), chemin du fichier config.json ;
     - config.json : base de données et paramètres du serveur.
   ============================================================ */

#ifndef CONFIG_H
#define CONFIG_H

/* Taille maximale des textes lus dans config.json (avec le '\0') */
#define CONFIG_TAILLE_TEXTE 128

/* Tous les réglages du serveur, rassemblés dans une seule structure.
   Remplie une fois au démarrage, puis seulement LUE par les autres modules. */
typedef struct {
    /* --- SimDom (arguments de la ligne de commande) --- */
    char simdom_ip[16];          /* "255.255.255.255" = 15 caractères + '\0' */
    int  simdom_port;

    /* --- Base de données (bloc "bdd" de config.json) --- */
    char bdd_hote[CONFIG_TAILLE_TEXTE];
    int  bdd_port;
    char bdd_nom[CONFIG_TAILLE_TEXTE];
    char bdd_utilisateur[CONFIG_TAILLE_TEXTE];
    char bdd_mot_de_passe[CONFIG_TAILLE_TEXTE];

    /* --- Serveur (bloc "serveur" de config.json) --- */
    int port_api;                /* port d'écoute de l'API (nginx renvoie /api ici) */
    int intervalle_relecture_s;  /* relecture de l'état des équipements, en secondes */
    int delai_reponse_ms;        /* au-delà, un équipement passe « indéterminé » */
} Config;

/* Lit les arguments et le fichier config.json, vérifie tout et remplit *cfg.
   Arguments attendus : <ip_simdom> <port_simdom> <fichier_config.json>
   Renvoie 0 si tout est bon, -1 sinon (les erreurs sont affichées sur stderr). */
int config_charger(int argc, char *argv[], Config *cfg);

/* Affiche un résumé de la configuration (mot de passe masqué). */
void config_afficher(const Config *cfg);

#endif /* CONFIG_H */