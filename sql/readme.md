# Base de données : création et peuplement

## Principe
- Les tables sont créées par un **script SQL** lancé à la main avec `psql`, pas par le serveur C.
  Le serveur se contentera de vérifier, au démarrage, que les tables existent.
- Les données de la maison viennent de `equipement.txt` (fourni par le simulateur).
  Le programme `lecture_equipement` le convertit en JSON, puis les scripts SQL lisent ce JSON.

```
equipement.txt ──► lecture_equipement (C) ──► pieces.json, equipements.json ──► scripts SQL ──► base
```

## Organisation
```
lecture_equipement/
  equipement.txt            fichier du simulateur (une correction manuelle, voir plus bas)
  lecture_equipement.c      lecture du .txt et écriture des JSON
  pieces.json               généré (46 pièces)
  equipements.json          généré (211 équipements)
sql/
  01_creation_tables.sql    supprime puis recrée les 11 tables
  peuplement_de_la_bdd/     la maison (sur Git)
    01_batiment.sql
    02_localisation.sql     Extérieur, Rez-de-chaussée, Premier étage
    03_piece.sql            lit pieces.json
    04_type_equipement.sql
    05_ip_equipement.sql    les 7 automates
    06_equipement.sql       lit equipements.json
  peuplement_des_utilisateurs/   les comptes (à venir ; le fichier avec mots de passe n'est pas sur Git)
```

## Ordre d'exécution
L'ordre compte : une table qui a une clé étrangère se remplit **après** la table qu'elle référence.
1. `lecture_equipement` (seulement si `equipement.txt` a changé)
2. `01_creation_tables.sql`
3. `peuplement_de_la_bdd/01` à `06`, dans l'ordre
4. `peuplement_des_utilisateurs/` (à venir)

Les commandes sont dans le README principal. Toujours lancer `psql` depuis `~/projet-domotique`
(les scripts 03 et 06 trouvent les JSON par un chemin relatif).

## Bon à savoir
- **`-h localhost` est obligatoire** : sans lui, PostgreSQL compare le nom de l'utilisateur Linux (`raph`)
  à l'utilisateur PostgreSQL (`domotique`) et refuse la connexion (« Peer authentication failed »).
- **`01_creation_tables.sql` efface toutes les données.** Après l'avoir relancé, il faut relancer tous les peuplements.
- **Les scripts de peuplement sont rejouables** (`ON CONFLICT DO NOTHING`) : les relancer ne crée pas de doublon.
- **Une erreur arrête le script** (`\set ON_ERROR_STOP on`) ; comme chaque bloc est inséré en une
  seule instruction, une seule ligne fautive annule tout le bloc.
- **Les identifiants des pièces** sont ceux de `equipement.txt` (1 à 45, 200) ; les autres sont générés par PostgreSQL.
- **Noms des équipements** : le programme garde la partie après le dernier « - »
  (« Salon - Luminaire salon nord » → « Luminaire salon nord », dans la pièce Salon).

## Anomalies de `equipement.txt`
- **Corrigée à la main** dans `lecture_equipement/equipement.txt` : luminaire central de la salle de bain de la
  chambre nord est (`.110 / 00000011`), pièce 30 → 31. Sans cette correction, la pièce 30 avait deux
  « Luminaire central » et la contrainte d'unicité (nom, pièce) bloquait le chargement.
- **Pas encore corrigée** (exercice à venir) : les équipements de la salle de bain et du dressing de la
  suite parentale sont rangés en pièces 10 / 11 (celles de la chambre invités) au lieu de 13 / 14.