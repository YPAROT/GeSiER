# Importer une spécification Word

L'import Word utilise deux fichiers : le document de spécification rempli et
un gabarit DOCX de même structure. Dans le gabarit, un exemplaire du bloc
d'exigence est encadré par :

```text
{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}
… structure d'une exigence …
{{GESIER_REQUIREMENT_TEMPLATE_END}}
```

Les zones `{{GESIER_DOCUMENT_TITLE}}` et `{{GESIER_REFERENCE}}` servent aussi
à lire le titre et la référence du document source. Elles préremplissent les
champs d'une nouvelle spécification sans remplacer une saisie déjà effectuée.
À défaut, GeSiER consulte les propriétés Word `Title` et `Reference` ; aucune
référence n'est déduite du nom du fichier.

Les valeurs de l'exemple sont remplacées par les balises suivantes :

| Balise | Valeur |
|---|---|
| `{{REQ_CODE}}` | Code unique, obligatoire |
| `{{REQ_TITLE}}` | Titre, obligatoire |
| `{{REQ_DESCRIPTION}}` | Description, obligatoire |
| `{{REQ_TYPE}}` | Code ou libellé du type |
| `{{REQ_STATUS}}` | Code ou libellé du statut |
| `{{REQ_SOURCE}}` | Source |
| `{{REQ_PRODUCT_TREES}}` | Codes PT, un par ligne |
| `{{REQ_APPLICABILITY}}` | Codes de configuration, un par ligne |
| `{{REQ_VERIFICATIONS}}` | Une vérification par ligne |
| `{{REQ_RELATIONS}}` | Une relation par ligne |

Le champ `REQ_ID`, s'il est présent, est uniquement informatif. GeSiER utilise
le code comme clé fonctionnelle.

Une vérification agrégée suit la forme `méthode | niveau | procédure | moyen |
verdict | Redmine | commentaire`. Une relation suit la présentation produite
par l'export : `Dépend de CODE`, `Dérive de CODE` ou `Enfant de CODE`, avec un
commentaire facultatif entre parenthèses.

Les titres Word structurés par niveaux deviennent des chapitres GeSiER. Les
paragraphes, listes, caractères gras et italiques de la description sont
conservés. Les tableaux sont aplatis et les images ou objets incorporés sont
signalés mais non importés.

Les bornes `GESIER_VERIFICATION_TEMPLATE_BEGIN/END` et
`GESIER_RELATION_TEMPLATE_BEGIN/END` sont validées lorsqu'elles sont présentes.
Le format agrégé ci-dessus reste la représentation compatible avec les exports
DOCX GeSiER existants.

La liste exhaustive des balises communes, d'import et d'export à intégrer au
manuel utilisateur est maintenue dans
[`prompt-manuel-utilisateur.md`](prompt-manuel-utilisateur.md#référence-des-balises-word-dimport-et-dexport).

## Revue avant import

Tous les blocs reconnus sont affichés avant l'écriture. Un bloc conforme au
gabarit est sélectionné par défaut. Un bloc qui ressemble à une exigence mais
dont certains libellés ou certaines cellules diffèrent est présenté comme
« à corriger », avec l'emplacement de la divergence, et reste décoché.

Une exigence peut être corrigée dans l'aperçu : code, titre, description,
source, type, statut, chapitre, Product Trees, configurations, vérifications
et relations. Elle peut également être décochée, notamment lorsqu'il s'agit
d'une exigence d'exemple. GeSiER ne tente pas de reconnaître un exemple à
partir de son nom afin de ne pas exclure une vraie exigence.

Si une exigence sélectionnée reste invalide, l'utilisateur peut revenir la
corriger, ignorer les exigences concernées ou annuler tout l'import. Aucune
écriture n'a lieu avant cette décision. Une erreur technique pendant l'écriture
annule la transaction complète : exigences, chapitres, rattachements,
vérifications et relations ne sont pas conservés. Les corrections de l'aperçu
restent disponibles pour une nouvelle tentative.
