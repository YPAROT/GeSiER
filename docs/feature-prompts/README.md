# Feuille de route linéaire de GeSiER

Ces prompts découpent la mise à niveau du logiciel en lots indépendants mais
ordonnés. Ils doivent être traités dans l'ordre ci-dessous : un seul lot est
développé, testé et validé par l'utilisateur avant de commencer le suivant.

| Ordre | Lot | Dépend de |
|---:|---|---|
| 01 | Stabilisation du socle et retrait de l'interface historique | état actuel |
| 02 | Exigences et relations abouties | 01 |
| 03 | Documents structurés enrichis | 02 |
| 04 | DOCX, templates, drafts et publications GED | 03 |
| 05 | Applicabilité et matrice d'applicabilité | 02 |
| 06 | Vérification et matrice de vérification | 02, 05 |
| 07 | Interfaces et matrice N² complète | 01, 03 |
| 08 | Changements, waivers et dérogations | 02, 03, 05, 07 |
| 09 | Import/export CSV et XLSX généralisé | 03, 05, 06, 07, 08 |
| 10 | Interopérabilité ReqIF | 02, 09 |
| 11 | Dashboard et indicateurs de couverture finalisés | 03 à 10 |
| 12 | Historique, intégrité, sauvegardes et migrations | 01 à 11 |
| 13 | Manuel utilisateur intégré au menu Aide | interface stabilisée |
| 14 | Packaging, installation et livraison | tous les lots |

Chaque prompt est autonome : il rappelle les contraintes essentielles, impose
une migration non destructive et demande compilation et tests. Avant de lancer
un lot, compléter son prompt avec les décisions apparues depuis sa création.

Le fichier [`../prompt-manuel-utilisateur.md`](../prompt-manuel-utilisateur.md)
reste le carnet évolutif des contenus à intégrer au manuel.
