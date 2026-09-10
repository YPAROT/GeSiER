# Prompt 06 — Vérification et matrice de vérification

Finaliser la planification de vérification sans remplacer Redmine pour
l'exécution AIT.

Chaque exigence accepte plusieurs méthodes uniques. Chaque ligne contient
méthode, niveau PT, procédure/cas de test, lien ou identifiant Redmine, moyen,
verdict vide/C/PC/NC et commentaire. Le niveau est un élément PT actif ; une
ancienne référence archivée reste visible. Aucun champ d'avancement n'est géré.

Créer une page Vérification et une matrice filtrable/exportable. Une exigence est
couverte si au moins une même ligne contient méthode et niveau PT valide.
Afficher les manques, permettre l'ouverture de l'exigence et produire un XLSX
avec synthèse, détails et anomalies. Les liens Redmine restent de simples
références ouvrables.

Critères d'acceptation : méthodes multiples ordonnées, doublons refusés, niveau
PT stable après déplacement/renommage, couverture exacte, matrice navigable,
export testé et migration des anciennes colonnes sans perte.
