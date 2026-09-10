# Prompt 07 — Interfaces et matrice N²

Créer la page métier Interfaces et finaliser la matrice N².

Une interface relie deux éléments PT, possède code, description, statut,
plusieurs types configurables, exigences associées et un ou plusieurs documents
ICD avec chapitre facultatif. Fournir liste filtrable et fiche éditable, gestion
des catalogues et navigation vers PT, exigences et documents. Historiser toutes
les opérations.

La N² utilise le même ordre de Product Tree sur les deux axes. Distinguer absence
d'interface, interface couverte par ICD, interface non couverte et interfaces ou
types multiples. Une cellule ouvre le détail et permet la navigation. Ajouter
filtres, légende accessible, taux de couverture ICD et export XLSX.

Critères d'acceptation : données bidirectionnelles cohérentes, N² exacte après
toute modification, détails cliquables, archivage sûr, export multi-onglets,
migration non destructive et tests Qt 6/CMake.
