# Prompt 09 — Imports et exports CSV/XLSX généralisés

Transformer l'import actuel en infrastructure réutilisable pour exigences,
documents et matrices, sans dépendre d'Excel à l'exécution.

Pour XLSX : choix d'onglet, aperçu, ligne d'en-tête, lignes ignorées, mapping,
transformations et profils réutilisables. Pour CSV : séparateur et encodage.
Valider entièrement avant écriture. Regrouper les erreurs identiques et permettre
l'import confirmé du sous-ensemble valide. Gérer créations, mises à jour,
conservation, omission et rejet par doublon.

Finaliser l'import de spécification : document nouveau/existant, source par
défaut, sections hiérarchiques, résolution des parents absents, méthodes,
niveaux PT et configurations. Une décision peut s'appliquer à toutes les
occurrences d'une même valeur normalisée. Ne jamais créer un PT implicitement.
Garantir une transaction unique et aucun document vide.

Créer des exports CSV/XLSX configurables : données/filtres, colonnes, ordre,
en-têtes, nom d'onglet et profils. Les matrices utilisent synthèse, détails et
anomalies. Critères : cellules vides non destructives en mise à jour, rollback
complet, rapports précis, gros fichiers testés et compilation Qt 6/CMake.
