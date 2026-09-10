# Prompt 03 — Documents structurés enrichis

Finaliser l'éditeur de documents de GeSiER. Un document possède une référence
principale, des références secondaires, un titre, un type, une description, des
métadonnées configurables et un arbre ordonné.

Fiabiliser ajout, renommage, suppression et glisser-déposer des chapitres et des
occurrences d'exigences. Interdire les cycles et conserver exactement l'ordre
après réouverture. Une exigence n'a qu'une occurrence par document mais peut
figurer dans plusieurs documents. Le double-clic ouvre sa fiche ; l'arbre affiche
`CODE — Titre` et reflète immédiatement toute modification de l'exigence.

Ajouter les nœuds texte riche et image avec légende, en préservant une structure
extensible. Permettre leur édition, déplacement et suppression. Ajouter une
prévisualisation logique du document, sans générer encore le DOCX. Journaliser
les changements de composition dans des transactions atomiques.

Critères d'acceptation : structure persistante et ordonnée, navigation croisée
Document/Exigence, aucune copie des données d'exigence, texte et images gérés,
rollback sur erreur, migration non destructive et tests Qt 6/CMake.
