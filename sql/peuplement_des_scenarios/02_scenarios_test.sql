-- =============================================================================
-- Peuplement scénarios 2 : scénarios de test (toute la maison)
-- Fichier : sql/peuplement_des_scenarios/02_scenarios_test.sql
-- Tous directs (« au clic »), créés par l'administrateur, déjà utilisables.
-- Rejouable : un nom déjà présent reprend le texte de ce fichier.
-- Attention : « tous les volets » ouvre aussi les 4 portes de garage.
-- =============================================================================
\set ON_ERROR_STOP on

INSERT INTO scenario (nom_scenario, script_ldsd, utilisable, actif, fk_id_utilisateur)
SELECT s.nom, s.script, true, false, u.id_utilisateur
FROM (VALUES
    ('Test - Allumer tous les luminaires',
$$En-tête :
au clic
Liste :
allumer tous les luminaires de toutes les pièces de tous les étages$$),

    ('Test - Éteindre tous les luminaires',
$$En-tête :
au clic
Liste :
éteindre tous les luminaires de toutes les pièces de tous les étages$$),

    ('Test - Ouvrir tous les volets',
$$En-tête :
au clic
Liste :
ouvrir tous les volets de toutes les pièces de tous les étages$$),

    ('Test - Fermer tous les volets',
$$En-tête :
au clic
Liste :
fermer tous les volets de toutes les pièces de tous les étages$$),

    ('Test - Clims en chauffage',
$$En-tête :
au clic
Liste :
allumer toutes les climatisations de toutes les pièces de tous les étages en chauffage$$),

    ('Test - Clims en climatisation',
$$En-tête :
au clic
Liste :
allumer toutes les climatisations de toutes les pièces de tous les étages en climatisation$$),

    ('Test - Éteindre toutes les clims',
$$En-tête :
au clic
Liste :
éteindre toutes les climatisations de toutes les pièces de tous les étages$$)
) AS s (nom, script)
JOIN utilisateur u ON u.email = 'admin@domotique.local'
ON CONFLICT (nom_scenario) DO UPDATE SET script_ldsd = EXCLUDED.script_ldsd;
