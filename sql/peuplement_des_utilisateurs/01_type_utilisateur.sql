-- =============================================================================
-- Peuplement utilisateurs 1 : les profils
-- Fichier : sql/peuplement_des_utilisateurs/01_type_utilisateur.sql
-- À lancer APRÈS tout peuplement_de_la_bdd/
-- =============================================================================
\set ON_ERROR_STOP on

-- Les 5 profils du cahier des charges (§ 4.1). Leurs droits ne sont pas en
-- base : ils sont codés dans le serveur C (module d'authentification).
INSERT INTO type_utilisateur (nom_type_utilisateur)
VALUES ('Administrateur'),
       ('Propriétaire'),
       ('Utilisateur'),
       ('Invité'),
       ('Surveillance')
ON CONFLICT DO NOTHING;