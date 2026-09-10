# Architecture de GeSiER

GeSiER est une application Qt Widgets dont un projet correspond à un fichier
SQLite. La connexion nommée `REQ_DB` n'existe que pendant l'ouverture d'un
projet. Aucun widget ne doit conserver de modèle ou de requête SQL au-delà de
la fermeture de cette connexion.

## Services

Les classes `RequirementService`, `RequirementRelationService`,
`ProductTreeService` et `DocumentService` portent les règles métier et les
transactions. Elles reçoivent le nom de connexion sans devenir propriétaires
de la base. `REQ_SQLManager` est seul responsable de créer, ouvrir, copier,
fermer et retirer `REQ_DB`.

## Widgets

`MainWindow` expose deux pages de premier niveau : **Tableau de bord** et
**Projet**. La page Projet contient la navigation métier et les widgets
spécialisés. Chaque widget reçoit une connexion avec `setConnectionName()` et
doit accepter une chaîne vide pour revenir à son état sans projet. Avant une
fermeture, un remplacement ou un « Enregistrer sous », `releaseProjectViews()`
détache tous les modèles et toutes les vues de la base.

Les anciens formulaires SQL restent provisoirement compilés parce que certaines
pages métier réutilisent encore leurs widgets d'édition. Ils ne sont plus
accessibles comme pages SQL ou Custom SQL dans l'interface principale.

## Migrations

`DatabaseMigrator` fait évoluer les fichiers existants jusqu'à
`CurrentVersion`. L'ouverture crée d'abord une sauvegarde lorsque la version du
schéma est ancienne. Une migration doit être transactionnelle, idempotente et
préserver les données personnalisées. Les nouvelles structures sont ajoutées
par étapes et la version SQLite `user_version` n'est mise à jour qu'après
succès.

## Durée de vie SQL

Avant `QSqlDatabase::removeDatabase("REQ_DB")`, les vues détachent leur modèle,
les modèles sont détruits et les objets `QSqlQuery`/`QSqlDatabase` temporaires
sortent de portée. Les accès utilisent `QSqlDatabase::database(name, false)` et
vérifient `contains()`, `isValid()` et `isOpen()` afin de ne jamais créer une
connexion implicite lorsque l'application est dans l'état sans projet.
