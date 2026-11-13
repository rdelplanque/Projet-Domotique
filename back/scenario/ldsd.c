/* ============================================================
   ldsd.c – Lecture (analyse) d'un scénario LDSD (voir ldsd.h)

   Trois étapes, ligne par ligne :
     1. DÉCOUPER la ligne en mots : un texte entre guillemets compte
        pour un seul mot (« "Plan de travail sud" » = 1 mot) ;
     2. vérifier l'ORDRE des mots (le premier mot dit quel gabarit suivre) ;
     3. chercher les NOMS dans la maison et vérifier que l'action
        convient au type de l'équipement.
   ============================================================ */

#include <stdarg.h>         /* va_list (messages d'erreur)  */
#include <stdio.h>          /* snprintf, vsnprintf          */
#include <string.h>         /* strcmp, strlen, memcpy       */

#include "ldsd.h"

#define MAX_MOTS       24       /* mots par ligne au plus          */
#define TAILLE_MOT     160      /* un nom entre guillemets compris */
#define TAILLE_LIGNE   1024

/* Noms exacts des types dans la base (table type_equipement) */
static const char *TYPES_BASE[TYPE_NOMBRE] = {
    "Luminaire", "Prise commandée", "Climatisation réversible",
    "Volet roulant / porte basculante"
};
static const char *TYPES_SINGULIER[TYPE_NOMBRE] = {
    "luminaire", "prise", "climatisation", "volet"
};
static const char *TYPES_PLURIEL[TYPE_NOMBRE] = {
    "luminaires", "prises", "climatisations", "volets"
};
/* Les groupes : « tous les luminaires »... (3 mots chacun) */
static const char *GROUPES[TYPE_NOMBRE][3] = {
    { "tous",   "les", "luminaires" },
    { "toutes", "les", "prises" },
    { "toutes", "les", "climatisations" },
    { "tous",   "les", "volets" }
};
static const char *VERBES[] = { "allumer", "éteindre", "ouvrir", "fermer" };
static const char *JOURS[7] = {
    "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"
};


/* ------------------------------------------------------------
   1. Découper une ligne en mots
   ------------------------------------------------------------ */

typedef struct {
    int  guillemets;            /* 1 si le mot était entre guillemets (un nom) */
    char texte[TAILLE_MOT];
} Mot;

/* Une ligne découpée, et où on en est dans sa lecture */
typedef struct {
    int         numero;         /* numéro de la ligne dans le texte */
    int         nb;
    Mot         mots[MAX_MOTS];
    int         pos;            /* prochain mot à lire */
    ErreurLdsd *err;
} Ligne;


/* Remplit l'erreur : numéro de ligne + message (comme printf) */
static int erreur(ErreurLdsd *err, int ligne, const char *format, ...)
{
    va_list arguments;

    err->ligne = ligne;
    va_start(arguments, format);
    vsnprintf(err->message, sizeof err->message, format, arguments);
    va_end(arguments);
    return -1;
}

/* Découpe texte (une ligne, sans '\n') en mots. Renvoie 0 ou -1. */
static int decouper(const char *texte, Ligne *l)
{
    const char *p = texte;
    size_t longueur;

    l->nb = 0;
    l->pos = 0;
    while (*p != '\0') {
        const char *fin;
        Mot *mot;

        if (*p == ' ' || *p == '\t') {
            p++;
            continue;
        }
        if (l->nb == MAX_MOTS) {
            return erreur(l->err, l->numero, "ligne trop longue (%d mots au plus)", MAX_MOTS);
        }
        mot = &l->mots[l->nb];

        if (*p == '"') {
            /* Un nom : tout jusqu'au guillemet suivant, espaces compris */
            fin = strchr(p + 1, '"');
            if (fin == NULL) {
                return erreur(l->err, l->numero, "guillemet fermant manquant");
            }
            longueur = (size_t)(fin - (p + 1));
            if (longueur == 0) {
                return erreur(l->err, l->numero, "nom vide entre guillemets");
            }
            mot->guillemets = 1;
            p++;                                /* on saute le guillemet ouvrant */
            fin++;                              /* on reprendra après le fermant */
        } else {
            /* Un mot-clé : jusqu'au prochain espace ou guillemet */
            fin = p + strcspn(p, " \t\"");
            longueur = (size_t)(fin - p);
            mot->guillemets = 0;
        }
        if (longueur >= TAILLE_MOT) {
            return erreur(l->err, l->numero, "nom trop long (%d caractères au plus)", TAILLE_MOT - 1);
        }
        memcpy(mot->texte, p, longueur);
        mot->texte[longueur] = '\0';
        l->nb++;
        p = fin;
    }
    return 0;
}


/* ------------------------------------------------------------
   2. Lire les mots dans l'ordre
   ------------------------------------------------------------ */

/* Reste-t-il des mots à lire ? */
static int fini(const Ligne *l)
{
    return l->pos >= l->nb;
}

/* Le mot courant est-il le mot-clé m (sans guillemets) ? */
static int est(const Ligne *l, const char *m)
{
    return !fini(l) && !l->mots[l->pos].guillemets
           && strcmp(l->mots[l->pos].texte, m) == 0;
}

/* Si le mot courant est m, on le « mange » et on renvoie 1 */
static int prendre(Ligne *l, const char *m)
{
    if (est(l, m)) {
        l->pos++;
        return 1;
    }
    return 0;
}

/* Le mot déjà lu juste avant, pour les messages : « après "Salon" » */
static const char *precedent(const Ligne *l, char *tampon, size_t taille)
{
    const Mot *m;

    if (l->pos == 0) {
        snprintf(tampon, taille, "le début de la ligne");
    } else {
        m = &l->mots[l->pos - 1];
        snprintf(tampon, taille, m->guillemets ? "\"%s\"" : "« %s »", m->texte);
    }
    return tampon;
}

/* Le mot m DOIT venir maintenant, sinon erreur */
static int exiger(Ligne *l, const char *m)
{
    char avant[TAILLE_MOT + 8];

    if (prendre(l, m)) {
        return 0;
    }
    return erreur(l->err, l->numero, "« %s » attendu après %s",
                  m, precedent(l, avant, sizeof avant));
}

/* Plus aucun mot ne doit suivre */
static int exiger_fin(Ligne *l)
{
    char avant[TAILLE_MOT + 8];

    if (fini(l)) {
        return 0;
    }
    return erreur(l->err, l->numero, "fin de ligne attendue après %s",
                  precedent(l, avant, sizeof avant));
}

/* Un nom entre guillemets DOIT venir maintenant ; quoi = « d'équipement »... */
static const char *exiger_nom(Ligne *l, const char *quoi)
{
    char avant[TAILLE_MOT + 8];

    if (!fini(l) && l->mots[l->pos].guillemets) {
        return l->mots[l->pos++].texte;
    }
    erreur(l->err, l->numero, "nom %s entre guillemets attendu après %s",
           quoi, precedent(l, avant, sizeof avant));
    return NULL;
}


/* ------------------------------------------------------------
   Outils : dates, heures, jours
   ------------------------------------------------------------ */

static int est_chiffre(char c)
{
    return c >= '0' && c <= '9';
}

/* Les n caractères de t sont des chiffres : leur valeur, sinon -1 */
static int nombre(const char *t, int n)
{
    int v = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (!est_chiffre(t[i])) {
            return -1;
        }
        v = v * 10 + (t[i] - '0');
    }
    return v;
}

static int jours_dans_mois(int mois, int annee)
{
    static const int JOURS_MOIS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int bissextile = (annee % 4 == 0 && annee % 100 != 0) || annee % 400 == 0;

    if (mois == 2 && bissextile) {
        return 29;
    }
    return JOURS_MOIS[mois - 1];
}

/* « hh:mm » → heure et minute. Renvoie 0 ou -1 (erreur remplie). */
static int lire_heure(Ligne *l, EnTete *e)
{
    const char *t;

    if (fini(l) || l->mots[l->pos].guillemets) {
        return erreur(l->err, l->numero, "heure attendue au format hh:mm après « à »");
    }
    t = l->mots[l->pos].texte;
    if (strlen(t) != 5 || t[2] != ':' || nombre(t, 2) < 0 || nombre(t + 3, 2) < 0) {
        return erreur(l->err, l->numero, "heure attendue au format hh:mm (« %s » reçu)", t);
    }
    e->heure = nombre(t, 2);
    e->minute = nombre(t + 3, 2);
    if (e->heure > 23 || e->minute > 59) {
        return erreur(l->err, l->numero, "heure impossible « %s » (00:00 à 23:59)", t);
    }
    l->pos++;
    return 0;
}

/* « jj/mm/aaaa » → jour, mois, année. Renvoie 0 ou -1. */
static int lire_date(Ligne *l, EnTete *e)
{
    const char *t;

    if (fini(l) || l->mots[l->pos].guillemets) {
        return erreur(l->err, l->numero, "date attendue au format jj/mm/aaaa après « le »");
    }
    t = l->mots[l->pos].texte;
    if (strlen(t) != 10 || t[2] != '/' || t[5] != '/'
        || nombre(t, 2) < 0 || nombre(t + 3, 2) < 0 || nombre(t + 6, 4) < 0) {
        return erreur(l->err, l->numero, "date attendue au format jj/mm/aaaa (« %s » reçu)", t);
    }
    e->jour = nombre(t, 2);
    e->mois = nombre(t + 3, 2);
    e->annee = nombre(t + 6, 4);
    if (e->annee < 2000 || e->mois < 1 || e->mois > 12
        || e->jour < 1 || e->jour > jours_dans_mois(e->mois, e->annee)) {
        return erreur(l->err, l->numero, "le %s n'existe pas", t);
    }
    l->pos++;
    return 0;
}

/* Numéro du jour (0 = lundi), ou -1 si ce n'est pas un jour */
static int numero_jour(const Mot *m)
{
    int i;

    if (m->guillemets) {
        return -1;
    }
    for (i = 0; i < 7; i++) {
        if (strcmp(m->texte, JOURS[i]) == 0) {
            return i;
        }
    }
    return -1;
}


/* ------------------------------------------------------------
   L'en-tête
   ------------------------------------------------------------ */

static int lire_en_tete(Ligne *l, EnTete *e)
{
    int j;

    memset(e, 0, sizeof *e);

    /* au clic */
    if (prendre(l, "au")) {
        e->type = DECLENCHEUR_DIRECT;
        if (exiger(l, "clic") != 0) {
            return -1;
        }
        return exiger_fin(l);
    }

    /* le jj/mm/aaaa à hh:mm */
    if (prendre(l, "le")) {
        e->type = DECLENCHEUR_UNE_FOIS;
        if (lire_date(l, e) != 0 || exiger(l, "à") != 0 || lire_heure(l, e) != 0) {
            return -1;
        }
        return exiger_fin(l);
    }

    /* chaque <jour> [et <jour>...] à hh:mm */
    if (prendre(l, "chaque")) {
        e->type = DECLENCHEUR_CHAQUE;
        for (;;) {
            if (fini(l)) {
                return erreur(l->err, l->numero, "jour attendu (lundi à dimanche)");
            }
            j = numero_jour(&l->mots[l->pos]);
            if (j < 0) {
                return erreur(l->err, l->numero, "jour inconnu « %s » (lundi à dimanche)",
                              l->mots[l->pos].texte);
            }
            if (e->jours & (1u << j)) {
                return erreur(l->err, l->numero, "« %s » est déjà dans la liste", JOURS[j]);
            }
            e->jours |= (unsigned char)(1u << j);
            l->pos++;

            if (prendre(l, "et")) {
                continue;                   /* un jour de plus */
            }
            if (est(l, "à")) {
                break;
            }
            return erreur(l->err, l->numero, "« et » ou « à » attendu après « %s »", JOURS[j]);
        }
        if (exiger(l, "à") != 0 || lire_heure(l, e) != 0) {
            return -1;
        }
        return exiger_fin(l);
    }

    return erreur(l->err, l->numero,
                  "en-tête inconnu « %s » (attendu : au clic, le …, chaque …)",
                  l->mots[0].texte);
}


/* ------------------------------------------------------------
   3. Chercher les noms dans la maison
   ------------------------------------------------------------ */

static const Etage *chercher_etage(const Maison *m, const char *nom)
{
    int i;

    for (i = 0; i < m->nb_etages; i++) {
        if (strcmp(m->etages[i].nom, nom) == 0) {
            return &m->etages[i];
        }
    }
    return NULL;
}

static const Piece *chercher_piece(const Maison *m, const char *nom, long long id_etage)
{
    int i;

    for (i = 0; i < m->nb_pieces; i++) {
        if (m->pieces[i].id_etage == id_etage && strcmp(m->pieces[i].nom, nom) == 0) {
            return &m->pieces[i];
        }
    }
    return NULL;
}

static const Equipement *chercher_equipement(const Maison *m, const char *nom, long long id_piece)
{
    int i;

    for (i = 0; i < m->nb_equipements; i++) {
        if (m->equipements[i].id_piece == id_piece && strcmp(m->equipements[i].nom, nom) == 0) {
            return &m->equipements[i];
        }
    }
    return NULL;
}

/* Type d'un équipement (TYPE_...), ou -1 si inconnu */
static int type_de(const Equipement *e)
{
    int t;

    for (t = 0; t < TYPE_NOMBRE; t++) {
        if (strcmp(e->type, TYPES_BASE[t]) == 0) {
            return t;
        }
    }
    return -1;
}

/* Étage de la pièce id_piece (0 si pièce inconnue) */
static long long etage_de_piece(const Maison *m, long long id_piece)
{
    int i;

    for (i = 0; i < m->nb_pieces; i++) {
        if (m->pieces[i].id == id_piece) {
            return m->pieces[i].id_etage;
        }
    }
    return 0;
}

int ldsd_dans_cible(const Maison *m, const Cible *c, const Equipement *e)
{
    if (c->id_equipement > 0) {
        return e->id == c->id_equipement;
    }
    return type_de(e) == (int)c->type
           && (c->id_piece == 0 || e->id_piece == c->id_piece)
           && (c->id_etage == 0 || etage_de_piece(m, e->id_piece) == c->id_etage);
}


/* ------------------------------------------------------------
   Une ligne d'action
   ------------------------------------------------------------ */

/* "Salon" de "Rez-de-chaussée" : la pièce et l'étage nommés.
   Remplit *piece. Renvoie 0 ou -1. */
static int lire_piece_et_etage(Ligne *l, const Maison *m, const Piece **piece)
{
    const char *nom_piece;
    const char *nom_etage;
    const Etage *etage;

    nom_piece = exiger_nom(l, "de pièce");
    if (nom_piece == NULL || exiger(l, "de") != 0) {
        return -1;
    }
    if (est(l, "tous")) {
        return erreur(l->err, l->numero,
                      "une pièce nommée demande un étage nommé (le même nom existe sur plusieurs étages)");
    }
    nom_etage = exiger_nom(l, "d'étage");
    if (nom_etage == NULL) {
        return -1;
    }
    etage = chercher_etage(m, nom_etage);
    if (etage == NULL) {
        return erreur(l->err, l->numero, "étage inconnu \"%s\"", nom_etage);
    }
    *piece = chercher_piece(m, nom_piece, etage->id);
    if (*piece == NULL) {
        return erreur(l->err, l->numero, "pas de pièce \"%s\" à l'étage \"%s\"",
                      nom_piece, nom_etage);
    }
    return 0;
}

/* Lit la cible : "X" de "P" de "E", ou un groupe. Renvoie 0 ou -1. */
static int lire_cible(Ligne *l, const Maison *m, Cible *c)
{
    const Piece *piece;
    const Equipement *e;
    const char *nom;
    int t;

    memset(c, 0, sizeof *c);

    /* Un équipement nommé : "X" de "P" de "E" */
    if (!fini(l) && l->mots[l->pos].guillemets) {
        nom = l->mots[l->pos++].texte;
        if (exiger(l, "de") != 0) {
            return -1;
        }
        if (est(l, "toutes")) {
            return erreur(l->err, l->numero,
                          "un équipement nommé demande une pièce et un étage nommés");
        }
        if (lire_piece_et_etage(l, m, &piece) != 0) {
            return -1;
        }
        e = chercher_equipement(m, nom, piece->id);
        if (e == NULL) {
            return erreur(l->err, l->numero, "pas d'équipement \"%s\" dans \"%s\"", nom, piece->nom);
        }
        t = type_de(e);
        if (t < 0) {
            return erreur(l->err, l->numero, "type d'équipement inconnu \"%s\"", e->type);
        }
        c->id_equipement = e->id;
        c->type = (TypeEquipement)t;
        return 0;
    }

    /* Un groupe : tous les luminaires... */
    for (t = 0; t < TYPE_NOMBRE; t++) {
        if (l->pos + 3 <= l->nb && est(l, GROUPES[t][0])
            && !l->mots[l->pos + 1].guillemets && strcmp(l->mots[l->pos + 1].texte, GROUPES[t][1]) == 0
            && !l->mots[l->pos + 2].guillemets && strcmp(l->mots[l->pos + 2].texte, GROUPES[t][2]) == 0) {
            break;
        }
    }
    if (t == TYPE_NOMBRE) {
        return erreur(l->err, l->numero,
                      "nom d'équipement entre guillemets ou groupe attendu après « %s »"
                      " (tous les luminaires, toutes les prises, toutes les climatisations, tous les volets)",
                      l->mots[0].texte);
    }
    l->pos += 3;
    c->type = (TypeEquipement)t;
    if (exiger(l, "de") != 0) {
        return -1;
    }

    /* ... de "P" de "E" */
    if (!fini(l) && l->mots[l->pos].guillemets) {
        if (lire_piece_et_etage(l, m, &piece) != 0) {
            return -1;
        }
        c->id_piece = piece->id;
        c->id_etage = piece->id_etage;
        return 0;
    }

    /* ... de toutes les pièces de "E" | de tous les étages */
    if (!(prendre(l, "toutes") && prendre(l, "les") && prendre(l, "pièces"))) {
        return erreur(l->err, l->numero,
                      "nom de pièce entre guillemets ou « toutes les pièces » attendu après « de »");
    }
    if (exiger(l, "de") != 0) {
        return -1;
    }
    if (prendre(l, "tous")) {
        if (exiger(l, "les") != 0 || exiger(l, "étages") != 0) {
            return -1;
        }
        return 0;                           /* toute la maison : pièce 0, étage 0 */
    }
    nom = exiger_nom(l, "d'étage");
    if (nom == NULL) {
        return -1;
    }
    if (chercher_etage(m, nom) == NULL) {
        return erreur(l->err, l->numero, "étage inconnu \"%s\"", nom);
    }
    c->id_etage = chercher_etage(m, nom)->id;
    return 0;
}

/* Où est le groupe, en clair : « dans Salon », « à l'étage Premier étage »... */
static void decrire_lieu(const Maison *m, const Cible *c, char *dest, size_t taille)
{
    int i;

    if (c->id_piece != 0) {
        for (i = 0; i < m->nb_pieces; i++) {
            if (m->pieces[i].id == c->id_piece) {
                snprintf(dest, taille, "dans \"%s\"", m->pieces[i].nom);
                return;
            }
        }
    }
    if (c->id_etage != 0) {
        for (i = 0; i < m->nb_etages; i++) {
            if (m->etages[i].id == c->id_etage) {
                snprintf(dest, taille, "à l'étage \"%s\"", m->etages[i].nom);
                return;
            }
        }
    }
    snprintf(dest, taille, "dans la maison");
}

static int lire_action(Ligne *l, const Maison *m, Action *a)
{
    char lieu[200];
    int v;
    int i;

    /* Le verbe */
    for (v = 0; v < 4; v++) {
        if (prendre(l, VERBES[v])) {
            break;
        }
    }
    if (v == 4) {
        return erreur(l->err, l->numero,
                      "action inconnue « %s » (allumer, éteindre, ouvrir, fermer)",
                      l->mots[0].texte);
    }
    a->ligne = l->numero;
    a->verbe = (Verbe)v;
    a->mode = VALEUR_ABSENTE;

    /* La cible */
    if (lire_cible(l, m, &a->cible) != 0) {
        return -1;
    }

    /* L'action convient-elle au type ? */
    if (a->cible.type == TYPE_VOLET && (a->verbe == VERBE_ALLUMER || a->verbe == VERBE_ETEINDRE)) {
        return erreur(l->err, l->numero,
                      "pour un volet ou une porte, utiliser « ouvrir » ou « fermer »");
    }
    if (a->cible.type != TYPE_VOLET && (a->verbe == VERBE_OUVRIR || a->verbe == VERBE_FERMER)) {
        return erreur(l->err, l->numero, "« %s » est réservé aux volets et portes", VERBES[v]);
    }

    /* Le mode (facultatif) : en chauffage / en climatisation */
    if (prendre(l, "en")) {
        if (a->verbe != VERBE_ALLUMER) {
            return erreur(l->err, l->numero, "le mode n'est possible qu'avec « allumer »");
        }
        if (a->cible.type != TYPE_CLIMATISATION) {
            return erreur(l->err, l->numero, "le mode est réservé aux climatisations");
        }
        if (prendre(l, "chauffage")) {
            a->mode = 1;
        } else if (prendre(l, "climatisation")) {
            a->mode = 0;
        } else {
            return erreur(l->err, l->numero, "mode attendu après « en » : chauffage ou climatisation");
        }
    }
    if (exiger_fin(l) != 0) {
        return -1;
    }

    /* Combien d'équipements ? Un groupe vide est une erreur. */
    a->nb_equipements = 0;
    for (i = 0; i < m->nb_equipements; i++) {
        if (ldsd_dans_cible(m, &a->cible, &m->equipements[i])) {
            a->nb_equipements++;
        }
    }
    if (a->nb_equipements == 0) {
        decrire_lieu(m, &a->cible, lieu, sizeof lieu);
        return erreur(l->err, l->numero, "pas de %s %s",
                      TYPES_PLURIEL[a->cible.type], lieu);
    }
    return 0;
}


/* ------------------------------------------------------------
   Le texte complet
   ------------------------------------------------------------ */

/* Où en est-on dans le texte ? */
typedef enum { ATTENTE_MARQUE_EN_TETE, ATTENTE_EN_TETE, ATTENTE_MARQUE_LISTE, ACTIONS } Etape;

/* La ligne est-elle exactement « mot : » ? */
static int est_marque(const Ligne *l, const char *mot)
{
    return l->nb == 2 && !l->mots[0].guillemets && strcmp(l->mots[0].texte, mot) == 0
           && !l->mots[1].guillemets && strcmp(l->mots[1].texte, ":") == 0;
}

int ldsd_lire(const char *texte, const Maison *m, Programme *p, ErreurLdsd *err)
{
    static Ligne l;             /* environ 4 Ko : hors de la pile */
    char brut[TAILLE_LIGNE];
    const char *debut = texte;
    Etape etape = ATTENTE_MARQUE_EN_TETE;
    int numero = 0;
    int ligne_liste = 0;

    memset(p, 0, sizeof *p);
    err->ligne = 0;
    err->message[0] = '\0';
    l.err = err;

    while (*debut != '\0') {
        const char *fin = strchr(debut, '\n');
        size_t longueur = (fin != NULL) ? (size_t)(fin - debut) : strlen(debut);
        char *t;

        numero++;
        if (longueur >= sizeof brut) {
            return erreur(err, numero, "ligne trop longue (%zu caractères au plus)", sizeof brut - 1);
        }
        memcpy(brut, debut, longueur);
        brut[longueur] = '\0';
        debut = (fin != NULL) ? fin + 1 : debut + longueur;

        /* \r de fin (texte tapé sous Windows), espaces de début */
        if (longueur > 0 && brut[longueur - 1] == '\r') {
            brut[longueur - 1] = '\0';
        }
        t = brut + strspn(brut, " \t");

        if (*t == '\0' || *t == '#') {
            continue;                       /* ligne vide ou commentaire */
        }

        l.numero = numero;
        if (decouper(t, &l) != 0) {
            return -1;
        }
        if (l.nb == 0) {
            continue;
        }

        switch (etape) {
        case ATTENTE_MARQUE_EN_TETE:
            if (!est_marque(&l, "En-tête")) {
                return erreur(err, numero, "« En-tête : » attendu en première ligne");
            }
            etape = ATTENTE_EN_TETE;
            break;

        case ATTENTE_EN_TETE:
            if (est_marque(&l, "Liste")) {
                return erreur(err, numero, "en-tête manquant avant « Liste : »");
            }
            if (lire_en_tete(&l, &p->en_tete) != 0) {
                return -1;
            }
            etape = ATTENTE_MARQUE_LISTE;
            break;

        case ATTENTE_MARQUE_LISTE:
            if (!est_marque(&l, "Liste")) {
                return erreur(err, numero, "« Liste : » attendu après l'en-tête (un seul en-tête par scénario)");
            }
            ligne_liste = numero;
            etape = ACTIONS;
            break;

        case ACTIONS:
            if (est_marque(&l, "En-tête") || est_marque(&l, "Liste")) {
                return erreur(err, numero, "« %s : » déjà donné plus haut", l.mots[0].texte);
            }
            if (p->nb_actions == LDSD_MAX_ACTIONS) {
                return erreur(err, numero, "trop d'actions (%d au plus)", LDSD_MAX_ACTIONS);
            }
            if (lire_action(&l, m, &p->actions[p->nb_actions]) != 0) {
                return -1;
            }
            p->nb_actions++;
            break;
        }
    }

    /* Fin du texte : qu'est-ce qui manque ? */
    switch (etape) {
    case ATTENTE_MARQUE_EN_TETE: return erreur(err, numero > 0 ? numero : 1, "le scénario est vide");
    case ATTENTE_EN_TETE:        return erreur(err, numero, "en-tête manquant après « En-tête : »");
    case ATTENTE_MARQUE_LISTE:   return erreur(err, numero, "« Liste : » manquant après l'en-tête");
    case ACTIONS:
        if (p->nb_actions == 0) {
            return erreur(err, ligne_liste, "la liste ne contient aucune action");
        }
        break;
    }
    return 0;
}


/* ------------------------------------------------------------
   Ce que le serveur a compris, en clair
   ------------------------------------------------------------ */

void ldsd_decrire_en_tete(const EnTete *e, char *dest, size_t taille)
{
    size_t n;
    int premier = 1;
    int j;

    switch (e->type) {
    case DECLENCHEUR_DIRECT:
        snprintf(dest, taille, "au clic");
        return;
    case DECLENCHEUR_UNE_FOIS:
        snprintf(dest, taille, "le %02d/%02d/%04d à %02d:%02d",
                 e->jour, e->mois, e->annee, e->heure, e->minute);
        return;
    case DECLENCHEUR_CHAQUE:
        snprintf(dest, taille, "chaque");
        for (j = 0; j < 7; j++) {
            if (e->jours & (1u << j)) {
                n = strlen(dest);
                snprintf(dest + n, taille - n, "%s %s", premier ? "" : " et", JOURS[j]);
                premier = 0;
            }
        }
        n = strlen(dest);
        snprintf(dest + n, taille - n, " à %02d:%02d", e->heure, e->minute);
        return;
    }
}

void ldsd_decrire_action(const Maison *m, const Action *a, char *dest, size_t taille)
{
    char lieu[200];
    const char *mode = "";
    int i;

    if (a->mode == 1) {
        mode = " en chauffage";
    } else if (a->mode == 0) {
        mode = " en climatisation";
    }

    if (a->cible.id_equipement > 0) {
        for (i = 0; i < m->nb_equipements; i++) {
            const Equipement *e = &m->equipements[i];
            int k;

            if (e->id != a->cible.id_equipement) {
                continue;
            }
            for (k = 0; k < m->nb_pieces && m->pieces[k].id != e->id_piece; k++) {
            }
            snprintf(dest, taille, "%s \"%s\" de \"%s\"%s", VERBES[a->verbe], e->nom,
                     k < m->nb_pieces ? m->pieces[k].nom : "?", mode);
            return;
        }
        snprintf(dest, taille, "%s l'équipement n° %lld%s", VERBES[a->verbe],
                 a->cible.id_equipement, mode);
        return;
    }

    decrire_lieu(m, &a->cible, lieu, sizeof lieu);
    snprintf(dest, taille, "%s %d %s %s%s", VERBES[a->verbe], a->nb_equipements,
             a->nb_equipements > 1 ? TYPES_PLURIEL[a->cible.type] : TYPES_SINGULIER[a->cible.type],
             lieu, mode);
}


/* ------------------------------------------------------------
   Noms lisibles
   ------------------------------------------------------------ */

const char *ldsd_nom_verbe(Verbe v)
{
    return ((int)v >= 0 && (int)v < 4) ? VERBES[v] : "?";
}

const char *ldsd_nom_type_pluriel(TypeEquipement t)
{
    return ((int)t >= 0 && (int)t < TYPE_NOMBRE) ? TYPES_PLURIEL[t] : "?";
}

const char *ldsd_nom_jour(int numero)
{
    return (numero >= 0 && numero < 7) ? JOURS[numero] : "?";
}
