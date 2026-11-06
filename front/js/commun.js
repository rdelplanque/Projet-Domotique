/* ============================================================
   commun.js – Ce qui est commun à toutes les pages internes
   1. Vérifie le jeton auprès du serveur (/api/moi)
   2. Fabrique l'en-tête dans <header class="entete"></header>
   3. Bouton Retour (vers data-retour, sinon vers l'accueil)
   4. Bouton Déconnexion
   5. Affiche les éléments [data-droit] autorisés
   6. Renvoie à l'accueil si la page demande un droit absent
   La page se décrit sur <body> :
     data-page="accueil" | "maison" | ...   (obligatoire)
     data-titre="Scénarios"                  (titre de l'en-tête)
     data-retour="/pages/scenarios.html"     (facultatif)
     data-droit="consulter"                  (facultatif : droit pour voir la page)
   Besoin de session.js, chargé avant.
   Le script propre à la page attend la fin des contrôles avec :
     pagePrete.then(function (ok) { if (ok) { ... } });
   ============================================================ */

const ACCUEIL = "/pages/accueil.html";

/* Les droits de l'utilisateur, donnés par le serveur.
   Les scripts des pages peuvent s'en servir : droits.includes("piloter") */
let droits = [];


/* Petit outil : crée une balise avec une classe et un texte.
   textContent (et pas innerHTML) : un prénom ne peut pas injecter de HTML. */
function element(balise, classe, texte) {
    const el = document.createElement(balise);
    if (classe) {
        el.className = classe;
    }
    if (texte) {
        el.textContent = texte;
    }
    return el;
}


/* Bouton Déconnexion : prévenir le serveur, puis retour au login.
   Même si le serveur ne répond pas, on se déconnecte côté page. */
async function deconnecter() {
    await api("POST", "/api/logout");
    retourLogin();
}


/* l'en-tête */
function fabriquerEntete() {
    const entete = document.querySelector("header.entete");
    const page = document.body.dataset.page;
    const logo = element("img", "entete-logo");
    const bouton = element("button", "entete-deconnexion", "Déconnexion");
    let retour;

    if (entete === null) {
        return;
    }
    logo.src = "/images/logo.jpg";
    logo.alt = "R-Domotik";
    bouton.type = "button";
    bouton.addEventListener("click", deconnecter);

    if (page === "accueil") {
        /* Accueil : logo + « Bienvenue Prénom » + Déconnexion */
        const prenom = sessionStorage.getItem("prenom") || "";
        entete.append(logo, element("h1", "entete-titre", "Bienvenue " + prenom), bouton);
    } else {
        /* Autres pages : Retour + logo + titre + Déconnexion */
        retour = element("a", "entete-retour", "← Retour");
        retour.href = document.body.dataset.retour || ACCUEIL;
        entete.append(retour, logo, element("h1", "entete-titre", document.body.dataset.titre || ""), bouton);
    }
}


/* 5 : montrer ce que l'utilisateur a le droit de voir.
   Tout ce qui a data-droit est caché (hidden) dans le HTML.
   Rappel : ce n'est que de l'affichage, le serveur protège (401/403). */
function afficherSelonDroits() {
    document.querySelectorAll("[data-droit]").forEach(function (el) {
        if (droits.includes(el.dataset.droit)) {
            el.hidden = false;
        }
    });

    /* Un groupe dont toutes les cartes sont cachées disparaît aussi
       (ex. « Administration » pour un Utilisateur). */
    document.querySelectorAll(".groupe").forEach(function (groupe) {
        groupe.hidden = groupe.querySelector("[data-droit]:not([hidden])") === null;
    });
}


/* Message en haut de la page si le serveur ne répond pas */
function afficherErreur(texte) {
    const message = element("p", "message-erreur-page", texte);
    document.body.prepend(message);
}


/* 1 : point de départ, au chargement de la page.
   Renvoie true si la page peut s'afficher, false sinon. */
async function demarrerPage() {
    let resultat;
    const droitPage = document.body.dataset.droit;

    if (!sessionStorage.getItem("jeton")) {
        retourLogin();                  /* pas de bracelet du tout */
        return false;
    }

    resultat = await api("GET", "/api/moi");
    if (resultat.code === 401) {
        return false;                   /* api() renvoie déjà au login */
    }
    if (resultat.code !== 200 || resultat.donnees === null) {
        afficherErreur("Le serveur ne répond pas. Réessayez dans un instant.");
        return false;
    }

    droits = resultat.donnees.droits || [];

    /* 6 : page ouverte en tapant son adresse sans en avoir le droit.
       Simple confort : le serveur refuse de toute façon les données (403). */
    if (droitPage && !droits.includes(droitPage)) {
        location.replace(ACCUEIL);
        return false;
    }

    fabriquerEntete();
    afficherSelonDroits();
    return true;
}

/* Une « promesse » : le résultat de demarrerPage, disponible plus tard.
   Les scripts des pages s'en servent pour démarrer APRÈS les contrôles. */
const pagePrete = demarrerPage();