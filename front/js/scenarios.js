/* ============================================================
   scenarios.js – Page « Scénarios » : la liste des scénarios
   Une carte par scénario, copiée du <template id="modele-scenario">.
   Pour l'instant : afficher la liste et le texte d'un scénario,
   le vérifier (le serveur le lit et dit ce qu'il a compris),
   lancer et arrêter un scénario direct, le rendre (non) utilisable (admin),
   ouvrir l'éditeur (Modifier → scenario.html?id=...).
   La liste est relue toutes les 2 s (« En cours » → « Arrêté »).
   Les autres boutons arriveront avec leurs routes (message « bientôt »).
   Besoin de session.js (api) et commun.js (droits, pagePrete).
   ============================================================ */

const message = document.querySelector(".maison-message");
const listeCartes = document.querySelector(".scenarios-liste");
const texteVide = document.querySelector(".scenarios-vide");
const modele = document.getElementById("modele-scenario");
const fenetreScript = document.querySelector(".fenetre-script");
const fenetreSupprimer = document.querySelector(".fenetre-supprimer");
const consoleVerif = document.querySelector(".fenetre-console");

const INTERVALLE_MS = 2000;     /* relecture de la liste */

const BIENTOT = "Cette action n'est pas encore disponible : elle arrive avec la suite du module scénarios.";

/* Le scénario dont la fenêtre « Supprimer » est ouverte */
let aSupprimer = null;

/* Dernière liste reçue, en texte : on ne redessine que si elle a changé */
let dernierTexte = "";


/* ------------------------------------------------------------
   Petits outils
   ------------------------------------------------------------ */

function montrerMessage(texte) {
    message.textContent = texte;
    message.hidden = false;
}

function cacherMessage() {
    message.hidden = true;
}

/* Une étiquette : texte + couleur facultative ("vert", "orange") */
function remplirEtiquette(etiquette, texte, couleur) {
    etiquette.textContent = texte;
    if (couleur) {
        etiquette.classList.add(couleur);
    }
}

function texteType(type) {
    if (type === "direct") {
        return "Direct";
    }
    if (type === "programme") {
        return "Programmé";
    }
    return "Type inconnu";          /* pas de ligne après « En-tête : » */
}


/* ------------------------------------------------------------
   Une carte
   ------------------------------------------------------------ */

/* Copie le modèle, le remplit avec le scénario s et montre
   les boutons qui conviennent à son type, son état et au profil. */
function carteScenario(s) {
    const copie = modele.content.cloneNode(true);       /* copie du <template> */
    const carte = copie.querySelector(".carte-scenario");
    const admin = droits.includes("scenarios_admin");

    function bouton(action) {
        return carte.querySelector('[data-action="' + action + '"]');
    }

    carte.querySelector(".scenario-nom").textContent = s.nom;
    carte.querySelector(".scenario-createur").textContent = "Créé par " + s.createur;

    remplirEtiquette(carte.querySelector(".scenario-type"), texteType(s.type));
    remplirEtiquette(carte.querySelector(".scenario-utilisable"),
                     s.utilisable ? "Utilisable" : "Non utilisable",
                     s.utilisable ? "vert" : "orange");
    if (s.en_cours) {
        remplirEtiquette(carte.querySelector(".scenario-etat"), "En cours", "vert");
    } else if (s.actif) {
        remplirEtiquette(carte.querySelector(".scenario-etat"), "Actif", "vert");
    } else {
        remplirEtiquette(carte.querySelector(".scenario-etat"), "Arrêté");
    }

    /* Afficher : le texte du scénario dans la fenêtre */
    bouton("afficher").addEventListener("click", function () {
        fenetreScript.querySelector(".fenetre-titre").textContent = s.nom;
        fenetreScript.querySelector(".fenetre-texte").textContent = s.script;
        consoleVerif.hidden = true;             /* résultat du scénario précédent effacé */
        fenetreScript.showModal();
    });

    /* Lancer / Arrêter (direct) ou Activer / Désactiver (programmé) :
       seulement si le scénario est utilisable */
    if (s.utilisable && s.type === "direct") {
        bouton(s.en_cours ? "arreter" : "lancer").hidden = false;
    }
    if (s.utilisable && s.type === "programme") {
        bouton(s.actif ? "desactiver" : "activer").hidden = false;
    }

    /* Administrateur : rendre (non) utilisable, supprimer */
    if (admin) {
        bouton("utilisable").hidden = false;
        bouton("utilisable").textContent = s.utilisable ? "Rendre non utilisable" : "Rendre utilisable";
        bouton("utilisable").addEventListener("click", function () {
            commander(bouton("utilisable"), "/api/scenarios/utilisable",
                      { id: s.id, utilisable: !s.utilisable });
        });
        bouton("supprimer").hidden = false;
        bouton("supprimer").addEventListener("click", function () {
            aSupprimer = s;
            fenetreSupprimer.querySelector(".fenetre-nom").textContent = s.nom;
            fenetreSupprimer.showModal();
        });
    }

    /* Modifier : l'éditeur, sur ce scénario */
    bouton("modifier").href = "/pages/scenario.html?id=" + s.id;

    /* Lancer / Arrêter : le serveur exécute le scénario */
    bouton("lancer").addEventListener("click", function () {
        commander(bouton("lancer"), "/api/scenarios/lancer", { id: s.id });
    });
    bouton("arreter").addEventListener("click", function () {
        commander(bouton("arreter"), "/api/scenarios/arreter", { id: s.id });
    });

    /* Pas encore de route côté serveur pour ces boutons */
    ["activer", "desactiver"].forEach(function (action) {
        bouton(action).addEventListener("click", function () {
            montrerMessage(BIENTOT);
        });
    });

    return carte;
}


/* ------------------------------------------------------------
   Échanges avec le serveur
   ------------------------------------------------------------ */

/* Lancer, Arrêter, Rendre (non) utilisable : le corps (ex. {"id":3}) est
   envoyé à la route, puis la liste est relue.
   Refus (déjà un scénario en cours, erreur dans le texte...) : message du serveur. */
async function commander(bouton, chemin, corps) {
    let resultat;

    bouton.disabled = true;                     /* pas de double clic */
    resultat = await api("POST", chemin, corps);
    bouton.disabled = false;
    if (resultat.code === 401) {
        return;
    }
    if (resultat.code === 200) {
        cacherMessage();
    } else {
        montrerMessage((resultat.donnees && resultat.donnees.erreur)
                       || "La demande a échoué (code " + resultat.code + ").");
    }
    await charger();
}

/* Vérifier : le serveur lit le texte affiché et répond
   soit ce qu'il a compris, soit la première erreur et sa ligne. */
async function verifier() {
    const texte = fenetreScript.querySelector(".fenetre-texte").textContent;
    const resultat = await api("POST", "/api/scenarios/verifier", { texte: texte });
    const r = resultat.donnees;

    consoleVerif.hidden = false;
    consoleVerif.className = "fenetre-console";
    if (resultat.code !== 200 || r === null) {
        consoleVerif.classList.add("console-erreur");
        consoleVerif.textContent = "Vérification impossible (code " + resultat.code + ").";
    } else if (r.ok) {
        consoleVerif.classList.add("console-ok");
        consoleVerif.textContent = "Aucune erreur : le scénario est correct.\n\n"
                                   + "Le serveur a compris :\n" + r.lecture.join("\n");
    } else {
        consoleVerif.classList.add("console-erreur");
        consoleVerif.textContent = "Ligne " + r.ligne + " : " + r.erreur;
    }
}

async function charger() {
    const resultat = await api("GET", "/api/scenarios");
    let scenarios;
    let texte;

    if (resultat.code === 401) {
        return;                                 /* api() renvoie au login */
    }
    if (resultat.code !== 200 || resultat.donnees === null) {
        montrerMessage("Impossible de lire les scénarios (code " + resultat.code + ").");
        return;
    }

    texte = JSON.stringify(resultat.donnees);
    if (texte === dernierTexte) {
        return;                     /* rien n'a changé : on ne touche pas aux cartes */
    }
    dernierTexte = texte;
    scenarios = resultat.donnees.scenarios || [];
    listeCartes.replaceChildren();
    scenarios.forEach(function (s) {
        listeCartes.append(carteScenario(s));
    });
    texteVide.hidden = scenarios.length > 0;
}


/* ------------------------------------------------------------
   Démarrage (après les contrôles de commun.js)
   ------------------------------------------------------------ */

pagePrete.then(function (ok) {
    if (!ok) {
        return;
    }

    document.querySelector(".fenetre-verifier").addEventListener("click", verifier);

    /* Réponse de la fenêtre « Supprimer » : "oui" ou "non" (value du bouton) */
    fenetreSupprimer.addEventListener("close", function () {
        if (fenetreSupprimer.returnValue === "oui" && aSupprimer !== null) {
            montrerMessage(BIENTOT);
        }
        aSupprimer = null;
    });

    charger();

    /* Relecture régulière, sauf si l'onglet est caché */
    setInterval(function () {
        if (!document.hidden) {
            charger();
        }
    }, INTERVALLE_MS);
});
