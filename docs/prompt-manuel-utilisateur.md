# Prompt pour le futur manuel utilisateur de GeSiER

## Référence des balises Word d'import et d'export

Ajouter au manuel une section de référence exhaustive consacrée aux gabarits
Word. Expliquer qu'une balise peut être répartie entre plusieurs segments de
mise en forme Word, mais que son texte visible doit rester strictement identique,
accolades comprises. Les noms de balises ne sont pas sensibles à la casse ; il
est néanmoins recommandé de toujours utiliser les formes ci-dessous.

### Structure générale d'un gabarit d'export

| Balise | Rôle | Règle |
|---|---|---|
| `{{GESIER_CONTENT}}` | Emplacement du contenu structuré du document | Obligatoire, exactement une fois |
| `{{GESIER_TOC}}` | Emplacement d'une table des matières Word | Facultative si le document contient déjà un champ TOC |
| `{{GESIER_REFERENCE}}` | Référence principale, puis références secondaires | Facultative ; utilisable dans le corps, l'en-tête ou le pied de page |
| `{{GESIER_METADATA}}` | Métadonnées du document, une par ligne | Facultative ; utilisable dans le corps, l'en-tête ou le pied de page |
| `{{GESIER_METADATA:Nom de la clé}}` | Valeur d'une métadonnée précise | Facultative ; par exemple `{{GESIER_METADATA:Indice}}` dans une cellule de cartouche |
| `{{GESIER_DOCUMENT_TITLE}}` | Titre du draft ou de la publication | Facultative ; les champs Word `DOCPROPERTY Title` sont également actualisés |
| `{{GESIER_CHAPTER_LEVEL_1}}` à `{{GESIER_CHAPTER_LEVEL_6}}` | Paragraphes prototypes des différents niveaux de chapitre | Au moins un prototype ; ajouter tous les niveaux nécessaires à la structure exportée |
| `{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}` | Début du prototype d'exigence | Obligatoire, exactement une fois |
| `{{GESIER_REQUIREMENT_TEMPLATE_END}}` | Fin du prototype d'exigence | Obligatoire, exactement une fois et après la balise de début |

Pour construire un cartouche, placer chaque métadonnée à l'endroit voulu, par
exemple `Indice : {{GESIER_METADATA:Indice}}` et
`Auteur : {{GESIER_METADATA:Auteur}}`. La recherche ignore la casse et les
espaces autour du nom de clé. Une clé absente ou ambiguë produit un avertissement
et une valeur vide ; la balise globale `{{GESIER_METADATA}}` conserve la liste
complète au format `Clé : Valeur`.

Préciser que les paragraphes portant les balises de début et de fin ne font pas
partie du rendu. Tout contenu situé entre ces deux bornes est dupliqué pour
chaque exigence.

### Champs disponibles dans un bloc d'exigence

| Balise | Valeur exportée ou attendue à l'import |
|---|---|
| `{{REQ_ID}}` | Identifiant interne à l'export ; informatif et ignoré à l'import |
| `{{REQ_CODE}}` | Code fonctionnel unique de l'exigence |
| `{{REQ_TITLE}}` | Titre de l'exigence |
| `{{REQ_DESCRIPTION}}` | Description |
| `{{REQ_TYPE}}` | Code ou libellé du type |
| `{{REQ_STATUS}}` | Code, raccourci ou libellé du statut |
| `{{REQ_SOURCE}}` | Source de l'exigence |
| `{{REQ_PRODUCT_TREES}}` | Allocations au Product Tree, un code par ligne |
| `{{REQ_APPLICABILITY}}` | Configurations applicables, un code par ligne |
| `{{REQ_VERIFICATIONS}}` | Vérifications, une par ligne |
| `{{REQ_RELATIONS}}` | Relations prises en charge, une par ligne |

À l'import, `REQ_CODE`, `REQ_TITLE` et `REQ_DESCRIPTION` sont obligatoires dans
le gabarit. Le code sert de clé pour détecter les exigences déjà présentes.
Une balise décrite par le gabarit mais vide dans le document source efface la
valeur correspondante lors d'une mise à jour confirmée.

Documenter la syntaxe compacte compatible avec les exports GeSiER :

- une vérification suit `méthode | niveau | procédure | moyen | verdict |
  Redmine | commentaire` ;
- une relation suit `Dépend de CODE`, `Dérive de CODE` ou `Enfant de CODE` ;
- le titre de l'exigence liée peut suivre le code après `—` ;
- un commentaire de relation peut être ajouté entre parenthèses.

### Sous-blocs répétables d'import

Présenter également les balises réservées à la description explicite des
listes répétables :

| Sous-bloc | Balises de champ |
|---|---|
| `{{GESIER_VERIFICATION_TEMPLATE_BEGIN}}` … `{{GESIER_VERIFICATION_TEMPLATE_END}}` | `{{VERIF_METHOD}}`, `{{VERIF_LEVEL}}`, `{{VERIF_PROCEDURE}}`, `{{VERIF_REDMINE}}`, `{{VERIF_MEANS}}`, `{{VERIF_VERDICT}}`, `{{VERIF_COMMENT}}` |
| `{{GESIER_RELATION_TEMPLATE_BEGIN}}` … `{{GESIER_RELATION_TEMPLATE_END}}` | `{{RELATION_TYPE}}`, `{{RELATION_DIRECTION}}`, `{{RELATION_CODE}}`, `{{RELATION_TITLE}}`, `{{RELATION_COMMENT}}` |

Indiquer que ces sous-blocs doivent rester à l'intérieur du bloc d'exigence,
que leurs bornes doivent être appariées et dans le bon ordre, et que le gabarit
doit conserver la même structure de paragraphes ou de tableau que le document
Word à lire.

### Procédure utilisateur et limites

Décrire la procédure complète : ouvrir **Exigences**, choisir **Importer une
spécification…**, sélectionner le DOCX source et son gabarit, valider le
gabarit, choisir le Product Tree principal et le document cible, contrôler
l'aperçu, résoudre les correspondances et les doublons, puis confirmer
l'import.

Rappeler les règles de conversion : les titres Word deviennent des chapitres ;
les paragraphes, listes, caractères gras et italiques des descriptions sont
conservés ; les tableaux sont aplatis en texte ; les images, objets incorporés,
commentaires Word, notes et révisions suivies ne sont pas importés. Ajouter au
manuel au moins un exemple complet de gabarit en tableau et un exemple de
document source correspondant.

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

## Configurations et applicabilité

Ajouter au manuel une section pédagogique intitulée **Configurations et
applicabilité des exigences**.

### À quoi sert une configuration ?

Une configuration représente une version physique, fonctionnelle ou
opérationnelle du système pour laquelle les exigences peuvent différer. Elle ne
représente pas un élément du Product Tree : le Product Tree décrit les parties
du produit, tandis qu'une configuration décrit une déclinaison ou un contexte
d'utilisation du produit.

Exemples de configurations :

| Code | Libellé | Description possible |
|---|---|---|
| `EM` | Modèle EM | Modèle d'ingénierie utilisé pour l'intégration et les essais fonctionnels. |
| `FLATSAT` | FlatSat | Banc à plat représentatif du système, utilisé pour les essais d'interfaces et de bout en bout. |
| `FM` | Modèle de vol | Équipement destiné au vol ; les exigences de qualification ou propres aux bancs peuvent ne pas lui être applicables. |

Préciser que le **code** est un identifiant court et unique, tandis que le
**libellé** est le nom lisible. La **description** doit expliquer ce que la
configuration représente et, si possible, son usage ou ses différences avec
les autres configurations. Le champ **ordre** détermine l'ordre d'affichage.

### Créer une configuration

1. Ouvrir **Projet > Applicabilité > Configurations**.
2. Cliquer sur **Nouvelle**.
3. Renseigner le code unique, le libellé, la description et l'ordre.
4. Laisser l'état **Active** coché tant que la configuration est utilisée pour
   les nouvelles affectations.
5. Cliquer sur **Enregistrer**.

Exemple conseillé : saisir le code `EM`, le libellé `Modèle EM`, puis une
description telle que « Modèle d'ingénierie utilisé pour l'intégration et les
essais fonctionnels ». Créer séparément `FLATSAT`, avec le libellé `FlatSat` et
une description du banc concerné. Une configuration doit représenter un
contexte cohérent : il ne faut donc pas regrouper « EM / FlatSat » sous une
seule configuration si certaines exigences s'appliquent à l'un mais pas à
l'autre.

### Affecter des configurations à une exigence

Pour une affectation simple :

1. Ouvrir la fiche de l'exigence.
2. Sélectionner l'onglet **Applicabilité**.
3. Cocher toutes les configurations auxquelles l'exigence s'applique.
4. Cliquer sur **Enregistrer** pour valider la fiche, ou sur **Annuler** pour
   abandonner les modifications.

Une case cochée signifie que l'exigence est applicable à la configuration. Une
case non cochée signifie qu'aucune applicabilité simple n'est définie pour
cette configuration.

### Utiliser la matrice d'applicabilité

Ouvrir **Projet > Applicabilité > Matrice exigences × configurations**. Chaque
ligne représente une exigence et chaque colonne de configuration indique son
applicabilité : `✓` signifie applicable et `—` non applicable ou non renseigné.
Les filtres de branche Product Tree, document, statut et type sont combinables.

La matrice est une vue de consultation et de contrôle. Pour modifier
l'applicabilité, double-cliquer sur une ligne ou une cellule : GeSiER ouvre la
fiche de l'exigence correspondante. Dans l'onglet **Applicabilité**, cocher ou
décocher les configurations, puis cliquer sur **Enregistrer**. Le bouton
**Annuler** permet de revenir à la valeur enregistrée.

Les éventuelles exceptions propres à un couple **branche PT / configuration**
restent visibles dans la matrice par une mise en évidence de la cellule, mais ne
sont pas modifiables directement depuis cette vue de synthèse.

Le taux affiché correspond au nombre de cellules applicables divisé par le
nombre total de couples exigences/configurations présents après filtrage. Il
est donc recalculé à chaque modification des filtres. Il est cliquable pour
revenir directement au détail de la matrice.

### Archiver sans perdre les affectations

Une configuration qui n'est plus proposée pour de nouveaux travaux doit être
**archivée**, et non supprimée. Cette opération se réalise uniquement dans
**Projet > Applicabilité > Configurations** : sélectionner la configuration,
puis cliquer sur **Archiver / restaurer**. Une configuration archivée reste
visible dans la fiche des exigences qui l'utilisent et dans la matrice ; ses
associations, son ordre et son historique sont conservés. La même commande
permet de la restaurer. La fiche Exigence sert uniquement à cocher ou décocher
les configurations existantes ; elle ne permet ni de les créer, ni de les
modifier, ni de les archiver. Une configuration archivée déjà associée reste
visible et cochée, mais sa case est grisée et verrouillée. Une configuration
archivée non associée n'est pas proposée. Il faut restaurer la configuration
avant de pouvoir modifier ses associations.

Dans la matrice, les configurations archivées sont masquées par défaut. Cocher
**Afficher les configurations archivées** pour les inclure ; leurs colonnes
sont alors grisées et identifiées par la mention `[archivée]`. Ce choix est
également appliqué à l'export XLSX.

### Exporter et contrôler

Depuis la matrice, cliquer sur **Exporter XLSX…**. Le classeur contient :

- une feuille **Synthèse** avec les volumes et le taux d'applicabilité ;
- une feuille **Détails** avec la matrice filtrée ;
- une feuille **Anomalies** signalant notamment les exigences qui ne sont
  applicables à aucune configuration.

Conseiller à l'utilisateur de vérifier les anomalies et les exigences sans
configuration avant une revue ou une livraison documentaire.

## Tableau de bord et couverture moyenne

Le tableau de bord présente séparément les indicateurs détaillés de couverture
des exigences. La carte **Couverture moyenne** est une synthèse de cinq taux :

1. allocation au Product Tree ;
2. traçabilité amont ;
3. couverture documentaire ;
4. planification de la vérification ;
5. applicabilité définie.

Chaque indicateur ayant le même poids, la valeur affichée est leur moyenne
arithmétique. Un indicateur dont le dénominateur est nul est exclu du calcul.

Exemple :

```text
Allocation PT       : 91 %
Traçabilité amont   : 84 %
Documentation       : 78 %
Vérification        : 73 %
Applicabilité       : 86 %

Couverture moyenne = (91 + 84 + 78 + 73 + 86) / 5
                   = 82,4 %, affiché 82 %
```

Les taux individuels sont arrondis à l'entier avant le calcul de la moyenne,
puis le résultat est affiché sous forme d'un pourcentage entier. La couverture
ICD des interfaces et la complétude des changements ne participent pas à cette
moyenne. Le dénominateur de la traçabilité peut également différer de celui des
autres indicateurs, car les exigences déclarées comme racines de traçabilité en
sont exclues.

La couverture moyenne doit être utilisée comme un repère de synthèse. Pour
identifier les actions à mener, consulter les cinq anneaux détaillés et cliquer
sur l'indicateur concerné afin d'afficher les exigences manquantes.
