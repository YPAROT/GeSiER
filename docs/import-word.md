# Importer une spécification Word

L'import Word utilise deux fichiers : le document de spécification rempli et
un gabarit DOCX de même structure. Dans le gabarit, un exemplaire du bloc
d'exigence est encadré par :

```text
{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}
… structure d'une exigence …
{{GESIER_REQUIREMENT_TEMPLATE_END}}
```

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
