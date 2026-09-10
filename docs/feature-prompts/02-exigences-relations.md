# Prompt 02 — Exigences et relations abouties

Finaliser la vue Exigences de GeSiER et ses relations. Préserver les identifiants
et données existants et utiliser des services transactionnels ne conservant que
le nom de connexion SQLite.

Achever les filtres multisélections, préférences de colonnes, allocation PT
multiple et PT principal. Garantir qu'aucune exigence n'est sélectionnée ou
créée par défaut. Ajouter une recherche efficace par code/titre lors du choix
d'une relation et afficher statut, type et allocations PT.

Présenter séparément parents/enfants de décomposition, sources/dérivées et
dépendances/dépendantes. Permettre ajout dans les deux sens, commentaire,
ouverture par double-clic et suppression. Finaliser la vue graphique avec
parents au-dessus, enfants en dessous, styles distincts par type, profondeur
réglable et navigation par clic. Refuser auto-référence, doublon et cycle de
décomposition et journaliser toute opération.

Critères d'acceptation : relations multiples dans tous les sens, listes et graphe
cohérents, navigation directe fonctionnelle même sous filtres, protection des
modifications non enregistrées, tests des cycles/doublons et compilation Qt 6
sans nouvel avertissement.
