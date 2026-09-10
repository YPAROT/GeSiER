# Prompt 12 — Historique, intégrité et migrations

Achever la robustesse transverse du fichier projet GeSiER.

Journaliser création, modification, suppression, archivage, import, export draft,
publication et association GED avec auteur, date, objet et valeurs avant/après.
Créer une page Historique filtrable permettant d'ouvrir l'objet concerné. Ne pas
dupliquer inutilement les événements d'une transaction groupée.

Centraliser les contraintes d'intégrité, suppressions contrôlées et listes de
bloquants. Versionner le schéma ; avant migration, créer une sauvegarde
récupérable et un rapport d'ambiguïtés. Tester toutes les migrations depuis des
bases historiques représentatives. Enregistrer sous doit copier sans altérer
l'original et récupérer proprement après échec.

Ajouter diagnostic du projet, vérification `foreign_key_check`, sauvegarde
manuelle et restauration guidée. Critères : aucune perte silencieuse, rollback
sur erreur injectée, historique complet, anciens projets ouvrables et aucun
warning SQLite durant tous les cycles de vie.
