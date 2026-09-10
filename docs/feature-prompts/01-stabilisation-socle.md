# Prompt 01 — Stabilisation du socle

Finaliser le socle actuel de GeSiER avant toute nouvelle fonction métier.

Inspecter les modifications locales existantes et les préserver. Corriger tous
les accès SQL effectués sans projet ouvert, les durées de vie de modèles Qt et
les avertissements `removeDatabase`. Tester ouverture, fermeture, remplacement
et Enregistrer sous, avec chaque page active et sans projet chargé.

Retirer de l'interface utilisateur les pages SQL, Custom SQL et les anciennes
vues désormais remplacées. Ne retirer leur code qu'après avoir vérifié qu'aucune
fonction moderne n'en dépend. Conserver uniquement Tableau de bord et Projet,
avec la navigation latérale métier. Désactiver les actions nécessitant une base
quand aucun projet n'est ouvert et afficher des états vides explicites.

Uniformiser le formatage des nouveaux fichiers, éliminer les avertissements de
compilation et documenter l'architecture services/widgets/migrations. Mettre le
README à jour. Ne pas modifier `.qtcreator/`.

Critères d'acceptation : Qt 6/CMake compile avec `-Wall -Wextra -Wpedantic`, tous
les tests passent, les parcours avec et sans projet ne produisent aucun message
Qt SQL, aucun modèle ne reste attaché à `REQ_DB`, et l'interface historique SQL
n'est plus accessible.
