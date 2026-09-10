# Prompt pour le futur manuel utilisateur de GeSiER

Rédiger et intégrer dans le manuel utilisateur accessible depuis le menu
**Aide** une section pédagogique consacrée aux relations entre exigences.

La section doit expliquer clairement les trois notions suivantes :

1. **Parent / enfant — Décompose**
   - Expliquer qu'une exigence enfant détaille une partie identifiable de
     l'exigence parent.
   - Exemple :
     - Parent `SYS-R-0010` : « Le système doit transmettre ses données
       scientifiques vers le calculateur de bord. »
     - Enfant `COM-R-0011` : « Le sous-système de communication doit transmettre
       les données scientifiques sur la liaison SpaceWire. »
   - Préciser que plusieurs enfants peuvent, ensemble, couvrir le contenu du
     parent et qu'une exigence peut avoir plusieurs parents dans GeSiER.

2. **Dérive de**
   - Expliquer qu'une exigence dérivée est créée comme conséquence d'une analyse,
     d'un choix d'architecture, d'une contrainte de sûreté ou d'une décision
     technique liée à une exigence amont, sans être une simple sous-partie de
     celle-ci.
   - Exemple :
     - Amont `SYS-R-0010` : « Le système doit transmettre ses données
       scientifiques vers le calculateur de bord. »
     - Dérivée `ELEC-R-0042` : « L'interface de communication doit assurer une
       isolation galvanique de 500 V. »
   - Expliquer que si l'exigence amont change ou disparaît, la justification de
     l'exigence dérivée doit être réexaminée.

3. **Dépend de**
   - Expliquer qu'une exigence dépend d'une autre lorsque sa réalisation ou sa
     vérification nécessite que l'autre exigence soit satisfaite, sans que la
     seconde soit nécessairement à l'origine de la première.
   - Exemple :
     - `SW-R-0025` : « Le logiciel doit transmettre un paquet de données en
       moins de 100 ms. »
     - dépend de `ELEC-R-0031` : « La liaison matérielle doit offrir un débit
       utile minimal de 10 Mbit/s. »
   - Expliquer qu'une modification de la liaison matérielle peut affecter la
     faisabilité ou la vérification de l'exigence logicielle.

Ajouter un tableau comparatif répondant à ces questions :

- l'exigence liée explique-t-elle pourquoi l'autre exigence existe ?
- détaille-t-elle une partie de son contenu ?
- conditionne-t-elle sa réalisation ou sa vérification ?
- que faut-il réexaminer lorsque l'exigence liée change ?

Terminer par ce moyen mnémotechnique :

- **Décompose** : « cette exigence détaille une partie de celle-ci » ;
- **Dérive de** : « cette exigence a été créée à cause de celle-ci » ;
- **Dépend de** : « cette exigence a besoin que celle-ci soit satisfaite ».

Le manuel doit également montrer comment créer, consulter et supprimer chaque
type de relation dans les vues graphique et simplifiée de GeSiER. Employer des
captures d'écran de la version finalisée de l'interface et rappeler que les
relations sont orientées, historisées, que les doublons sont refusés et que les
cycles de décomposition sont interdits.
