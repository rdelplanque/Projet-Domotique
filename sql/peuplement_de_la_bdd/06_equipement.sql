-- =============================================================================
-- Peuplement 6 : les équipements
-- Fichier : sql/peuplement_de_la_bdd/06_equipement.sql
-- À lancer APRÈS 03, 04 et 05, DEPUIS ~/projet-domotique
-- Source : lecture_equipement/equipements.json (produit par lecture_equipement.c)
-- =============================================================================
\set ON_ERROR_STOP on

\set equipements `cat lecture_equipement/equipements.json`

-- Chaque objet JSON devient une ligne. L'automate et le type sont
-- retrouvés par leur nom pour obtenir leurs identifiants.
-- ON CONFLICT : si l'équipement (automate + entrée) existe déjà, on l'ignore
-- (script rejouable). Toute autre erreur arrête le script.
INSERT INTO equipement (numero_equipement, etat, nom_equipement, fk_id_piece,
                        puissance, mode, fk_id_ip_equipement, fk_id_type_equipement)
SELECT e.numero, e.etat, e.nom, e.piece, e.puissance, e.mode,
       ip.id_ip_equipement, t.id_type_equipement
FROM json_to_recordset(:'equipements'::json)
     AS e (ip text, numero text, etat smallint, nom text, piece bigint,
           puissance integer, mode smallint, type text)
JOIN ip_equipement   ip ON ip.ip_equipement = e.ip::inet
JOIN type_equipement t  ON t.nom_type_equipement = e.type
ON CONFLICT (fk_id_ip_equipement, numero_equipement) DO NOTHING;