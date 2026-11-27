/* ============================================================
   planificateur.c – Les scénarios programmés (voir planificateur.h)

   Le temps est compté en MINUTES depuis le 01/01/1970 (time() / 60) :
   « une minute a passé » = ce nombre a augmenté. Si la boucle a tardé
   (plusieurs minutes d'un coup), chaque minute manquée est examinée,
   dans l'ordre : aucun scénario n'est oublié.
   ============================================================ */

#include <stdio.h>          /* printf, snprintf              */
#include <string.h>         /* memset                        */
#include <time.h>           /* time, localtime_r, mktime     */

#include "planificateur.h"
#include "execution.h"
#include "../bdd/bdd_scenario.h"

#define PREFIXE "[planificateur] "

/* Au-delà, des minutes manquées d'un coup (machine en veille...) sont
   abandonnées, comme une exécution manquée serveur arrêté. */
#define RATTRAPAGE_MAX_MIN 60

/* Un scénario programmé actif, gardé en mémoire : son en-tête suffit */
typedef struct {
    long long id;
    char      nom[101];
    EnTete    en_tete;
} Prevu;

/* Un scénario dans la file d'attente */
typedef struct {
    long long id;
    char      nom[101];
} EnAttente;

static PGconn *connexion = NULL;

static Prevu     prevus[SCENARIO_MAX];
static int       nb_prevus = 0;

static EnAttente file[SCENARIO_MAX];        /* file[0] partira le premier */
static int       nb_file = 0;

static long long derniere_minute = 0;       /* dernière minute examinée */

/* La liste lue dans la base : environ 530 Ko, hors de la pile */
static ListeScenarios liste;


/* ------------------------------------------------------------
   Le temps en minutes
   ------------------------------------------------------------ */

static long long minute_actuelle(void)
{
    return (long long)(time(NULL) / 60);
}

/* La minute (depuis 1970) d'un en-tête « une fois », en heure locale */
static long long minute_de(const EnTete *e)
{
    struct tm t;

    memset(&t, 0, sizeof t);
    t.tm_year  = e->annee - 1900;
    t.tm_mon   = e->mois - 1;
    t.tm_mday  = e->jour;
    t.tm_hour  = e->heure;
    t.tm_min   = e->minute;
    t.tm_isdst = -1;                        /* heure d'été : à mktime de voir */
    return (long long)(mktime(&t) / 60);
}

int planificateur_heure_passee(const EnTete *e)
{
    /* derniere_minute : la minute en cours est déjà examinée, ou va l'être
       sans ce scénario. Avant le démarrage : l'heure actuelle. */
    long long reference = (derniere_minute > 0) ? derniere_minute : minute_actuelle();

    return e->type == DECLENCHEUR_UNE_FOIS && minute_de(e) <= reference;
}


/* ------------------------------------------------------------
   La file d'attente
   ------------------------------------------------------------ */

int planificateur_en_attente(long long id)
{
    int i;

    for (i = 0; i < nb_file; i++) {
        if (file[i].id == id) {
            return 1;
        }
    }
    return 0;
}

/* Enlève la case i de la file (les suivantes avancent d'une place) */
static void enlever(int i)
{
    for (; i < nb_file - 1; i++) {
        file[i] = file[i + 1];
    }
    nb_file--;
}

void planificateur_retirer(long long id)
{
    int i;

    for (i = 0; i < nb_file; i++) {
        if (file[i].id == id) {
            printf(PREFIXE "« %s » retiré de la file d'attente\n", file[i].nom);
            enlever(i);
            return;
        }
    }
}

static void mettre_en_file(const Prevu *p)
{
    if (planificateur_en_attente(p->id) || execution_en_cours() == p->id) {
        printf(PREFIXE "« %s » est déjà en attente ou en cours : ignoré\n", p->nom);
        return;
    }
    if (nb_file == SCENARIO_MAX) {
        printf(PREFIXE "file d'attente pleine : « %s » ignoré\n", p->nom);
        return;
    }
    file[nb_file].id = p->id;
    snprintf(file[nb_file].nom, sizeof file[nb_file].nom, "%s", p->nom);
    nb_file++;
    printf(PREFIXE "« %s » : c'est l'heure, mis dans la file (position %d)\n", p->nom, nb_file);
}


/* ------------------------------------------------------------
   Les programmés actifs, en mémoire
   ------------------------------------------------------------ */

/* Enlève la case i de prevus */
static void oublier(int i)
{
    for (; i < nb_prevus - 1; i++) {
        prevus[i] = prevus[i + 1];
    }
    nb_prevus--;
}

/* Désactive un « une fois » dans la base (il est parti, ou son heure est passée) */
static void desactiver(const Prevu *p, const char *pourquoi)
{
    if (bdd_ecrire_actif(connexion, p->id, 0) < 0) {
        fprintf(stderr, PREFIXE "« %s » : désactivation impossible (base)\n", p->nom);
    }
    printf(PREFIXE "« %s » désactivé (%s)\n", p->nom, pourquoi);
}

void planificateur_recharger(void)
{
    ErreurLdsd err;
    int i;

    if (bdd_lire_scenarios(connexion, &liste) != 0) {
        fprintf(stderr, PREFIXE "base inaccessible : liste des programmés inchangée\n");
        return;
    }

    nb_prevus = 0;
    for (i = 0; i < liste.nb; i++) {
        const Scenario *s = &liste.scenarios[i];
        Prevu *p = &prevus[nb_prevus];

        if (!s->actif || !s->utilisable) {
            continue;
        }
        if (ldsd_lire_en_tete(s->script, &p->en_tete, &err) != 0
            || p->en_tete.type == DECLENCHEUR_DIRECT) {
            continue;           /* pas un programmé lisible : rien à planifier */
        }
        p->id = s->id;
        snprintf(p->nom, sizeof p->nom, "%s", s->nom);

        if (planificateur_heure_passee(&p->en_tete)) {
            desactiver(p, "heure passée : exécution manquée");
            continue;
        }
        nb_prevus++;
    }
    printf(PREFIXE "%d scénario(s) programmé(s) actif(s)\n", nb_prevus);
}

void planificateur_initialiser(PGconn *conn)
{
    connexion = conn;
    derniere_minute = minute_actuelle();
    planificateur_recharger();
}


/* ------------------------------------------------------------
   Chaque minute
   ------------------------------------------------------------ */

/* Le scénario p doit-il partir à la minute décrite par *t ? */
static int c_est_l_heure(const Prevu *p, const struct tm *t)
{
    const EnTete *e = &p->en_tete;
    int jour = (t->tm_wday + 6) % 7;        /* tm_wday : 0 = dimanche ; ici 0 = lundi */

    if (e->heure != t->tm_hour || e->minute != t->tm_min) {
        return 0;
    }
    if (e->type == DECLENCHEUR_CHAQUE) {
        return (e->jours & (1u << jour)) != 0;
    }
    return e->type == DECLENCHEUR_UNE_FOIS
           && e->annee == t->tm_year + 1900 && e->mois == t->tm_mon + 1 && e->jour == t->tm_mday;
}

/* Met dans la file tous les programmés de la minute m */
static void examiner_minute(long long m)
{
    time_t secondes = (time_t)(m * 60);
    struct tm t;
    int i;

    localtime_r(&secondes, &t);
    i = 0;
    while (i < nb_prevus) {
        if (!c_est_l_heure(&prevus[i], &t)) {
            i++;
            continue;
        }
        mettre_en_file(&prevus[i]);
        if (prevus[i].en_tete.type == DECLENCHEUR_UNE_FOIS) {
            desactiver(&prevus[i], "« une fois » : parti");
            oublier(i);                     /* la case i contient maintenant le suivant */
        } else {
            i++;
        }
    }
}

/* Lance les scénarios de la file, tant que le moteur est libre */
static void lancer_suivant(void)
{
    char message[EXECUTION_TAILLE_MESSAGE];
    EnAttente premier;

    while (nb_file > 0 && execution_en_cours() == 0) {
        premier = file[0];
        enlever(0);
        if (execution_lancer(premier.id, 1, "le planificateur", message, sizeof message)
            != LANCEMENT_OK) {
            printf(PREFIXE "« %s » non lancé : %s\n", premier.nom, message);
        }
    }
}

void planificateur_avancer(void)
{
    long long maintenant = minute_actuelle();
    long long m;

    /* 1. Les minutes passées depuis le dernier tour (en général une seule) */
    if (maintenant > derniere_minute) {
        m = derniere_minute + 1;
        if (maintenant - m >= RATTRAPAGE_MAX_MIN) {
            printf(PREFIXE "%lld minute(s) sans examen : les plus anciennes sont abandonnées\n",
                   maintenant - m);
            m = maintenant - RATTRAPAGE_MAX_MIN + 1;
        }
        for (; m <= maintenant; m++) {
            examiner_minute(m);
        }
        derniere_minute = maintenant;
    }
    /* (horloge reculée : rien, on attend qu'elle repasse derniere_minute) */

    /* 2. Le moteur est libre : au suivant */
    lancer_suivant();
    fflush(stdout);
}
