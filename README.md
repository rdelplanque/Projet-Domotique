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
Depuis Windows : `lancer.ps1` en parxershell (démarre la VM, détecte le port de SimDom, lance `demarrer.sh`).

## Base de données
```bash
psql -U domotique -d domotique              # se connecter
psql -U domotique -d domotique -f sql/<fichier>.sql   # exécuter un script
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
