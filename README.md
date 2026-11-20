# Projet Domotique – ESGI/B2 2026/2027

Application web (serveur C + client HTML/CSS/JS + PostgreSQL) pour piloter la maison simulée par SimDom.

## Compiler
```bash
make -C back            # compile → back/build/serveur
make -C back clean      # supprime les fichiers compilés
```

## Lancer
```bash
./demarrer.sh <ip_simdom> <port_simdom>     # ex. ./demarrer.sh 192.168.56.1 53217
```
Depuis Windows : `lancer.ps1` sous powershell (démarre SimDOM, démarre la VM, détecte le port de SimDom, acces par SSH, lance `demarrer.sh` sur la VM).

## Base de données
### 1. Automatiser les données de la BDD
```bash
cd lecture_equipement
gcc -Wall -Wextra -std=c11 -o lecture_equipement *.c
./lecture_equipement equipement.txt
```
### 2. La BDD
```bash
# se connecter
psql -U domotique -d domotique              
# exécuter le fichier de création des tables
psql -h localhost -U domotique -d domotique -f sql/01_creation_tables.sql   
# peuplement des tables:
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/01_batiment.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/02_localisation.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/03_piece.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/04_type_equipement.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/05_ip_equipement.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/06_equipement.sql

# spécifique aux utilisateurs:
psql -h localhost -U domotique -d domotique -f sql/peuplement_des_utilisateurs/01_type_utilisateur.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_des_utilisateurs/02_utilisateurs.sql  #n'est pas push vers git // un exemple est push vers github mais pas vers la BDD
psql -h localhost -U domotique -d domotique -f sql/peuplement_des_utilisateurs/03_acces_piece.sql

# scénarios : 2 exemples + 7 scénarios de test (rejouables)
psql -h localhost -U domotique -d domotique -f sql/peuplement_des_scenarios/01_scenarios_exemple.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_des_scenarios/02_scenarios_test.sql

# base créée AVANT la colonne scenario.actif : l'ajouter sans rien effacer
psql -h localhost -U domotique -d domotique -f sql/02_ajout_scenario_actif.sql

# exemple de vérification:
# toutes les tables
psql -h localhost -U domotique -d domotique -c '\dt'        

psql -h localhost -U domotique -d domotique -c 'SELECT * FROM localisation'
psql -h localhost -U domotique -d domotique -c 'SELECT count(*) FROM piece'      
psql -h localhost -U domotique -d domotique -c 'SELECT count(*) FROM equipement' 
```

## API (serveur C, derrière nginx)
Toutes les routes protégées demandent l'en-tête `Authorization: Bearer <jeton>` (sinon 401),
puis le droit indiqué (sinon 403). Droits par profil : `back/auth/droits.c`.

| Route | Droit | Rôle |
|---|---|---|
| `GET /api/ping` | publique | le serveur répond |
| `POST /api/login` | publique | connexion : jeton, profil, prénom, nom |
| `POST /api/logout` | connecté | ferme la session |
| `GET /api/moi` | connecté | profil et liste des droits |
| `GET /api/maison` | consulter | étages, pièces, équipements (`?piece=23`, `&relire=1`) |
| `POST /api/commande` | piloter | `{"id":57,"etat":1}` (+ `"mode"` pour une clim) |
| `GET /api/scenarios` | scenarios | liste : type, utilisable, actif, en cours, texte |
| `POST /api/scenarios/verifier` | scenarios | `{"texte":"..."}` → ce que le serveur a compris, ou la 1ʳᵉ erreur et sa ligne |
| `POST /api/scenarios/creer` | scenarios | `{"nom":"...","texte":"..."}` → `{"id":12}` ; refusé (409) si le texte a une erreur ou si le nom est pris |
| `POST /api/scenarios/modifier` | scenarios | `{"id":12,"nom":"...","texte":"..."}` ; repasse non utilisable ; 409 si le scénario est en cours |
| `POST /api/scenarios/lancer` | scenarios | `{"id":3}` lance un scénario direct (un seul à la fois) |
| `POST /api/scenarios/arreter` | scenarios | `{"id":3}` arrête le scénario en cours |
| `POST /api/scenarios/utilisable` | scenarios_admin | `{"id":3,"utilisable":true}` (refusé si le texte a une erreur) |

Tester à la main depuis la VM :
```bash
JETON=$(curl -s localhost:8080/api/login -d '{"email":"admin@domotique.local","mot_de_passe":"..."}' | sed 's/.*"jeton":"\([^"]*\)".*/\1/')
curl -s localhost:8080/api/scenarios -H "Authorization: Bearer $JETON"
```

## Scénarios (langage LDSD)
Un scénario = un **en-tête** (quand ?) et une **liste** d'actions (quoi ?). Une instruction par ligne,
`#` = commentaire. Les noms s'écrivent **entre guillemets**, exactement comme dans la base ;
les mots sans guillemets sont des mots-clés.
```
En-tête :
au clic
Liste :
fermer tous les volets de "Salon" de "Rez-de-chaussée"
allumer "Luminaire salon nord" de "Salon" de "Rez-de-chaussée"
allumer "Climatisation réversible" de "Salon" de "Rez-de-chaussée" en chauffage
```

| En-tête | Exemple | Type |
|---|---|---|
| `au clic` | `au clic` | direct (bouton Lancer) |
| `le <jj/mm/aaaa> à <hh:mm>` | `le 24/12/2026 à 19:00` | programmé (une fois) |
| `chaque <jour> [et <jour>…] à <hh:mm>` | `chaque samedi et dimanche à 08:00` | programmé |

| Action | Pour |
|---|---|
| `allumer … [en chauffage / en climatisation]` | luminaires, prises, clims (le mode : clims seulement) |
| `éteindre …` | luminaires, prises, clims |
| `ouvrir …` / `fermer …` | volets et portes |

Cible : `"X" de "P" de "E"` (un équipement), ou un groupe : `tous les luminaires`, `toutes les prises`,
`toutes les climatisations`, `tous les volets`, suivi de `de "P" de "E"`, `de toutes les pièces de "E"`
ou `de toutes les pièces de tous les étages`. Attention : « tous les volets » comprend les portes de garage.

Code : `back/scenario/ldsd.c` (lecture et vérification), `execution.c` (exécution), `scenario.c` (routes).
- **Lecture** : à la première erreur, numéro de ligne + message (bouton « Vérifier » de la page).
- **Exécution** : pas de thread, le scénario avance par tranches de ~0,25 s dans la boucle HTTP
  (les pages restent servies). Pour chaque équipement : relecture de l'état réel, panne ou absence
  de réponse = sauté, déjà dans l'état voulu = rien envoyé, sinon commande puis relecture.
  Toute la maison (123 luminaires) ≈ 20 s.
- **Journal** du serveur : lignes `[scénario]` (lancement, chaque action, équipements sautés, bilan).
- **Éditeur** (`front/pages/scenario.html`, `?id=12` pour modifier) : listes déroulantes
  étage → pièce → type → équipement → action (→ mode) qui écrivent la ligne à la fin du texte ;
  « Écrire l'en-tête » remplace la ligne qui suit `En-tête :`. Un texte faux n'est pas enregistré ;
  un scénario créé ou modifié est non utilisable jusqu'à la validation de l'administrateur.
- Pas encore fait : Activer / Désactiver (programmés), Supprimer, `attendre`, conditions.

## Services
```bash
sudo systemctl status postgresql nginx      # état
sudo systemctl restart nginx                # redémarrer
```

## Tester SimDom à la main
```bash
nc -z 192.168.56.1 <port> && echo OK        # SimDom joignable ?
# lire l'état du luminaire nord du salon (réponse : dernier octet 00/01/02)
printf '\xc0\xa8\x00\x64\x00\x00\x00\x01\x00\x01\x01\x01\x04' | nc -w 2 192.168.56.1 <port> | od -An -tx1
```

## Depuis Windows
```powershell
ssh domotique                                                    # terminal dans la VM
code --remote ssh-remote+domotique /home/raph/projet-domotique   # ouvrir VSCode en ssh
```
## GIT
```bash
git status                          # voir ce qui a changé
git add .                           # préparer
git commit -m "Ce que j'ai fait"    # enregistrer en local
git push                            # envoyer sur GitHub
```

## nginx (serveur web frontal)
- `http://192.168.56.10/` → fichiers de `front/` (page de login)
- `http://192.168.56.10/api/...` → serveur C sur le port 8080
- Configuration versionnée : `nginx/domotique.conf`

Installation et activation (une seule fois) :
```bash
sudo apt install nginx
sudo ln -s /home/raph/projet-domotique/nginx/domotique.conf /etc/nginx/sites-enabled/domotique.conf
sudo rm /etc/nginx/sites-enabled/default    # supprime seulement le lien du site par défaut
chmod o+x /home/raph                         # sinon erreur 403 (www-data ne peut pas entrer)
sudo nginx -t && sudo systemctl reload nginx
```

Après une modification de `domotique.conf` :
```bash
sudo nginx -t && sudo systemctl reload nginx
```

Utile :
```bash
systemctl status nginx                     # état du service (q pour sortir)
sudo tail -f /var/log/nginx/error.log      # erreurs (403, 502...)
```

- 403 Forbidden : droits sur `/home/raph` (voir `chmod` ci-dessus).
- 502 Bad Gateway sur `/api` : le serveur C n'est pas lancé.
- Raspberry Pi : adapter la ligne `root` de `domotique.conf`.


## Accès depuis un téléphone (même réseau local que le PC)

La VM (192.168.56.10) n'est pas visible depuis le Wi-Fi : le PC sert de relais.
Téléphone → `http://<IP Wi-Fi du PC>:8080` → VirtualBox → nginx (port 80 de la VM).

Mise en place (une seule fois) :
1. VirtualBox : Configuration de la VM → Réseau → Carte 1 (NAT) → Avancé → Redirection de ports
   → TCP, IP hôte vide, port hôte `8080`, IP invité vide, port invité `80`.
2. Pare-feu Windows (PowerShell **en administrateur**) :
```powershell
   New-NetFirewallRule -DisplayName "Domotique HTTP 8080" -Direction Inbound -Protocol TCP -LocalPort 8080 -Action Allow -Profile Private
```

À chaque utilisation :
```powershell
ipconfig                   # IPv4 de la carte Wi-Fi du PC
Get-NetConnectionProfile   # le Wi-Fi doit être en Private
```