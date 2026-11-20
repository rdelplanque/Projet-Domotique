/* ============================================================
   scenario.js – Éditeur de scénario (scenario.html)
   Les listes déroulantes fabriquent une ligne LDSD, ajoutée à la FIN
   du texte ; le texte reste modifiable à la main.
     1. Quand ?           → remplace la ligne qui suit « En-tête : »
     2. Ajouter une action → étage → pièce → type → équipement → action (→ mode)
        Chaque liste ne propose que ce qui existe dans le choix précédent :
        on ne peut pas fabriquer une ligne refusée par le langage.
     3. Vérifier / Enregistrer (le serveur revérifie : un texte faux
        n'est pas enregistré).
   scenario.html        : nouveau scénario (POST /api/scenarios/creer)
   scenario.html?id=12  : modification   (POST /api/scenarios/modifier)
   Besoin de session.js (api) et commun.js (pagePrete).
   ============================================================ */

/* Les 4 types : nom exact dans la base, libellé de la liste,
   groupe LDSD (« tous les … ») et actions possibles. */
const TYPES = [
    { nom: "Luminaire", libelle: "Luminaire",
      groupe: "tous les luminaires", actions: ["allumer", "éteindre"] },
    { nom: "Prise commandée", libelle: "Prise commandée",
      groupe: "toutes les prises", actions: ["allumer", "éteindre"] },
    { nom: "Climatisation réversible", libelle: "Climatisation",
      groupe: "toutes les climatisations", actions: ["allumer", "éteindre"] },
    { nom: "Volet roulant / porte basculante", libelle: "Volet / porte",
      groupe: "tous les volets", actions: ["ouvrir", "fermer"] }
];
const CLIM = "Climatisation réversible";

const TOUS = "*";               /* valeur des choix « Tous les … » */
const TEXTE_NEUF = "En-tête :\nau clic\nListe :\n";
const LISTE_SCENARIOS = "/pages/scenarios.html";

/* Le scénario modifié (null = nouveau scénario) */
const idScenario = Number(new URLSearchParams(location.search).get("id")) || null;

let maison = null;              /* réponse de GET /api/maison */

const message = document.querySelector(".maison-message");
const champNom = document.getElementById("nom");
const champTexte = document.getElementById("texte");
const consoleVerif = document.querySelector(".fenetre-console");

const choixEtage = document.getElementById("choix-etage");
const choixPiece = document.getElementById("choix-piece");
const choixType = document.getElementById("choix-type");
const choixEquipement = document.getElementById("choix-equipement");
const choixAction = document.getElementById("choix-action");
const choixMode = document.getElementById("choix-mode");
const apercu = document.querySelector(".apercu-ligne");
const apercuNombre = document.querySelector(".apercu-nombre");


/* ------------------------------------------------------------
   Petits outils
   ------------------------------------------------------------ */

function montrerMessage(texte) {
    message.textContent = texte;
    message.hidden = false;
    message.scrollIntoView({ block: "nearest" });
}

function cacherMessage() {
    message.hidden = true;
}

function trierParNom(liste) {
    return liste.slice().sort(function (a, b) {
        return a.nom.localeCompare(b.nom, "fr");
    });
}

function trouverType(nom) {
    return TYPES.find(function (t) { return t.nom === nom; });
}

/* Remplit une liste avec des options [{ valeur, texte }].
   Le choix précédent est gardé s'il existe encore, sinon c'est le premier. */
function remplirListe(liste, options) {
    const ancien = liste.value;

    liste.replaceChildren();
    options.forEach(function (o) {
        const option = document.createElement("option");
        option.value = o.valeur;
        option.textContent = o.texte;          /* textContent : pas de HTML injecté */
        liste.append(option);
    });
    if (options.some(function (o) { return o.valeur === ancien; })) {
        liste.value = ancien;
    }
}

/* Console verte (correct) ou rouge (erreur) */
function afficherConsole(texte, correct) {
    consoleVerif.hidden = false;
    consoleVerif.className = "fenetre-console " + (correct ? "console-ok" : "console-erreur");
    consoleVerif.textContent = texte;
}


/* ------------------------------------------------------------
   2. Les listes déroulantes de l'action
   ------------------------------------------------------------ */

/* Les équipements qui correspondent à l'étage et à la pièce choisis */
function equipementsChoisis() {
    const etage = choixEtage.value;
    const piece = choixPiece.value;

    return maison.equipements.filter(function (e) {
        const p = maison.pieces.find(function (x) { return x.id === e.piece; });

        return p !== undefined
            && (etage === TOUS || String(p.etage) === etage)
            && (piece === TOUS || String(p.id) === piece);
    });
}

/* Appelée à chaque changement d'une liste : remet à jour les listes
   suivantes, puis l'aperçu de la ligne. */
function majListes() {
    const etage = choixEtage.value;
    let pieces;
    let equipements;
    let type;

    /* Pièce : seulement celles de l'étage qui ont au moins un équipement.
       « Tous les étages » → forcément « Toutes les pièces ». */
    pieces = (etage === TOUS) ? [] : maison.pieces.filter(function (p) {
        return String(p.etage) === etage
            && maison.equipements.some(function (e) { return e.piece === p.id; });
    });
    remplirListe(choixPiece, [{ valeur: TOUS, texte: "Toutes les pièces" }].concat(
        trierParNom(pieces).map(function (p) { return { valeur: String(p.id), texte: p.nom }; })));
    choixPiece.disabled = (etage === TOUS);

    /* Type : seulement les types présents à cet endroit (dans l'ordre de TYPES) */
    equipements = equipementsChoisis();
    remplirListe(choixType, TYPES.filter(function (t) {
        return equipements.some(function (e) { return e.type === t.nom; });
    }).map(function (t) { return { valeur: t.nom, texte: t.libelle }; }));
    type = trouverType(choixType.value);
    if (type === undefined) {
        apercu.textContent = "";            /* aucun équipement à cet endroit */
        return;
    }

    /* Équipement : « Tous les … », ou un seul si une pièce est choisie */
    equipements = equipements.filter(function (e) { return e.type === choixType.value; });
    remplirListe(choixEquipement, [{ valeur: TOUS, texte: majuscule(type.groupe) }].concat(
        (choixPiece.value === TOUS) ? [] : trierParNom(equipements).map(function (e) {
            return { valeur: String(e.id), texte: e.nom };
        })));
    choixEquipement.disabled = (choixPiece.value === TOUS);

    /* Action selon le type ; mode seulement pour « allumer » une climatisation */
    remplirListe(choixAction, type.actions.map(function (a) { return { valeur: a, texte: a }; }));
    document.querySelector(".choix-mode").hidden = !(type.nom === CLIM && choixAction.value === "allumer");

    majApercu(type, equipements);
}

function majuscule(texte) {
    return texte.charAt(0).toUpperCase() + texte.slice(1);
}

/* Fabrique la ligne LDSD et compte les équipements touchés */
function majApercu(type, equipements) {
    const etage = maison.etages.find(function (x) { return String(x.id) === choixEtage.value; });
    const piece = maison.pieces.find(function (x) { return String(x.id) === choixPiece.value; });
    const equipement = maison.equipements.find(function (x) { return String(x.id) === choixEquipement.value; });
    let cible;
    let nombre;

    if (equipement !== undefined) {
        cible = '"' + equipement.nom + '" de "' + piece.nom + '" de "' + etage.nom + '"';
        nombre = 1;
    } else if (piece !== undefined) {
        cible = type.groupe + ' de "' + piece.nom + '" de "' + etage.nom + '"';
        nombre = equipements.length;
    } else if (etage !== undefined) {
        cible = type.groupe + ' de toutes les pièces de "' + etage.nom + '"';
        nombre = equipements.length;
    } else {
        cible = type.groupe + " de toutes les pièces de tous les étages";
        nombre = equipements.length;
    }

    apercu.textContent = choixAction.value + " " + cible
        + (document.querySelector(".choix-mode").hidden ? "" : choixMode.value);
    apercuNombre.textContent = "(" + nombre + " équipement" + (nombre > 1 ? "s" : "") + ")";
}

/* + Ajouter : la ligne va à la fin du texte (après « Liste : ») */
function ajouterLigne() {
    let texte = champTexte.value;

    if (texte !== "" && !texte.endsWith("\n")) {
        texte += "\n";
    }
    if (!texte.split("\n").some(function (l) { return l.trim() === "Liste :"; })) {
        texte += "Liste :\n";
    }
    champTexte.value = texte + apercu.textContent + "\n";
    champTexte.scrollTop = champTexte.scrollHeight;     /* montrer la ligne ajoutée */
}


/* ------------------------------------------------------------
   1. Quand ? (la ligne d'en-tête)
   ------------------------------------------------------------ */

function choixQuand() {
    return document.querySelector('input[name="quand"]:checked').value;
}

/* Montre la date, les jours, l'heure selon le choix */
function majQuand() {
    const quand = choixQuand();

    document.querySelector(".quand-date").hidden = (quand !== "une");
    document.querySelector(".quand-jours").hidden = (quand !== "chaque");
    document.querySelector(".quand-heure").hidden = (quand === "clic");
    document.querySelector(".erreur-quand").hidden = true;
}

/* Fabrique la ligne d'en-tête, ou renvoie null (message sous les champs) */
function ligneEnTete() {
    const quand = choixQuand();
    const heure = document.getElementById("heure").value;      /* "19:00" */
    const date = document.getElementById("date").value;        /* "2026-12-24" */
    const erreur = document.querySelector(".erreur-quand");
    let jours;
    let morceaux;

    erreur.hidden = true;                   /* message d'un essai précédent effacé */
    if (quand === "clic") {
        return "au clic";
    }
    if (quand === "une") {
        if (date === "" || heure === "") {
            erreur.textContent = "Choisissez une date et une heure.";
            erreur.hidden = false;
            return null;
        }
        morceaux = date.split("-");                             /* [aaaa, mm, jj] */
        return "le " + morceaux[2] + "/" + morceaux[1] + "/" + morceaux[0] + " à " + heure;
    }
    jours = Array.from(document.querySelectorAll(".quand-jours input:checked"))
                 .map(function (c) { return c.value; });
    if (jours.length === 0 || heure === "") {
        erreur.textContent = "Cochez au moins un jour et choisissez une heure.";
        erreur.hidden = false;
        return null;
    }
    return "chaque " + jours.join(" et ") + " à " + heure;
}

/* Écrire l'en-tête : remplace la ligne qui suit « En-tête : »
   (lignes vides et commentaires sautés), ou l'ajoute. */
function ecrireEnTete() {
    const ligne = ligneEnTete();
    const lignes = champTexte.value.split("\n");
    let i;
    let j;

    if (ligne === null) {
        return;
    }
    i = lignes.findIndex(function (l) { return l.trim() === "En-tête :"; });
    if (i < 0) {
        lignes.unshift("En-tête :", ligne);         /* pas de marque : en tête du texte */
    } else {
        j = i + 1;
        while (j < lignes.length && (lignes[j].trim() === "" || lignes[j].trim().startsWith("#"))) {
            j++;
        }
        if (j < lignes.length && lignes[j].trim() !== "Liste :") {
            lignes[j] = ligne;
        } else {
            lignes.splice(i + 1, 0, ligne);         /* rien entre les deux marques */
        }
    }
    champTexte.value = lignes.join("\n");
}

/* Modification : coche « Quand ? » d'après l'en-tête du texte lu */
function lireEnTete(texte) {
    const lignes = texte.split("\n").map(function (l) { return l.trim(); });
    const i = lignes.indexOf("En-tête :");
    let ligne;
    let r;

    if (i < 0) {
        return;
    }
    ligne = lignes.slice(i + 1).find(function (l) { return l !== "" && !l.startsWith("#"); }) || "";

    if ((r = ligne.match(/^le (\d\d)\/(\d\d)\/(\d{4}) à (\d\d:\d\d)$/)) !== null) {
        document.querySelector('input[value="une"]').checked = true;
        document.getElementById("date").value = r[3] + "-" + r[2] + "-" + r[1];
        document.getElementById("heure").value = r[4];
    } else if ((r = ligne.match(/^chaque (.+) à (\d\d:\d\d)$/)) !== null) {
        document.querySelector('input[value="chaque"]').checked = true;
        r[1].split(" et ").forEach(function (jour) {
            const case_ = document.querySelector('.quand-jours input[value="' + jour + '"]');
            if (case_ !== null) {
                case_.checked = true;
            }
        });
        document.getElementById("heure").value = r[2];
    }
    majQuand();
}


/* ------------------------------------------------------------
   3. Vérifier et enregistrer
   ------------------------------------------------------------ */

async function verifier() {
    const resultat = await api("POST", "/api/scenarios/verifier", { texte: champTexte.value });
    const r = resultat.donnees;

    if (resultat.code === 401) {
        return;
    }
    if (resultat.code !== 200 || r === null) {
        afficherConsole("Vérification impossible (code " + resultat.code + ").", false);
    } else if (r.ok) {
        afficherConsole("Aucune erreur : le scénario est correct.\n\n"
                        + "Le serveur a compris :\n" + r.lecture.join("\n"), true);
    } else {
        afficherConsole("Ligne " + r.ligne + " : " + r.erreur, false);
    }
}

async function enregistrer() {
    const bouton = document.querySelector(".bouton-enregistrer");
    const nom = champNom.value.trim();
    const corps = { nom: nom, texte: champTexte.value };
    let resultat;

    if (nom === "") {
        montrerMessage("Donnez un nom au scénario.");
        champNom.focus();
        return;
    }
    if (idScenario !== null) {
        corps.id = idScenario;
    }

    bouton.disabled = true;                     /* pas de double clic */
    resultat = await api("POST", idScenario !== null ? "/api/scenarios/modifier"
                                                     : "/api/scenarios/creer", corps);
    bouton.disabled = false;

    if (resultat.code === 401) {
        return;
    }
    if (resultat.code === 200) {
        location.href = LISTE_SCENARIOS;        /* retour à la liste */
        return;
    }
    if (resultat.donnees && resultat.donnees.ligne) {
        /* Texte faux : l'erreur va dans la console, comme pour Vérifier */
        cacherMessage();
        afficherConsole("Enregistrement refusé, " + resultat.donnees.erreur, false);
        return;
    }
    montrerMessage((resultat.donnees && resultat.donnees.erreur)
                   || "L'enregistrement a échoué (code " + resultat.code + ").");
}


/* ------------------------------------------------------------
   Démarrage (après les contrôles de commun.js)
   ------------------------------------------------------------ */

/* Modification : relit le scénario dans la liste et remplit la page.
   Renvoie false s'il est introuvable. */
async function chargerScenario() {
    const resultat = await api("GET", "/api/scenarios");
    let s;

    if (resultat.code !== 200 || resultat.donnees === null) {
        montrerMessage("Impossible de lire le scénario (code " + resultat.code + ").");
        return false;
    }
    s = (resultat.donnees.scenarios || []).find(function (x) { return x.id === idScenario; });
    if (s === undefined) {
        montrerMessage("Ce scénario n'existe pas (ou plus).");
        return false;
    }
    champNom.value = s.nom;
    champTexte.value = s.script;
    lireEnTete(s.script);
    if (s.en_cours) {
        montrerMessage("Ce scénario est en cours : arrêtez-le avant d'enregistrer vos modifications.");
    }
    return true;
}

pagePrete.then(async function (ok) {
    let resultat;

    if (!ok) {
        return;
    }

    /* Titre : « Modifier le scénario » au lieu de « Nouveau scénario » */
    if (idScenario !== null) {
        document.querySelector(".entete-titre").textContent = "Modifier le scénario";
        document.title = "R-Domotik – Modifier le scénario";
    }

    /* La maison, pour les listes déroulantes */
    resultat = await api("GET", "/api/maison");
    if (resultat.code === 401) {
        return;
    }
    if (resultat.code !== 200 || resultat.donnees === null) {
        montrerMessage("Impossible de lire la maison (code " + resultat.code + ").");
        return;
    }
    maison = resultat.donnees;

    remplirListe(choixEtage, [{ valeur: TOUS, texte: "Tous les étages" }].concat(
        maison.etages.map(function (e) { return { valeur: String(e.id), texte: e.nom }; })));
    [choixEtage, choixPiece, choixType, choixEquipement, choixAction, choixMode]
        .forEach(function (liste) { liste.addEventListener("change", majListes); });
    majListes();

    document.querySelectorAll('input[name="quand"]').forEach(function (r) {
        r.addEventListener("change", majQuand);
    });
    document.querySelector(".bouton-entete").addEventListener("click", ecrireEnTete);
    document.querySelector(".bouton-ajouter").addEventListener("click", ajouterLigne);
    document.querySelector(".bouton-verifier").addEventListener("click", verifier);
    document.querySelector(".bouton-enregistrer").addEventListener("click", enregistrer);

    if (idScenario === null) {
        champTexte.value = TEXTE_NEUF;
        majQuand();
    } else if (!(await chargerScenario())) {
        document.querySelector(".bouton-enregistrer").disabled = true;
    }
});
