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
Depuis Windows : `lancer.ps1` sous powershell (démarre la VM, détecte le port de SimDom, lance `demarrer.sh` sur la VM).

## Base de données
# 1 automatiser les données de la BDD
'''
cd lecture_equipement
gcc -Wall -Wextra -std=c11 -o lecture_equipement *.c
./lecture_equipement equipement.txt 
'''
# 2 la bdd
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

# exemple de vérification:
# toutes les tables
psql -h localhost -U domotique -d domotique -c '\dt'        

psql -h localhost -U domotique -d domotique -c 'SELECT * FROM localisation'
psql -h localhost -U domotique -d domotique -c 'SELECT count(*) FROM piece'      
psql -h localhost -U domotique -d domotique -c 'SELECT count(*) FROM equipement' 
```

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
'''
git status                          # voir ce qui a changé
git add .                           # préparer
git commit -m "Ce que j'ai fait"    # enregistrer en local
git push                            # envoyer sur GitHub
'''

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


## Accès depuis un téléphone (même Wi-Fi que le PC)

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
- Test sur le PC : `http://localhost:8080`
- Wi-Fi d'école ou public (appareils isolés) : utiliser le point d'accès mobile du PC ou du téléphone.