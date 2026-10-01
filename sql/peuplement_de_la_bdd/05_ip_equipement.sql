-- =============================================================================
-- Peuplement 5 : les automates
-- Fichier : sql/peuplement_de_la_bdd/05_ip_equipement.sql
-- Aucune dépendance
-- =============================================================================
\set ON_ERROR_STOP on

-- Identifiants des automates SimDom (pas de vraies adresses réseau)
--   .100 luminaires RdC + extérieur   .110 luminaires 1er étage
--   .101 prises commandées
--   .102 climatisations RdC           .112 climatisations 1er étage (messages de 14 octets)
--   .103 volets RdC                   .113 volets 1er étage
INSERT INTO ip_equipement (ip_equipement)
VALUES ('192.168.0.100'), 
        ('192.168.0.101'), 
        ('192.168.0.102'), 
        ('192.168.0.103'),
        ('192.168.0.110'), 
        ('192.168.0.112'), 
        ('192.168.0.113')
ON CONFLICT DO NOTHING;