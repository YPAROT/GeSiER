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
verdict | Redmine | commentaire`. Une relation agrégée accepte soit les codes
`DEPENDS_ON`, `DERIVES_FROM`, `DECOMPOSE`, soit les libellés `Dépend de`,
`Dérive de`, `Enfant de`, avec un commentaire facultatif entre parenthèses.

Les titres Word structurés par niveaux deviennent des chapitres GeSiER. Les
paragraphes, listes, caractères gras et italiques de la description sont
conservés. Les tableaux sont aplatis et les images ou objets incorporés sont
signalés mais non importés.

## Collections structurées

Les collections peuvent être décrites par un prototype répété. Les paragraphes
ou lignes portant les bornes ne sont pas exportés ; le contenu entre les bornes
est dupliqué dans l'ordre des données, ou supprimé lorsque la collection est
vide.

| Sous-bloc | Champs disponibles | Champs obligatoires |
|---|---|---|
| `{{GESIER_VERIFICATION_TEMPLATE_BEGIN}}` … `{{GESIER_VERIFICATION_TEMPLATE_END}}` | `{{VERIF_METHOD}}`, `{{VERIF_LEVEL}}`, `{{VERIF_PROCEDURE}}`, `{{VERIF_REDMINE}}`, `{{VERIF_MEANS}}`, `{{VERIF_VERDICT}}`, `{{VERIF_COMMENT}}` | `VERIF_METHOD` |
| `{{GESIER_RELATION_TEMPLATE_BEGIN}}` … `{{GESIER_RELATION_TEMPLATE_END}}` | `{{RELATION_TYPE}}`, `{{RELATION_CODE}}`, `{{RELATION_TITLE}}`, `{{RELATION_COMMENT}}` | `RELATION_TYPE`, `RELATION_CODE` |

Si un sous-bloc structuré et sa balise agrégée sont tous deux présents, le
sous-bloc est prioritaire à l'import afin d'éviter les doublons. Les balises
`REQ_VERIFICATIONS` et `REQ_RELATIONS` restent prises en charge pour les anciens
gabarits.

`VERIF_LEVEL` contient de préférence le code PT hiérarchique complet, par
exemple `SYS-PWR-CTRL`. L'import tente d'abord ce code complet, puis un segment
ou un nom pour les anciens documents. Un texte qui ne correspond à aucun PT est
conservé comme niveau historique. À l'inverse, `REQ_APPLICABILITY` contient
exclusivement des codes de configuration tels que `FM` ou `QM`.

Le sens des relations est exprimé depuis l'exigence importée :

- `DECOMPOSE` / `Enfant de` désigne son parent ;
- `DERIVES_FROM` / `Dérive de` désigne l'exigence à l'origine de sa création ;
- `DEPENDS_ON` / `Dépend de` désigne l'exigence dont elle dépend.

`RELATION_DIRECTION` n'est plus produit. Il reste accepté, lorsqu'il vaut `IN`
ou `OUT`, dans les anciens gabarits d'import.

Exemple de prototype :

```text
{{GESIER_VERIFICATION_TEMPLATE_BEGIN}}
Méthode: {{VERIF_METHOD}} | Niveau: {{VERIF_LEVEL}}
Procédure: {{VERIF_PROCEDURE}} | Verdict: {{VERIF_VERDICT}}
{{GESIER_VERIFICATION_TEMPLATE_END}}

{{GESIER_RELATION_TEMPLATE_BEGIN}}
{{RELATION_TYPE}} {{RELATION_CODE}} — {{RELATION_TITLE}}
Commentaire: {{RELATION_COMMENT}}
{{GESIER_RELATION_TEMPLATE_END}}
```

À l'export, **Présentation des relations** propose **Libellés traduits** (choix
par défaut) ou **Codes techniques**. Le choix s'applique aux sous-blocs et à
`REQ_RELATIONS`; il est mémorisé. Le menu général **Langue** propose français
et anglais et pilote les libellés DOCX (`Enfant de`/`Child of`, `Dérive
de`/`Derives from`, `Dépend de`/`Depends on`). Cette préférence globale est
persistée et servira de base au futur chantier de traduction de l'interface.

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
