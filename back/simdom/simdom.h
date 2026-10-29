/* ============================================================
   simdom.h – Dialogue avec le simulateur SimDom (la « maison »)
   Une seule connexion TCP, ouverte au démarrage et gardée ouverte.
   Messages binaires de 13 octets (14 pour une climatisation) :
     octets 1-4   IP de l'automate     (192.168.0.100 → c0 a8 00 64)
     octets 5-12  entrée, un octet par bit ("00010111" → 00 00 00 01 00 01 01 01)
     octet 13     code : 00 éteindre, 01 allumer, 04 lire l'état
     octet 14     mode (climatisations) : 00 climatisation, 01 chauffage
   Voir claude/protocole-simdom.md.
   ============================================================ */

#ifndef SIMDOM_H
#define SIMDOM_H

/* Réponses de simdom_lire quand il n'y a pas d'état */
#define SIMDOM_SANS_REPONSE   (-1)  /* pas de réponse dans le délai : « indéterminé » */
#define SIMDOM_DECONNECTE     (-2)  /* pas de connexion avec SimDom                  */

/* Nombre maximal d'équipements lus en une rafale (simdom_lire_tout) */
#define SIMDOM_MAX_RAFALE     512

/* L'adresse d'un équipement dans SimDom, telle que rangée dans la base */
typedef struct {
    const char *ip;         /* "192.168.0.100" (ip_equipement)       */
    const char *entree;     /* "00010111"      (numero_equipement)   */
    int         clim;       /* 1 = climatisation : message de 14 octets */
} AdresseSimdom;

/* Retient où joindre SimDom et le délai d'attente d'une réponse.
   À appeler une fois au démarrage (dans main), avant tout le reste. */
void simdom_initialiser(const char *ip, int port, int delai_ms);

/* Ouvre la connexion. Renvoie 0 si connecté, -1 sinon (message affiché). */
int simdom_connecter(void);

/* 1 si la connexion est ouverte, 0 sinon. */
int simdom_est_connecte(void);

/* Envoie une commande : etat 0 (éteindre / baisser) ou 1 (allumer / monter).
   mode (climatisations seulement) : 0 climatisation, 1 chauffage.
   SimDom ne répond rien à une commande.
   Renvoie 0 si le message est parti, -1 sinon (adresse invalide, coupure...). */
int simdom_commander(const AdresseSimdom *a, int etat, int mode);

/* Lit l'état d'un équipement (code 04).
   Renvoie 0, 1 ou 2 (éteint, allumé, en panne) et range le mode dans *mode
   pour une climatisation ; ou SIMDOM_SANS_REPONSE / SIMDOM_DECONNECTE. */
int simdom_lire(const AdresseSimdom *a, int *mode);

/* Lit l'état de n équipements EN RAFALE, comme le client officiel de SimDom :
   toutes les demandes partent d'un coup, puis les réponses sont lues dans
   l'ordre où elles arrivent et rangées grâce à leur adresse.
   Remplit etats[i] (0, 1, 2 ou SIMDOM_SANS_REPONSE) et, pour une
   climatisation, modes[i] (les autres cases de modes ne sont pas touchées).
   On arrête d'attendre quand SimDom reste muet pendant le délai.
   Renvoie 0, ou SIMDOM_DECONNECTE (etats[] alors sans valeur fiable). */
int simdom_lire_tout(const AdresseSimdom adresses[], int n, int etats[], int modes[]);

/* Ferme la connexion (arrêt du serveur). */
void simdom_fermer(void);

#endif /* SIMDOM_H */
