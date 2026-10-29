/* ============================================================
   maison.js – Page « Ma maison » : consulter et piloter
   Trois niveaux sur la même page :
     étages → pièces d'un étage → équipements d'une pièce (par type)
   Le niveau affiché est noté dans l'adresse, après le # :
     maison.html                      les étages
     maison.html#etage=2              les pièces de l'étage 2
     maison.html#etage=2&piece=23     les équipements de la pièce 23
   Le bouton « précédent » du navigateur remonte donc d'un niveau.
   Besoin de session.js (api) et commun.js (element, pagePrete).
   ============================================================ */

const INTERVALLE_MS = 1000;     /* relecture automatique toutes les secondes */

/* Les 4 types d'équipements, dans l'ordre d'affichage.
   nom = valeur exacte de nom_type_equipement dans la base. */
const TYPES = [
    { nom: "Luminaire", titre: "Luminaires", icone: "💡",
      allumer: "Allumer", eteindre: "Éteindre", allume: "Allumé", eteint: "Éteint" },
    { nom: "Prise commandée", titre: "Prises", icone: "🔌",
      allumer: "Allumer", eteindre: "Éteindre", allume: "Allumée", eteint: "Éteinte" },
    { nom: "Climatisation réversible", titre: "Climatisations", icone: "❄️",
      allumer: "Allumer", eteindre: "Éteindre", allume: "Allumée", eteint: "Éteinte" },
    { nom: "Volet roulant / porte basculante", titre: "Volets et portes", icone: "🪟",
      allumer: "Monter", eteindre: "Baisser", allume: "Ouvert", eteint: "Fermé" }
];
const CLIM = "Climatisation réversible";

/* Ce que la page sait */
let maison = null;          /* dernière réponse de GET /api/maison */
let dernierTexte = "";      /* la même en texte : pour savoir si quelque chose a changé */
let idEtage = null;         /* étage choisi (null = liste des étages)  */
let idPiece = null;         /* pièce choisie (null = liste des pièces) */

const vue = document.querySelector(".maison-vue");
const ariane = document.querySelector(".fil-ariane");
const message = document.querySelector(".maison-message");


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

/* Trie une liste d'objets par leur nom (ordre français : é avec e) */
function trierParNom(liste) {
    return liste.slice().sort(function (a, b) {
        return a.nom.localeCompare(b.nom, "fr");
    });
}

function trouverEtage(id) {
    return maison.etages.find(function (e) { return e.id === id; });
}

function trouverPiece(id) {
    return maison.pieces.find(function (p) { return p.id === id; });
}

/* Une carte cliquable vers un autre niveau (même style que l'accueil) */
function carteLien(texte, adresse) {
    const carte = element("a", "carte", null);
    carte.href = adresse;
    carte.append(element("span", "carte-nom", texte));
    return carte;
}


/* ------------------------------------------------------------
   Où en est-on ? (lu dans l'adresse, après le #)
   ------------------------------------------------------------ */

function lireAdresse() {
    const parametres = new URLSearchParams(location.hash.slice(1));
    idEtage = parametres.has("etage") ? Number(parametres.get("etage")) : null;
    idPiece = parametres.has("piece") ? Number(parametres.get("piece")) : null;
}


/* ------------------------------------------------------------
   Affichage
   ------------------------------------------------------------ */

/* Fil d'Ariane : Maison › Rez-de-chaussée › Salon
   Chaque niveau est un lien, sauf le dernier (là où l'on est). */
function afficherAriane(etage, piece) {
    const niveaux = [{ texte: "Maison", adresse: "#" }];

    if (etage) {
        niveaux.push({ texte: etage.nom, adresse: "#etage=" + etage.id });
    }
    if (piece) {
        niveaux.push({ texte: piece.nom, adresse: null });
    }

    ariane.replaceChildren();
    niveaux.forEach(function (niveau, i) {
        const dernier = (i === niveaux.length - 1);
        let morceau;

        if (i > 0) {
            ariane.append(element("span", "fil-separateur", "›"));
        }
        if (dernier) {
            morceau = element("span", "fil-actuel", niveau.texte);
        } else {
            morceau = element("a", "fil-lien", niveau.texte);
            morceau.href = niveau.adresse;
        }
        ariane.append(morceau);
    });
}

/* Niveau 1 : les étages */
function afficherEtages() {
    const cartes = element("div", "cartes", null);

    maison.etages.forEach(function (etage) {
        cartes.append(carteLien(etage.nom, "#etage=" + etage.id));
    });
    vue.append(cartes);
}

/* Niveau 2 : les pièces d'un étage */
function afficherPieces(etage) {
    const cartes = element("div", "cartes", null);
    const pieces = maison.pieces.filter(function (p) { return p.etage === etage.id; });

    trierParNom(pieces).forEach(function (piece) {
        cartes.append(carteLien(piece.nom, "#etage=" + etage.id + "&piece=" + piece.id));
    });
    vue.append(cartes);
}

/* Texte et classe CSS (couleur) d'après l'état */
function decrireEtat(equipement, type) {
    let texte;
    let classe;

    if (equipement.etat === null) {
        texte = "Indéterminé";
        classe = "etat-indetermine";
    } else if (equipement.etat === 2) {
        texte = "En panne";
        classe = "etat-panne";
    } else if (equipement.etat === 1) {
        texte = type.allume;
        classe = "etat-allume";
    } else {
        texte = type.eteint;
        classe = "etat-eteint";
    }
    if (type.nom === CLIM && equipement.mode !== null) {
        texte += equipement.mode === 1 ? " · chauffage" : " · climatisation";
    }
    return { texte: texte, classe: classe };
}

/* Un bouton de commande */
function boutonCommande(texte, equipement, etat, mode) {
    const bouton = element("button", "bouton-commande", texte);
    bouton.type = "button";
    bouton.addEventListener("click", function () {
        commander(bouton, equipement, etat, mode);
    });
    return bouton;
}

/* Une carte équipement : icône, nom, état, boutons */
function carteEquipement(equipement, type, piloter) {
    const etat = decrireEtat(equipement, type);
    const carte = element("div", "carte-equipement " + etat.classe, null);
    const boutons = element("div", "equipement-boutons", null);
    let icone = type.icone;

    if (type.nom === CLIM && equipement.mode === 1) {
        icone = "🔥";                           /* chauffage */
    }
    carte.append(
        element("span", "equipement-icone", icone),
        element("span", "equipement-nom", equipement.nom),
        element("span", "equipement-etat", etat.texte)
    );

    /* Boutons : seulement si on peut piloter cette pièce
       et si l'état est connu et pas en panne */
    if (piloter && (equipement.etat === 0 || equipement.etat === 1)) {
        boutons.append(boutonCommande(
            equipement.etat === 1 ? type.eteindre : type.allumer,
            equipement, 1 - equipement.etat, equipement.mode));

        if (type.nom === CLIM) {
            const mode = equipement.mode === 1 ? 1 : 0;
            boutons.append(boutonCommande(
                mode === 1 ? "Passer en climatisation" : "Passer en chauffage",
                equipement, equipement.etat, 1 - mode));
        }
        carte.append(boutons);
    }
    return carte;
}

/* Niveau 3 : les équipements d'une pièce, regroupés par type */
function afficherEquipements(piece) {
    const equipements = maison.equipements.filter(function (e) { return e.piece === piece.id; });

    TYPES.forEach(function (type) {
        const duType = equipements.filter(function (e) { return e.type === type.nom; });
        const groupe = element("section", "groupe", null);
        const cartes = element("div", "cartes", null);

        if (duType.length === 0) {
            return;                             /* pas de groupe vide */
        }
        trierParNom(duType).forEach(function (e) {
            cartes.append(carteEquipement(e, type, piece.piloter));
        });
        groupe.append(element("h2", "groupe-titre", type.titre), cartes);
        vue.append(groupe);
    });
}

/* Redessine le niveau courant */
function afficher() {
    let etage = idEtage !== null ? trouverEtage(idEtage) : null;
    let piece = idPiece !== null ? trouverPiece(idPiece) : null;

    /* Adresse incohérente (pièce d'un autre étage, id inconnu...) : on remonte */
    if (piece && (!etage || piece.etage !== etage.id)) {
        piece = null;
    }

    vue.replaceChildren();
    afficherAriane(etage, piece);
    if (piece) {
        afficherEquipements(piece);
    } else if (etage) {
        afficherPieces(etage);
    } else {
        afficherEtages();
    }
}


/* ------------------------------------------------------------
   Échanges avec le serveur
   ------------------------------------------------------------ */

/* Lit toute la maison. On ne redessine que si quelque chose a changé
   (sinon, à chaque relecture, un bouton pourrait disparaître sous le doigt).
   Si une pièce est affichée, on la signale au serveur (?piece=23) : il la
   relit en priorité dans SimDom. relire = true (bouton Rafraîchir) : le
   serveur relit cette pièce tout de suite avant de répondre. */
async function charger(relire) {
    let chemin = "/api/maison";
    let resultat;
    let texte;

    if (idPiece !== null) {
        chemin += "?piece=" + idPiece;
        if (relire === true) {
            chemin += "&relire=1";
        }
    }
    resultat = await api("GET", chemin);

    if (resultat.code === 401) {
        return;                                 /* api() renvoie au login */
    }
    if (resultat.code !== 200 || resultat.donnees === null) {
        montrerMessage("Impossible de lire l'état de la maison (code " + resultat.code + ").");
        return;
    }

    texte = JSON.stringify(resultat.donnees);
    if (texte !== dernierTexte) {
        dernierTexte = texte;
        maison = resultat.donnees;
        afficher();
    }
}

/* Envoie une commande au serveur, puis relit la maison.
   etat : 0 ou 1 voulu ; mode : 0 ou 1 (climatisations seulement). */
async function commander(bouton, equipement, etat, mode) {
    const corps = { id: equipement.id, etat: etat };
    let resultat;

    if (equipement.type === CLIM) {
        corps.mode = mode === 1 ? 1 : 0;        /* SimDom veut toujours le mode */
    }

    bouton.disabled = true;                     /* pas de double clic */
    resultat = await api("POST", "/api/commande", corps);
    bouton.disabled = false;

    if (resultat.code === 401) {
        return;
    }
    if (resultat.code === 200) {
        cacherMessage();
    } else {
        montrerMessage((resultat.donnees && resultat.donnees.erreur)
                       || "La commande a échoué (code " + resultat.code + ").");
    }
    await charger();
}


/* ------------------------------------------------------------
   Démarrage (après les contrôles de commun.js)
   ------------------------------------------------------------ */

pagePrete.then(async function (ok) {
    if (!ok) {
        return;
    }
    lireAdresse();
    await charger();

    /* Changement de niveau (clic sur une carte, fil d'Ariane, bouton précédent).
       On redemande aussitôt au serveur : la nouvelle pièce devient prioritaire. */
    window.addEventListener("hashchange", function () {
        lireAdresse();
        cacherMessage();
        if (maison) {
            afficher();
        }
        charger();
    });

    /* Rafraîchir : relecture immédiate de la pièce affichée dans SimDom */
    document.querySelector(".fil-rafraichir").addEventListener("click", function () {
        charger(true);
    });

    /* Relecture automatique, sauf si l'onglet est caché */
    setInterval(function () {
        if (!document.hidden) {
            charger();
        }
    }, INTERVALLE_MS);
});
