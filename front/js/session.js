/* ============================================================
   session.js – Parler au serveur avec le jeton (le bracelet)
   Toutes les pages internes l'utilisent pour appeler /api/...
   Chargé AVANT commun.js et le script propre à la page.
   ============================================================ */

/* Retour à la page de login : on oublie tout (jeton, profil...). */
function retourLogin() {
    sessionStorage.clear();
    location.replace("/");
}

/* Appelle l'API et renvoie { code, donnees }.
   - methode : "GET", "POST"...
   - chemin  : "/api/moi"...
   - corps   : objet à envoyer en JSON (facultatif)
   code 0 = serveur injoignable ; 401 = jeton refusé → retour au login. */
async function api(methode, chemin, corps) {
    const options = { method: methode, headers: {} };
    const jeton = sessionStorage.getItem("jeton");
    let reponse;
    let donnees = null;

    /* Le bracelet, montré à chaque requête */
    if (jeton) {
        options.headers["Authorization"] = "Bearer " + jeton;
    }
    if (corps !== undefined) {
        options.headers["Content-Type"] = "application/json";
        options.body = JSON.stringify(corps);
    }

    try {
        reponse = await fetch(chemin, options);
    } catch (erreur) {
        return { code: 0, donnees: null };      /* réseau coupé, VM éteinte... */
    }

    /* Bracelet refusé (serveur redémarré, session expirée...) */
    if (reponse.status === 401) {
        retourLogin();
        return { code: 401, donnees: null };
    }

    /* Une erreur 502 de nginx est en HTML, pas en JSON : d'où le try. */
    try {
        donnees = await reponse.json();
    } catch (erreur) {
        donnees = null;
    }
    return { code: reponse.status, donnees: donnees };
}