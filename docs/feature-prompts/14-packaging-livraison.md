# Prompt 14 — Packaging et livraison

Préparer une version distribuable de GeSiER pour Windows après validation de
tous les lots métier.

Définir version applicative et version de schéma, icône, métadonnées, licence et
notes de version. Produire avec CMake un paquet incluant Qt, pilote SQLite et
toutes les bibliothèques nécessaires, sans dépendance à Qt Creator, Excel ou
Word. Ne jamais inclure de base utilisateur ou de configuration personnelle.

Ajouter une procédure d'installation/mise à jour/désinstallation et une version
portable si retenue. Tester sur une machine propre, chemins avec espaces et
accents, projet ancien, création de projet, import XLSX, export DOCX et fermeture.
Prévoir sauvegarde avant migration et diagnostic exploitable en cas d'échec.

Critères d'acceptation : installation reproductible, application autonome,
signature ou hash de livraison, aucune DLL manquante, aucun warning au démarrage
ou à l'arrêt, tests automatiques exécutés avant packaging et manuel inclus.
