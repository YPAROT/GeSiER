# Prompt 05 — Applicabilité et configurations

Finaliser la gestion des configurations et l'applicabilité des exigences.

Créer une page de gestion des configurations avec code unique, libellé,
description, ordre, état actif/archivé et historique. Conserver visiblement les
configurations archivées encore utilisées. Dans la fiche Exigence, proposer une
liste multisélection claire avec Enregistrer/Annuler.

Ajouter la matrice d'applicabilité exigences × configurations, filtrable par
branche Product Tree, document, statut et type. Prévoir l'extension par couple
PT/configuration sans casser l'applicabilité simple existante. Exporter la
matrice en XLSX avec synthèse, détails et anomalies.

Critères d'acceptation : unicité des associations, archivage sans perte, filtres
combinables, taux d'applicabilité exact et cliquable, migration non destructive,
export vérifié et tests Qt 6/CMake.
