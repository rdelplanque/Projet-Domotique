-- =============================================================================
-- Peuplement 1 : le bâtiment
-- Fichier : sql/peuplement_de_la_bdd/01_batiment.sql
-- Lancement : psql -h localhost -U domotique -d domotique -f sql/peuplement_de_la_bdd/01_batiment.sql
-- =============================================================================
\set ON_ERROR_STOP on

-- On ne donne pas id_batiment : PostgreSQL le génère (1 pour le premier).
-- ON CONFLICT DO NOTHING : si ce nom existe déjà (script relancé),
-- on ne fait rien au lieu de provoquer une erreur.
INSERT INTO batiment (nom_batiment)
VALUES ('Maison SimDom')
ON CONFLICT DO NOTHING;