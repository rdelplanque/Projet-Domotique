-- =============================================================================
-- Peuplement 2 : les niveaux (localisations) du bâtiment
-- Fichier : sql/peuplement_de_la_bdd/02_localisation.sql
-- À lancer APRÈS 01_batiment.sql
-- =============================================================================
\set ON_ERROR_STOP on

-- On ne connaît pas l'identifiant du bâtiment (il est généré automatiquement) :
-- on le retrouve par son nom avec un SELECT.
-- Le SELECT renvoie une ligne par niveau listé dans VALUES, chaque ligne
-- étant associée à l'identifiant du bâtiment « Maison SimDom ».
INSERT INTO localisation (nom_localisation, fk_id_batiment)
SELECT niveau.nom, b.id_batiment
FROM (VALUES ('Extérieur'),
             ('Rez-de-chaussée'),
             ('Premier étage')) AS niveau (nom)
CROSS JOIN batiment b
WHERE b.nom_batiment = 'Maison SimDom'
ON CONFLICT DO NOTHING;