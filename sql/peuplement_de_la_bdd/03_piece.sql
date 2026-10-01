-- =============================================================================
-- Peuplement 3 : les pièces et emplacements
-- Fichier : sql/peuplement_de_la_bdd/03_piece.sql
-- À lancer APRÈS 02_localisation.sql, DEPUIS ~/projet-domotique
-- Source : lecture_equipement/pieces.json (produit par lecture_equipement.c)
-- =============================================================================
\set ON_ERROR_STOP on

-- psql lit le fichier JSON (commande shell entre accents graves)
-- et range tout son contenu dans la variable « pieces »
\set pieces `cat lecture_equipement/pieces.json`

-- json_to_recordset transforme le tableau JSON en lignes de table :
-- un objet {"id":..., "nom":..., "niveau":...} = une ligne (id, nom, niveau).
-- L'identifiant de la pièce est celui de equipement.txt (1 à 45, 200).
-- Le niveau est retrouvé par son nom dans la table localisation.
INSERT INTO piece (id_piece, nom_piece, fk_id_localisation)
SELECT p.id, p.nom, l.id_localisation
FROM json_to_recordset(:'pieces'::json) AS p (id bigint, nom text, niveau text)
JOIN localisation l ON l.nom_localisation = p.niveau
ON CONFLICT DO NOTHING;