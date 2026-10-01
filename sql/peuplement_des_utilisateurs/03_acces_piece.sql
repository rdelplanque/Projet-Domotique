-- =============================================================================
-- Peuplement utilisateurs 3 : les pièces autorisées aux invités
-- Fichier : sql/peuplement_des_utilisateurs/03_acces_piece.sql
-- À lancer APRÈS 02_utilisateurs.sql
-- =============================================================================
\set ON_ERROR_STOP on

-- Chaque compte de profil « Invité » reçoit le droit de piloter la chambre
-- invités, sa salle de bain et son dressing (pièces 9, 10 et 11).
-- Une ligne = une autorisation ; pas de ligne = pas d'accès.
-- On ne cite aucun email : le fichier reste valable quels que soient les
-- comptes créés dans 02_utilisateurs.sql.
INSERT INTO acces_piece (fk_id_utilisateur, fk_id_piece)
SELECT u.id_utilisateur, p.id_piece
FROM utilisateur u
JOIN type_utilisateur t ON t.id_type_utilisateur = u.fk_id_type_utilisateur
CROSS JOIN piece p
WHERE t.nom_type_utilisateur = 'Invité'
  AND p.id_piece IN (9, 10, 11)
ON CONFLICT DO NOTHING;