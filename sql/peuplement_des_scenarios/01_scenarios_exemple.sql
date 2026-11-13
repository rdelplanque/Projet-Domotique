-- =============================================================================
-- Peuplement scénarios 1 : deux scénarios d'exemple
-- Fichier : sql/peuplement_des_scenarios/01_scenarios_exemple.sql
-- À lancer APRÈS les utilisateurs (le créateur est retrouvé par son e-mail)
-- et après 02_ajout_scenario_actif.sql.
-- =============================================================================
\set ON_ERROR_STOP on

-- Le texte LDSD est écrit entre $$ ... $$ : PostgreSQL le garde tel quel,
-- retours à la ligne et guillemets compris (pas besoin de les échapper).
-- utilisable = true : on les valide à la main, le parser n'existe pas encore.
-- ON CONFLICT : si le nom existe déjà, on remet le texte de ce fichier
-- (script rejouable).
INSERT INTO scenario (nom_scenario, script_ldsd, utilisable, actif, fk_id_utilisateur)
SELECT s.nom, s.script, true, false, u.id_utilisateur
FROM (VALUES
    ('Soirée salon',
$$En-tête :
au clic
Liste :
fermer tous les volets de "Salon" de "Rez-de-chaussée"
allumer "Luminaire salon nord" de "Salon" de "Rez-de-chaussée"
allumer "Applique cheminée nord" de "Salon" de "Rez-de-chaussée"
allumer "Applique cheminée sud" de "Salon" de "Rez-de-chaussée"
éteindre "Luminaire salon sud" de "Salon" de "Rez-de-chaussée"$$,
     'admin@domotique.local'),

    ('Réveil du week-end',
$$En-tête :
chaque samedi et dimanche à 08:00
Liste :
ouvrir tous les volets de toutes les pièces de "Premier étage"
allumer toutes les climatisations de toutes les pièces de "Premier étage" en chauffage$$,
     'proprietaire@domotique.local')
) AS s (nom, script, email)
JOIN utilisateur u ON u.email = s.email
ON CONFLICT (nom_scenario) DO UPDATE SET script_ldsd = EXCLUDED.script_ldsd;
