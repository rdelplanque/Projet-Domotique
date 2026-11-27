/* ============================================================
   execution.c – Le moteur : exécute un scénario (voir execution.h)

   Pour chaque équipement visé, comme une commande de la page Ma maison :
     1. relire son état RÉEL dans SimDom ;
     2. en panne ou muet : on saute (noté dans le journal) ;
        déjà dans l'état voulu : on n'envoie rien (SimDom est lent) ;
     3. sinon commander, relire pour confirmer, écrire l'état dans la base.
   ============================================================ */

#include <stdio.h>          /* printf, snprintf */
#include <time.h>           /* clock_gettime    */

#include "execution.h"
#include "ldsd.h"
#include "../bdd/bdd_maison.h"
#include "../bdd/bdd_scenario.h"
#include "../simdom/simdom.h"

#define PREFIXE "[scénario] "

/* Travail fait à chaque appel de execution_avancer, avant de rendre la
   main au guichet. Un équipement = 3 échanges SimDom (~150 à 300 ms). */
#define BUDGET_MS 250

static PGconn *connexion = NULL;

/* Le scénario en cours. static : gros (maison ~120 Ko, programme...),
   et il doit survivre d'un appel de execution_avancer au suivant. */
static int        en_cours = 0;
static long long  id_en_cours = 0;
static char       nom_en_cours[101];
static Scenario   scenario;
static Maison     maison_exec;
static Programme  programme;

/* Où en est-on ? action_courante dans programme.actions, et pour cette
   action, la liste de ses équipements (cases de maison_exec.equipements). */
static int action_courante;
static int cibles[MAISON_MAX_EQUIPEMENTS];
static int nb_cibles;
static int cible_courante;

/* Le bilan, affiché à la fin */
static struct timespec debut;
static int nb_commandes, nb_deja, nb_sautes;


void execution_initialiser(PGconn *conn)
{
    connexion = conn;
}

long long execution_en_cours(void)
{
    return en_cours ? id_en_cours : 0;
}

const char *execution_nom_en_cours(void)
{
    return en_cours ? nom_en_cours : "";
}

static long ecoule_ms(const struct timespec *depuis)
{
    struct timespec maintenant;

    clock_gettime(CLOCK_MONOTONIC, &maintenant);
    return (maintenant.tv_sec - depuis->tv_sec) * 1000
         + (maintenant.tv_nsec - depuis->tv_nsec) / 1000000;
}


/* ------------------------------------------------------------
   Déroulement
   ------------------------------------------------------------ */

/* Prépare l'action courante : la liste de ses équipements */
static void preparer_action(void)
{
    const Action *a = &programme.actions[action_courante];
    char texte[350];
    int i;

    nb_cibles = 0;
    cible_courante = 0;
    for (i = 0; i < maison_exec.nb_equipements; i++) {
        if (ldsd_dans_cible(&maison_exec, &a->cible, &maison_exec.equipements[i])) {
            cibles[nb_cibles++] = i;
        }
    }
    ldsd_decrire_action(&maison_exec, a, texte, sizeof texte);
    printf(PREFIXE "ligne %d : %s\n", a->ligne, texte);
}

/* Fin du scénario (normale ou non) : bilan dans le journal */
static void terminer(const char *comment)
{
    printf(PREFIXE "« %s » %s en %ld ms : %d commande(s), %d déjà dans l'état voulu, %d sautée(s)\n",
           nom_en_cours, comment, ecoule_ms(&debut), nb_commandes, nb_deja, nb_sautes);
    en_cours = 0;
    id_en_cours = 0;
}

/* Nom de la pièce d'un équipement, pour le journal */
static const char *piece_de(const Equipement *e)
{
    int i;

    for (i = 0; i < maison_exec.nb_pieces; i++) {
        if (maison_exec.pieces[i].id == e->id_piece) {
            return maison_exec.pieces[i].nom;
        }
    }
    return "?";
}

int execution_arreter_si(long long id, const char *pourquoi)
{
    if (en_cours && id == id_en_cours) {
        terminer(pourquoi);
        return 1;
    }
    return 0;
}

/* Exécute l'action a sur l'équipement e.
   Renvoie 0 (fait, déjà fait ou sauté), -1 si SimDom est perdu. */
static int executer_un(const Action *a, const Equipement *e)
{
    AdresseSimdom ad;
    int etat_voulu = (a->verbe == VERBE_ALLUMER || a->verbe == VERBE_OUVRIR) ? 1 : 0;
    int mode_voulu = 0;
    int etat;
    int mode = e->mode;

    ad.ip = e->ip;
    ad.entree = e->entree;
    ad.clim = (a->cible.type == TYPE_CLIMATISATION);

    /* 1. État réel avant la commande */
    etat = simdom_lire(&ad, &mode);
    if (etat == SIMDOM_DECONNECTE) {
        return -1;
    }
    if (etat == SIMDOM_SANS_REPONSE) {
        bdd_ecrire_etat(connexion, e->id, VALEUR_ABSENTE, ad.clim ? mode : VALEUR_ABSENTE);
        printf(PREFIXE "  %s (%s) : ne répond pas, sauté\n", e->nom, piece_de(e));
        nb_sautes++;
        return 0;
    }
    if (etat == 2) {
        bdd_ecrire_etat(connexion, e->id, 2, ad.clim ? mode : VALEUR_ABSENTE);
        printf(PREFIXE "  %s (%s) : en panne, sauté\n", e->nom, piece_de(e));
        nb_sautes++;
        return 0;
    }

    /* 2. Le mode d'une clim : celui demandé, sinon on garde l'actuel */
    if (ad.clim) {
        mode_voulu = (a->mode != VALEUR_ABSENTE) ? a->mode : mode;
    }
    if (etat == etat_voulu && (!ad.clim || mode == mode_voulu)) {
        nb_deja++;
        return 0;
    }

    /* 3. Commander, relire pour confirmer, enregistrer */
    if (simdom_commander(&ad, etat_voulu, mode_voulu) != 0) {
        return -1;
    }
    etat = simdom_lire(&ad, &mode);
    if (etat == SIMDOM_DECONNECTE) {
        return -1;
    }
    if (etat == SIMDOM_SANS_REPONSE) {
        etat = VALEUR_ABSENTE;
    }
    bdd_ecrire_etat(connexion, e->id, etat, ad.clim ? mode : VALEUR_ABSENTE);
    if (etat != etat_voulu || (ad.clim && mode != mode_voulu)) {
        printf(PREFIXE "  %s (%s) : commande non prise en compte\n", e->nom, piece_de(e));
        nb_sautes++;
    } else {
        nb_commandes++;
    }
    return 0;
}

int execution_avancer(void)
{
    struct timespec tranche;

    if (!en_cours) {
        return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &tranche);

    while (en_cours && ecoule_ms(&tranche) < BUDGET_MS) {
        /* Action finie : on passe à la suivante, ou c'est la fin */
        if (cible_courante >= nb_cibles) {
            action_courante++;
            if (action_courante >= programme.nb_actions) {
                terminer("terminé");
                break;
            }
            preparer_action();
            continue;
        }

        if (executer_un(&programme.actions[action_courante],
                        &maison_exec.equipements[cibles[cible_courante]]) != 0) {
            fprintf(stderr, PREFIXE "SimDom perdu\n");
            terminer("arrêté (SimDom perdu)");
            break;
        }
        cible_courante++;
    }
    fflush(stdout);
    return en_cours;
}


/* ------------------------------------------------------------
   Le lancement
   ------------------------------------------------------------ */

Lancement execution_lancer(long long id, int par_planificateur, const char *qui,
                           char *message, size_t taille)
{
    ErreurLdsd err;
    int trouve;

    message[0] = '\0';
    if (en_cours) {
        snprintf(message, taille, "« %s » est déjà en cours : un seul scénario à la fois",
                 nom_en_cours);
        return LANCEMENT_OCCUPE;
    }

    /* 1. Le scénario, tel qu'il est dans la base maintenant */
    trouve = bdd_lire_scenario(connexion, id, &scenario);
    if (trouve < 0) {
        snprintf(message, taille, "erreur de la base de données");
        return LANCEMENT_BASE;
    }
    if (trouve == 1) {
        snprintf(message, taille, "scénario inconnu");
        return LANCEMENT_INCONNU;
    }
    if (!scenario.utilisable) {
        snprintf(message, taille, "scénario non utilisable");
        return LANCEMENT_NON_UTILISABLE;
    }

    /* 2. La maison, puis la lecture du texte (il a pu changer, la maison aussi) */
    if (bdd_lire_maison(connexion, 0, &maison_exec) != 0) {
        snprintf(message, taille, "erreur de la base de données");
        return LANCEMENT_BASE;
    }
    if (ldsd_lire(scenario.script, &maison_exec, &programme, &err) != 0) {
        snprintf(message, taille, "erreur dans le scénario, ligne %d : %s",
                 err.ligne, err.message);
        return LANCEMENT_ERREUR_TEXTE;
    }
    if (!par_planificateur && programme.en_tete.type != DECLENCHEUR_DIRECT) {
        snprintf(message, taille, "scénario programmé : il se lance avec « Activer »");
        return LANCEMENT_MAUVAIS_TYPE;
    }
    if (par_planificateur && programme.en_tete.type == DECLENCHEUR_DIRECT) {
        snprintf(message, taille, "scénario direct : il se lance avec « Lancer »");
        return LANCEMENT_MAUVAIS_TYPE;
    }

    /* 3. SimDom doit répondre */
    if (!simdom_est_connecte() && simdom_connecter() != 0) {
        snprintf(message, taille, "maison non connectée");
        return LANCEMENT_MAISON;
    }

    /* 4. C'est parti : la boucle fera le travail (execution_avancer) */
    en_cours = 1;
    id_en_cours = scenario.id;
    snprintf(nom_en_cours, sizeof nom_en_cours, "%s", scenario.nom);
    nb_commandes = nb_deja = nb_sautes = 0;
    clock_gettime(CLOCK_MONOTONIC, &debut);
    action_courante = 0;
    printf(PREFIXE "« %s » lancé par %s (%d action(s))\n",
           nom_en_cours, qui, programme.nb_actions);
    preparer_action();
    return LANCEMENT_OK;
}
