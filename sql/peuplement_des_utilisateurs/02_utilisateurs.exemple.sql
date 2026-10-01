-- =============================================================================
-- Peuplement utilisateurs 2 : les comptes de test
-- Fichier MODÈLE : sql/peuplement_des_utilisateurs/02_utilisateurs.exemple.sql
--
-- Mode d'emploi :
--   1. copier ce fichier en 02_utilisateurs.sql (même dossier)
--   2. dans la copie, remplacer chaque 'A_CHANGER_...' par un vrai mot de passe
--   3. lancer la copie (elle n'est PAS envoyée sur Git, voir .gitignore)
-- =============================================================================
\set ON_ERROR_STOP on

-- Le mot de passe n'est jamais stocké : crypt() calcule son empreinte bcrypt
-- (gen_salt('bf', 10) = bcrypt, coût 10, avec un « sel » aléatoire différent
-- pour chaque compte). Même la base ne connaît pas le mot de passe.
--
-- Le profil et le bâtiment sont retrouvés par leur nom.
-- date_expiration : NULL = jamais ; ici seul l'Invité expire (dans 30 jours).
-- ON CONFLICT DO NOTHING : un email déjà présent est ignoré (script rejouable).
INSERT INTO utilisateur (nom, prenom, email, password_hash, date_expiration,
                         fk_id_type_utilisateur, fk_id_batiment)
SELECT u.nom, u.prenom, u.email,
       crypt(u.mot_de_passe, gen_salt('bf', 10)),
       u.expiration,
       t.id_type_utilisateur, b.id_batiment
FROM (VALUES
    -- nom,      prénom,         email,                           mot de passe,              expiration,                 profil
    ('Admin',    'Test',         'admin@domotique.local',         'A_CHANGER_admin',         NULL::timestamptz,          'Administrateur'),
    ('Proprio',  'Test',         'proprietaire@domotique.local',  'A_CHANGER_proprietaire',  NULL,                       'Propriétaire'),
    ('User',     'Test',         'utilisateur@domotique.local',   'A_CHANGER_utilisateur',   NULL,                       'Utilisateur'),
    ('Invite',   'Test',         'invite@domotique.local',        'A_CHANGER_invite',        now() + interval '30 days', 'Invité'),
    ('Surveil',  'Test',         'surveillance@domotique.local',  'A_CHANGER_surveillance',  NULL,                       'Surveillance')
) AS u (nom, prenom, email, mot_de_passe, expiration, profil)
JOIN type_utilisateur t ON t.nom_type_utilisateur = u.profil
JOIN batiment b         ON b.nom_batiment = 'Maison SimDom'
ON CONFLICT DO NOTHING;