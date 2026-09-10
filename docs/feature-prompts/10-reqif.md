# Prompt 10 — Interopérabilité ReqIF

Implémenter une première interopérabilité ReqIF robuste en s'appuyant uniquement
sur la spécification officielle du format et une bibliothèque compatible avec
la distribution du logiciel.

Importer/exporter identifiants et codes, titres, descriptions, types, statuts,
attributs principaux, structures hiérarchiques et relations entre exigences.
Représenter les allocations PT lorsque le format le permet. Conserver les champs
inconnus comme attributs importés ou les signaler précisément. Ne jamais perdre
silencieusement une donnée.

Prévoir aperçu, mapping de catalogues, résolution des références, doublons et
rapport avant transaction. Préserver les identifiants externes pour permettre
des échanges successifs. Détecter références cassées, cycles et contenus riches
non pris en charge.

Critères d'acceptation : aller-retour sur jeux ReqIF de référence, relations et
hiérarchie conservées, rapport des écarts, rollback intégral, tests XML de
sécurité et absence de dépendance à un outil ReqIF externe.
