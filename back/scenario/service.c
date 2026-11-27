/* ============================================================
   service.c – Les règles des scénarios (voir service.h)
   ============================================================ */

#include <stdio.h>          /* printf, snprintf, vsnprintf */
#include <stdarg.h>         /* va_list                     */

#include "service.h"
#include "ldsd.h"
#include "execution.h"
#include "planificateur.h"
#include "scenario.h"
#include "../bdd/bdd_scenario.h"
#include "../bdd/bdd_maison.h"

#define PREFIXE "[scénario] "

static PGconn *connexion = NULL;

/* Gros objets, hors de la pile (maison ~120 Ko, scénario 8 Ko...) */
static Maison    maison_lue;
static Programme programme;
static Scenario  un_scenario;


/* ------------------------------------------------------------
   Entrée du module
   ------------------------------------------------------------ */

void scenario_initialiser(PGconn *conn)
{
    connexion = conn;
    scenario_routes_initialiser(conn);
    execution_initialiser(conn);
    planificateur_initialiser(conn);
}

int scenario_avancer(void)
{
    planificateur_avancer();            /* l'heure d'un programmé ? moteur libre ? */
    return execution_avancer();         /* le scénario en cours, une tranche */
}


/* ------------------------------------------------------------
   Petits outils
   ------------------------------------------------------------ */

/* Remplit le résultat (message comme printf). Renvoie l'issue. */
static Issue issue(Resultat *r, Issue i, const char *format, ...)
{
    va_list arguments;

    r->issue = i;
    r->ligne = 0;
    va_start(arguments, format);
    vsnprintf(r->message, sizeof r->message, format, arguments);
    va_end(arguments);
    return i;
}

/* Lit le scénario id dans un_scenario.
   Renvoie ISSUE_OK, ou ISSUE_INCONNU / ISSUE_BASE (résultat rempli). */
static Issue lire_scenario(long long id, Resultat *r)
{
    int trouve = bdd_lire_scenario(connexion, id, &un_scenario);

    if (trouve < 0) {
        return issue(r, ISSUE_BASE, "erreur de la base de données");
    }
    if (trouve == 1) {
        return issue(r, ISSUE_INCONNU, "scénario inconnu");
    }
    return ISSUE_OK;
}

/* Le texte est-il correct ? Lu avec la maison actuelle, rangé dans programme.
   avant : début du message d'erreur ("" ou "erreur dans le scénario, ").
   Renvoie ISSUE_OK, ou ISSUE_REFUS (avec la ligne fautive) / ISSUE_BASE. */
static Issue texte_correct(const char *texte, const char *avant, Resultat *r)
{
    ErreurLdsd err;

    if (bdd_lire_maison(connexion, 0, &maison_lue) != 0) {
        return issue(r, ISSUE_BASE, "erreur de la base de données");
    }
    if (ldsd_lire(texte, &maison_lue, &programme, &err) != 0) {
        issue(r, ISSUE_REFUS, "%sligne %d : %s", avant, err.ligne, err.message);
        r->ligne = err.ligne;
        return ISSUE_REFUS;
    }
    return ISSUE_OK;
}

/* Résultat d'une écriture dans la base (0 fait, 1 rien touché, -1 erreur) */
static Issue ecriture(int resultat, Resultat *r, const char *si_rien)
{
    if (resultat < 0) {
        return issue(r, ISSUE_BASE, "erreur de la base de données");
    }
    if (resultat == 1) {
        return issue(r, ISSUE_INCONNU, "%s", si_rien);
    }
    return issue(r, ISSUE_OK, "");
}


/* ------------------------------------------------------------
   Créer, modifier, supprimer
   ------------------------------------------------------------ */

void service_creer(const char *nom, const char *texte, long long qui,
                   long long *id, Resultat *r)
{
    int resultat;

    if (texte_correct(texte, "", r) != ISSUE_OK) {
        return;
    }
    resultat = bdd_creer_scenario(connexion, nom, texte, qui, id);
    if (resultat < 0) {
        issue(r, ISSUE_BASE, "erreur de la base de données");
        return;
    }
    if (resultat == 1) {
        issue(r, ISSUE_REFUS, "ce nom est déjà pris par un autre scénario");
        return;
    }
    printf(PREFIXE "n° %lld « %s » créé par l'utilisateur %lld\n", *id, nom, qui);
    issue(r, ISSUE_OK, "");
}

void service_modifier(long long id, const char *nom, const char *texte, long long qui,
                      Resultat *r)
{
    int resultat;

    if (lire_scenario(id, r) != ISSUE_OK) {
        return;
    }
    if (execution_en_cours() == id) {
        issue(r, ISSUE_REFUS, "scénario en cours : arrêtez-le avant de le modifier");
        return;
    }
    if (texte_correct(texte, "", r) != ISSUE_OK) {
        return;
    }
    resultat = bdd_modifier_scenario(connexion, id, nom, texte);
    if (resultat < 0) {
        issue(r, ISSUE_BASE, "erreur de la base de données");
        return;
    }
    if (resultat == 1) {
        /* Il existe (lu plus haut) : c'est donc le nom qui est pris */
        issue(r, ISSUE_REFUS, "ce nom est déjà pris par un autre scénario");
        return;
    }
    printf(PREFIXE "n° %lld « %s » modifié par l'utilisateur %lld (repasse non utilisable)\n",
           id, nom, qui);
    planificateur_retirer(id);
    planificateur_recharger();          /* il n'est plus actif */
    issue(r, ISSUE_OK, "");
}

void service_supprimer(long long id, long long qui, Resultat *r)
{
    if (lire_scenario(id, r) != ISSUE_OK) {
        return;
    }
    if (execution_en_cours() == id) {
        issue(r, ISSUE_REFUS, "scénario en cours : arrêtez-le avant de le supprimer");
        return;
    }
    if (ecriture(bdd_supprimer_scenario(connexion, id), r, "scénario inconnu") != ISSUE_OK) {
        return;
    }
    printf(PREFIXE "n° %lld « %s » supprimé par l'utilisateur %lld\n", id, un_scenario.nom, qui);
    planificateur_retirer(id);
    planificateur_recharger();
}


/* ------------------------------------------------------------
   Utilisable, actif
   ------------------------------------------------------------ */

void service_utilisable(long long id, int utilisable, long long qui, Resultat *r)
{
    if (lire_scenario(id, r) != ISSUE_OK) {
        return;
    }

    if (utilisable) {
        /* Rendre utilisable : seulement un texte correct */
        if (texte_correct(un_scenario.script, "erreur dans le scénario, ", r) != ISSUE_OK) {
            return;
        }
    } else {
        /* Non utilisable : il ne doit plus tourner, ni attendre son tour */
        execution_arreter_si(id, "arrêté (rendu non utilisable)");
        planificateur_retirer(id);
    }

    if (ecriture(bdd_ecrire_utilisable(connexion, id, utilisable), r, "scénario inconnu")
        != ISSUE_OK) {
        return;
    }
    printf(PREFIXE "n° %lld rendu %s par l'utilisateur %lld\n", id,
           utilisable ? "utilisable" : "non utilisable", qui);
    planificateur_recharger();          /* non utilisable = désactivé */
}

void service_activer(long long id, int actif, long long qui, Resultat *r)
{
    EnTete e;

    if (lire_scenario(id, r) != ISSUE_OK) {
        return;
    }

    if (actif) {
        if (!un_scenario.utilisable) {
            issue(r, ISSUE_REFUS, "scénario non utilisable");
            return;
        }
        if (texte_correct(un_scenario.script, "erreur dans le scénario, ", r) != ISSUE_OK) {
            return;
        }
        e = programme.en_tete;
        if (e.type == DECLENCHEUR_DIRECT) {
            issue(r, ISSUE_REFUS, "scénario direct : il se lance avec « Lancer »");
            return;
        }
        if (planificateur_heure_passee(&e)) {
            issue(r, ISSUE_REFUS, "le %02d/%02d/%04d à %02d:%02d est déjà passé",
                  e.jour, e.mois, e.annee, e.heure, e.minute);
            return;
        }
    } else {
        planificateur_retirer(id);
    }

    if (ecriture(bdd_ecrire_actif(connexion, id, actif), r, "scénario inconnu") != ISSUE_OK) {
        return;
    }
    printf(PREFIXE "n° %lld « %s » %s par l'utilisateur %lld\n", id, un_scenario.nom,
           actif ? "activé" : "désactivé", qui);
    planificateur_recharger();
}


/* ------------------------------------------------------------
   Lancer, arrêter (scénarios directs, ou programmé en cours)
   ------------------------------------------------------------ */

void service_lancer(long long id, long long qui, Resultat *r)
{
    char qui_texte[48];

    snprintf(qui_texte, sizeof qui_texte, "l'utilisateur %lld", qui);
    switch (execution_lancer(id, 0, qui_texte, r->message, sizeof r->message)) {
    case LANCEMENT_OK:          r->issue = ISSUE_OK;      break;
    case LANCEMENT_INCONNU:     r->issue = ISSUE_INCONNU; break;
    case LANCEMENT_BASE:        r->issue = ISSUE_BASE;    break;
    case LANCEMENT_MAISON:      r->issue = ISSUE_MAISON;  break;
    default:                    r->issue = ISSUE_REFUS;   break;  /* occupé, non utilisable... */
    }
    r->ligne = 0;
}

void service_arreter(long long id, long long qui, Resultat *r)
{
    char pourquoi[64];

    snprintf(pourquoi, sizeof pourquoi, "arrêté par l'utilisateur %lld", qui);
    if (!execution_arreter_si(id, pourquoi)) {
        issue(r, ISSUE_REFUS, "ce scénario n'est pas en cours");
        return;
    }
    issue(r, ISSUE_OK, "");
}
