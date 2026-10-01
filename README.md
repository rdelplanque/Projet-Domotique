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
Depuis Windows : `lancer.ps1` en parxershell (démarre la VM, détecte le port de SimDom, lance `demarrer.sh` sur la VM).

## Base de données
# 1 automatiser les données de la BDD
'''
cd lecture_equipement
gcc -Wall -Wextra -std=c11 -o lecture_equipement lecture_equipement.c
./lecture_equipement equipement.txt
'''
# 2 la bdd
```bash
# se connecter
psql -U domotique -d domotique              
# exécuter le fichier de création des tables
psql -h localhost -U domotique -d domotique -f sql/01_creation_tables.sql   
# création des tables:
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/01_batiment.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/02_localisation.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/03_piece.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/04_type_equipement.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/05_ip_equipement.sql
psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/06_equipement.sql

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