# GeSiER

GeSiER est un outil local et léger de traçabilité des exigences. Un projet est
stocké dans un fichier SQLite échangeable. L'application relie exigences,
product tree, documents, interfaces, vérification et changements.

## Construire avec Qt 6 et CMake

Prérequis : CMake 3.21 ou supérieur, Qt 6.5 ou supérieur avec les modules
Widgets, SQL et PrintSupport, et un compilateur C++17.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64
cmake --build build
ctest --test-dir build --output-on-failure
```

Le fichier `GeSiER.pro` est conservé provisoirement pour faciliter la transition
des anciens environnements Qt Creator. CMake est désormais le système de
construction de référence.

L'organisation des services, widgets, migrations et la règle de durée de vie
de `REQ_DB` sont décrites dans [docs/architecture.md](docs/architecture.md).

Au démarrage sans projet, l'interface reste utilisable pour créer ou ouvrir un
fichier, tandis que les actions et pages nécessitant SQLite sont désactivées.
La navigation principale ne présente que **Tableau de bord** et **Projet** ; les
anciens écrans SQL ne sont plus exposés.

## Fonctions de la refonte engagée

- migration automatique et sauvegardée des anciennes bases ;
- relations multiples typées entre exigences ;
- vues graphique et tabulaire de la traçabilité ;
- applicabilité, documents structurés, publications, interfaces typées et
  changements dans le schéma v2 ;
- tableau de bord des taux de couverture ;
- import/export CSV autonome, sans bibliothèque QtCSV externe.
- import/export ReqIF transactionnel avec aperçu, conservation des identifiants
  externes, hiérarchies, relations, allocations PT et attributs inconnus.
