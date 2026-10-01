-- =============================================================================
-- Projet Domotique - Création des tables
-- Fichier : sql/01_creation_tables.sql
--
-- Lancement (depuis ~/projet-domotique) :
--   psql -h localhost -U domotique -d domotique -f sql/01_creation_tables.sql
--
-- ATTENTION : ce script SUPPRIME les tables existantes avant de les recréer
-- Toutes les données sont alors perdues : il faut relancer les peuplements.
-- =============================================================================

-- Arrêter le script à la première erreur (sinon psql continue)
\set ON_ERROR_STOP on

-- Tout ou rien : si une table échoue, aucune modification n'est gardée
BEGIN;

-- -----------------------------------------------------------------------------
-- 0. Nettoyage : suppression dans l'ordre inverse des dépendances
-- -----------------------------------------------------------------------------
DROP TABLE IF EXISTS historique_etat;
DROP TABLE IF EXISTS acces_piece;
DROP TABLE IF EXISTS scenario;
DROP TABLE IF EXISTS equipement;
DROP TABLE IF EXISTS utilisateur;
DROP TABLE IF EXISTS piece;
DROP TABLE IF EXISTS localisation;
DROP TABLE IF EXISTS batiment;
DROP TABLE IF EXISTS ip_equipement;
DROP TABLE IF EXISTS type_equipement;
DROP TABLE IF EXISTS type_utilisateur;

-- Extension qui fournit crypt() et gen_salt() : empreintes bcrypt des mots
-- de passe (utilisée par le peuplement des utilisateurs)
CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- -----------------------------------------------------------------------------
-- 1. Tables sans clé étrangère
-- -----------------------------------------------------------------------------

-- Profils : Administrateur, Propriétaire, Utilisateur, Invité, Surveillance
CREATE TABLE type_utilisateur (
    id_type_utilisateur   bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_type_utilisateur  varchar(50) NOT NULL UNIQUE
);

CREATE TABLE batiment (
    id_batiment   bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_batiment  varchar(100) NOT NULL UNIQUE
);

-- Luminaire, prise commandée, climatisation réversible, volet roulant
CREATE TABLE type_equipement (
    id_type_equipement   bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_type_equipement  varchar(50) NOT NULL UNIQUE
);

-- Automates (identifiants SimDom, pas de vraies adresses réseau)
-- Type inet : PostgreSQL vérifie que c'est bien une adresse IP
CREATE TABLE ip_equipement (
    id_ip_equipement  bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    ip_equipement     inet NOT NULL UNIQUE
);

-- -----------------------------------------------------------------------------
-- 2. Bâtiment -> niveaux -> pièces
-- -----------------------------------------------------------------------------

-- Niveaux : Extérieur, Rez-de-chaussée, Premier étage
CREATE TABLE localisation (
    id_localisation   bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_localisation  varchar(50) NOT NULL,
    fk_id_batiment    bigint NOT NULL REFERENCES batiment (id_batiment),
    UNIQUE (nom_localisation, fk_id_batiment)
);

-- Pièces : l'identifiant n'est PAS automatique, on reprend les numéros
-- de equipement.txt (1 à 45 et 200)
-- Une même pièce peut exister à deux niveaux (WC, Placard, Hall nord...)
-- d'où l'unicité sur le couple (nom, niveau)
CREATE TABLE piece (
    id_piece            bigint PRIMARY KEY,
    nom_piece           varchar(100) NOT NULL,
    fk_id_localisation  bigint NOT NULL REFERENCES localisation (id_localisation),
    UNIQUE (nom_piece, fk_id_localisation)
);

-- -----------------------------------------------------------------------------
-- 3. Utilisateurs
-- -----------------------------------------------------------------------------

-- password_hash : empreinte bcrypt, jamais le mot de passe en clair
-- date_expiration : vide (NULL) = le compte n'expire pas
CREATE TABLE utilisateur (
    id_utilisateur          bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom                     varchar(100) NOT NULL,
    prenom                  varchar(100) NOT NULL,
    email                   varchar(255) NOT NULL UNIQUE,
    password_hash           text NOT NULL,
    date_expiration         timestamptz,
    fk_id_type_utilisateur  bigint NOT NULL REFERENCES type_utilisateur (id_type_utilisateur),
    fk_id_batiment          bigint NOT NULL REFERENCES batiment (id_batiment)
);

-- Pièces qu'un Invité peut piloter : une ligne = une autorisation
-- ON DELETE CASCADE : si l'utilisateur ou la pièce est supprimé(e),
-- ses autorisations disparaissent avec
CREATE TABLE acces_piece (
    fk_id_utilisateur  bigint NOT NULL REFERENCES utilisateur (id_utilisateur) ON DELETE CASCADE,
    fk_id_piece        bigint NOT NULL REFERENCES piece (id_piece) ON DELETE CASCADE,
    PRIMARY KEY (fk_id_utilisateur, fk_id_piece)
);

-- -----------------------------------------------------------------------------
-- 4. Équipements
-- -----------------------------------------------------------------------------

-- numero_equipement : entrée de l'automate, 8 caractères '0' ou '1'
-- etat : 0 éteint/fermé, 1 allumé/ouvert, 2 en panne, NULL = indéterminé
-- mode : climatisations seulement (0 climatisation, 1 chauffage), NULL sinon
-- puissance : en watts, NULL si inconnue
CREATE TABLE equipement (
    id_equipement          bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_equipement         varchar(150) NOT NULL,
    numero_equipement      char(8) NOT NULL CHECK (numero_equipement ~ '^[01]{8}$'),
    etat                   smallint CHECK (etat IN (0, 1, 2)),
    mode                   smallint CHECK (mode IN (0, 1)),
    puissance              integer CHECK (puissance >= 0),
    fk_id_ip_equipement    bigint NOT NULL REFERENCES ip_equipement (id_ip_equipement),
    fk_id_type_equipement  bigint NOT NULL REFERENCES type_equipement (id_type_equipement),
    fk_id_piece            bigint NOT NULL REFERENCES piece (id_piece),
    -- un automate n'a qu'un équipement par entrée
    UNIQUE (fk_id_ip_equipement, numero_equipement),
    -- les scénarios LDSD désignent un équipement par son nom ET sa pièce
    UNIQUE (nom_equipement, fk_id_piece)
);

-- Une ligne seulement quand l'état d'un équipement change
-- etat peut être NULL (passage en « indéterminé »)
CREATE TABLE historique_etat (
    id_historique     bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    etat              smallint CHECK (etat IN (0, 1, 2)),
    mode              smallint CHECK (mode IN (0, 1)),
    date_heure        timestamptz NOT NULL DEFAULT now(),
    fk_id_equipement  bigint NOT NULL REFERENCES equipement (id_equipement) ON DELETE CASCADE
);

-- Accélère les statistiques : « historique de tel équipement, dans l'ordre »
CREATE INDEX idx_historique_equipement_date
    ON historique_etat (fk_id_equipement, date_heure);

-- -----------------------------------------------------------------------------
-- 5. Scénarios
-- -----------------------------------------------------------------------------

-- utilisable : true quand le scénario a été vérifié sans erreur
-- fk_id_utilisateur : créateur (on ne peut pas supprimer un utilisateur
-- qui a encore des scénarios)
CREATE TABLE scenario (
    id_scenario        bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nom_scenario       varchar(100) NOT NULL UNIQUE,
    script_ldsd        text NOT NULL,
    utilisable         boolean NOT NULL DEFAULT false,
    fk_id_utilisateur  bigint NOT NULL REFERENCES utilisateur (id_utilisateur)
);

COMMIT;