-- =============================================================================
-- Peuplement 4 : les types d'équipements
-- Fichier : sql/peuplement_de_la_bdd/04_type_equipement.sql
-- Aucune dépendance
-- =============================================================================
\set ON_ERROR_STOP on

INSERT INTO type_equipement (nom_type_equipement)
VALUES ('Luminaire'),
       ('Prise commandée'),
       ('Climatisation réversible'),
       ('Volet roulant / porte basculante')
ON CONFLICT DO NOTHING;