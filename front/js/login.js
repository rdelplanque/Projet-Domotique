/* ==========================================================
   login.js – Logique de la page de connexion (index.html)

   1. Intercepte l'envoi du formulaire (sinon : 405 de nginx)
   2. Envoie {email, mot_de_passe} en JSON à POST /api/login
   3. Succès  → garde le jeton et va sur pages/accueil.html
      Échec   → affiche le message dans #message-erreur
   ========================================================== */

"use strict";   // le navigateur signale plus d'erreurs (variables mal écrites...)

/* --- Les éléments de la page dont on a besoin --- */
const formulaire   = document.getElementById("form-login");
const champEmail   = document.getElementById("email");
const champMdp     = document.getElementById("mot_de_passe");
const zoneErreur   = document.getElementById("message-erreur");
const bouton       = document.querySelector('button[form="form-login"]');

const PAGE_ACCUEIL = "/pages/accueil.html";


/* --- Déjà connecté (jeton gardé) ? On va directement à l'accueil --- */
if (sessionStorage.getItem("jeton")) {
  location.replace(PAGE_ACCUEIL);
}


/* --- Petits outils d'affichage --- */
function afficherErreur(texte) {
  zoneErreur.textContent = texte;   // textContent : jamais de HTML injecté
  zoneErreur.hidden = false;
}

function cacherErreur() {
  zoneErreur.textContent = "";
  zoneErreur.hidden = true;
}

function boutonEnAttente(attente) {
  bouton.disabled = attente;        // évite les doubles clics
  bouton.textContent = attente ? "Connexion…" : "Se connecter";
}

// "e-mail ou mot de passe incorrect" → "E-mail ou mot de passe incorrect"
function majuscule(texte) {
  return texte.charAt(0).toUpperCase() + texte.slice(1);
}


/* --- Envoi du formulaire --- */
formulaire.addEventListener("submit", async (evenement) => {
  // Empêche l'envoi « classique » du navigateur (rechargement de page → 405)
  evenement.preventDefault();

  cacherErreur();
  boutonEnAttente(true);

  try {
    // fetch envoie la requête HTTP ; await attend la réponse sans bloquer la page
    const reponse = await fetch("/api/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        email: champEmail.value.trim(),       // trim : retire les espaces autour
        mot_de_passe: champMdp.value          // le mot de passe : tel quel
      })
    });

    // Le serveur C répond toujours en JSON ; nginx (502) répond en HTML
    let donnees = null;
    try {
      donnees = await reponse.json();
    } catch {
      donnees = null;
    }

    if (reponse.ok && donnees && donnees.jeton) {
      // Succès : on garde la session pour les autres pages.
      // sessionStorage : effacé à la fermeture de l'onglet.
      sessionStorage.setItem("jeton",  donnees.jeton);
      sessionStorage.setItem("profil", donnees.profil);
      sessionStorage.setItem("prenom", donnees.prenom);
      sessionStorage.setItem("nom",    donnees.nom);
      location.replace(PAGE_ACCUEIL);   // replace : « retour » ne revient pas au login
      return;
    }

    // Échec : on choisit le message à afficher
    if (reponse.status === 502 || reponse.status === 504) {
      afficherErreur("Le serveur ne répond pas. Réessayez dans un instant.");
    } else if (donnees && donnees.erreur) {
      afficherErreur(majuscule(donnees.erreur));      // 401, 403, 400...
    } else {
      afficherErreur("Erreur inattendue (code " + reponse.status + ").");
    }

  } catch (erreur) {
    // fetch lui-même a échoué : réseau coupé, VM éteinte...
    afficherErreur("Impossible de joindre le serveur. Vérifiez votre connexion.");
  }

  boutonEnAttente(false);
  champMdp.value = "";      // on efface le mot de passe après un échec
  champMdp.focus();
});