/* ============================================================
   execution.c – Exécution d'un scénario direct (voir execution.h)

   Pour chaque équipement visé, comme une commande de la page Ma maison :
     1. relire son état RÉEL dans SimDom ;
     2. en panne ou muet : on saute (noté dans le journal) ;
        déjà dans l'état voulu : on n'envoie rien (SimDom est lent) ;
     3. sinon commander, relire pour confirmer, écrire l'état dans la base.
   ============================================================ */

#include <stdio.h>          /* printf, snprintf */
#include <string.h>         /* strcmp           */
#include <time.h>           /* clock_gettime    */
#include <cjson/cJSON.h>

#include "execution.h"
#include "ldsd.h"
#include "../bdd/bdd_maison.h"
#include "../bdd/bdd_scenario.h"
#include "../simdom/simdom.h"
#include "../auth/session.h"

#define PREFIXE "[scénario] "

/* Travail fait à chaque appel de scenario_avancer, avant de rendre la
   main au guichet. Un équipement = 3 échanges SimDom (~150 à 300 ms). */
#define BUDGET_MS 250

static PGconn *connexion = NULL;

/* Le scénario en cours. static : gros (maison ~120 Ko, programme...),
   et il doit survivre d'un appel de scenario_avancer au suivant. */
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

long long scenario_en_cours(void)
{
    return en_cours ? id_en_cours : 0;
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

void execution_arreter_si(long long id, const char *pourquoi)
{
    if (en_cours && id == id_en_cours) {
        terminer(pourquoi);
    }
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

int scenario_avancer(void)
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
   Les routes
   ------------------------------------------------------------ */

/* Lit {"id":3} dans le corps. Renvoie l'id, ou 0 si absent ou invalide. */
static long long lire_id(const Requete *req)
{
    cJSON *json;
    const cJSON *id;
    long long valeur = 0;

    if (req->corps == NULL || (json = cJSON_Parse(req->corps)) == NULL) {
        return 0;
    }
    id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if (cJSON_IsNumber(id) && id->valuedouble >= 1) {
        valeur = (long long)id->valuedouble;
    }
    cJSON_Delete(json);
    return valeur;
}

static void repondre_ok(Reponse *rep, long long id)
{
    char json[64];

    snprintf(json, sizeof json, "{\"ok\":true,\"id\":%lld}", id);
    http_reponse_json(rep, 200, json);
}

void scenario_lancer(const Requete *req, Reponse *rep)
{
    long long id = lire_id(req);
    ErreurLdsd err;
    char message[LDSD_TAILLE_MESSAGE + 64];
    int trouve;

    if (id == 0) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...}");
        return;
    }
    if (en_cours) {
        snprintf(message, sizeof message, "« %s » est déjà en cours : un seul scénario direct à la fois",
                 nom_en_cours);
        http_reponse_erreur(rep, 409, message);
        return;
    }

    /* 1. Le scénario, tel qu'il est dans la base maintenant */
    trouve = bdd_lire_scenario(connexion, id, &scenario);
    if (trouve < 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }
    if (trouve == 1) {
        http_reponse_erreur(rep, 404, "scénario inconnu");
        return;
    }
    if (!scenario.utilisable) {
        http_reponse_erreur(rep, 409, "scénario non utilisable");
        return;
    }

    /* 2. La maison, puis la lecture du texte (il a pu changer, la maison aussi) */
    if (bdd_lire_maison(connexion, 0, &maison_exec) != 0) {
        http_reponse_erreur(rep, 500, "erreur de la base de données");
        return;
    }
    if (ldsd_lire(scenario.script, &maison_exec, &programme, &err) != 0) {
        snprintf(message, sizeof message, "erreur dans le scénario, ligne %d : %s",
                 err.ligne, err.message);
        http_reponse_erreur(rep, 409, message);
        return;
    }
    if (programme.en_tete.type != DECLENCHEUR_DIRECT) {
        http_reponse_erreur(rep, 409, "scénario programmé : il se lance avec « Activer »");
        return;
    }

    /* 3. SimDom doit répondre */
    if (!simdom_est_connecte() && simdom_connecter() != 0) {
        http_reponse_erreur(rep, 503, "maison non connectée");
        return;
    }

    /* 4. C'est parti : la boucle fera le travail (scenario_avancer) */
    en_cours = 1;
    id_en_cours = scenario.id;
    snprintf(nom_en_cours, sizeof nom_en_cours, "%s", scenario.nom);
    nb_commandes = nb_deja = nb_sautes = 0;
    clock_gettime(CLOCK_MONOTONIC, &debut);
    action_courante = 0;
    printf(PREFIXE "« %s » lancé par l'utilisateur %lld (%d action(s))\n",
           nom_en_cours, req->session->id_utilisateur, programme.nb_actions);
    preparer_action();

    repondre_ok(rep, id);
}

void scenario_arreter(const Requete *req, Reponse *rep)
{
    long long id = lire_id(req);

    if (id == 0) {
        http_reponse_erreur(rep, 400, "corps JSON attendu : {\"id\":...}");
        return;
    }
    if (!en_cours || id != id_en_cours) {
        http_reponse_erreur(rep, 409, "ce scénario n'est pas en cours");
        return;
    }
    printf(PREFIXE "arrêt demandé par l'utilisateur %lld\n", req->session->id_utilisateur);
    terminer("arrêté");
    repondre_ok(rep, id);
}
