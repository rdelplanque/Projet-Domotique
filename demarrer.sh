#!/usr/bin/env bash
# ============================================================
#  demarrer.sh - Démarrage du serveur domotique (dans la VM)
#  Usage : ./demarrer.sh <ip_simdom> <port_simdom>
#  Exemple : ./demarrer.sh 192.168.56.1 61404
#  (normalement appelé par lancer.ps1 depuis Windows)
# ============================================================

# --- Dossiers du projet (calculés à partir de l'emplacement du script,
#     pour que le script marche quel que soit le dossier d'où on le lance)
PROJET="$(cd "$(dirname "$0")" && pwd)"
BACK="$PROJET/back"
FRONT="$PROJET/front"
EXECUTABLE="$BACK/build/serveur"
CONFIG="$BACK/config/config.json"

# --- Petites fonctions d'affichage
ok()     { echo "  [OK]    $1"; }
alerte() { echo "  [ALERTE] $1"; }
erreur() { echo "  [ERREUR] $1"; exit 1; }

echo "=== Démarrage du projet domotique ==="

# ------------------------------------------------------------
# 1. Arguments : IP et port de SimDom
# ------------------------------------------------------------
IP="$1"
PORT="$2"

if [ -z "$IP" ] || [ -z "$PORT" ]; then
    erreur "Usage : $0 <ip_simdom> <port_simdom>"
fi

# IP : 4 nombres séparés par des points
if [[ ! "$IP" =~ ^[0-9]{1,3}(\.[0-9]{1,3}){3}$ ]]; then
    erreur "Adresse IP invalide : $IP"
fi

# Port : un nombre entre 1 et 65535
if [[ ! "$PORT" =~ ^[0-9]+$ ]] || [ "$PORT" -lt 1 ] || [ "$PORT" -gt 65535 ]; then
    erreur "Port invalide : $PORT"
fi
ok "SimDom attendu sur $IP:$PORT"

# ------------------------------------------------------------
# 2. PostgreSQL (indispensable : sans base, on s'arrête)
# ------------------------------------------------------------
if systemctl is-active --quiet postgresql; then
    ok "PostgreSQL est démarré"
else
    erreur "PostgreSQL n'est pas démarré (sudo systemctl start postgresql)"
fi

# ------------------------------------------------------------
# 3. nginx (pas encore configuré : simple alerte s'il manque)
# ------------------------------------------------------------
if systemctl is-active --quiet nginx; then
    ok "nginx est démarré (site servi depuis $FRONT)"
else
    alerte "nginx n'est pas démarré : l'interface web ne sera pas accessible"
fi

# ------------------------------------------------------------
# 4. SimDom joignable ?
#    Pas bloquant : le serveur C sait réessayer la connexion.
# ------------------------------------------------------------
if nc -z -w 3 "$IP" "$PORT" 2>/dev/null; then
    ok "SimDom répond sur $IP:$PORT"
else
    alerte "SimDom ne répond pas sur $IP:$PORT (le serveur réessaiera)"
fi

# ------------------------------------------------------------
# 4bis. Fichier de configuration (hors Git : à créer à la main)
# ------------------------------------------------------------
if [ -f "$CONFIG" ]; then
    ok "Configuration trouvée : $CONFIG"
else
    erreur "Fichier absent : $CONFIG (cp $BACK/config/config.exemple.json $CONFIG puis le remplir)"
fi

# ------------------------------------------------------------
# 5. Compilation du serveur C
#    make ne recompile que les fichiers modifiés : c'est rapide.
# ------------------------------------------------------------
if [ -f "$BACK/Makefile" ]; then
    echo "  Compilation (make)..."
    if make -C "$BACK" --no-print-directory; then
        ok "Compilation réussie"
    else
        erreur "La compilation a échoué (voir les messages ci-dessus)"
    fi
else
    alerte "Pas de Makefile dans $BACK : compilation ignorée"
fi

# ------------------------------------------------------------
# 6. Lancement du serveur
# ------------------------------------------------------------
if [ -x "$EXECUTABLE" ]; then
    echo "=== Lancement de $EXECUTABLE $IP $PORT $CONFIG ==="
    exec "$EXECUTABLE" "$IP" "$PORT" "$CONFIG"
else
    alerte "Exécutable introuvable : $EXECUTABLE (serveur non lancé)"
fi