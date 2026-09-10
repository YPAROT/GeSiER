# Prompt 04 — DOCX, templates, drafts et publications

Implémenter la génération DOCX à partir des documents structurés. Utiliser une
bibliothèque intégrée au projet CMake et ne pas exiger Microsoft Word.

Permettre de charger et valider un template DOCX par projet/document. Réutiliser
styles, marges, en-têtes, pieds de page et zones réservées pour références,
métadonnées, sommaire et contenu. Générer chapitres, exigences, textes, images et
légendes dans leur ordre. Signaler les zones ou styles manquants avant export.

Distinguer strictement export draft et publication. Le draft accepte un libellé
provisoire et un filigrane DRAFT, n'exige ni version ni GED et ne crée qu'un
événement d'export. La publication exige référence, version et titre, conserve
un instantané complet, auteur/date/template/métadonnées, empreinte SHA-256 et
permet d'ajouter ensuite l'identifiant ou lien GED.

Critères d'acceptation : rendu fidèle au template, exigence actualisée dans tout
nouvel export, drafts régénérables, publications immuables et traçables, hash
vérifiable, erreurs explicites et tests sur plusieurs templates DOCX.
