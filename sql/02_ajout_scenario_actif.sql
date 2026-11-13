-- =============================================================================
-- Mise à jour 2 : colonne « actif » de la table scenario
-- Fichier : sql/02_ajout_scenario_actif.sql
-- Pour une base DÉJÀ créée : ajoute la colonne sans rien effacer.
-- (Une base créée avec la nouvelle version de 01_creation_tables.sql
--  l'a déjà : ce script ne fait alors rien.)
-- =============================================================================
\set ON_ERROR_STOP on

-- actif : true quand un scénario programmé attend son heure (bouton Activer)
ALTER TABLE scenario
    ADD COLUMN IF NOT EXISTS actif boolean NOT NULL DEFAULT false;
