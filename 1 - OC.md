# ST4EVER - Document 1 - CONCEPTS OPERATIONNELS

## 0 - INTRODUCTION
Les concepts opérationnels sont le point de départ du développement du développement fonctionnel de l'application ST4Ever; ils sont décrits sous la forme de texte explicatif des comportements ST4Ever <-> Utilisateur, contiennent les informations fonctionnelles visibles utilisateur et représentent le niveau d'information d'un manuel utilisateur.

=> ***Phase développement***: Les Concepts Opérationnels sont déclinés en Use Cases (cf CLAUDE.md section 6 - Use Cases) permettant de cadrer le projet et le découpage du projet en blocs fonctionnels auto-porteurs codés / testés / validés entre Claude et Tonton (moi-même).

=> ***Phase documentation***: Les Concepts Opérationnels sont la source des UFR (User Fonctional Requirements du document 2 - SR.md)

## 1 - CONCEPTS OPERATIONNELS

***Contexte***: Ce projet est une application console interactive multi-plateforme développée en C pur à but éducatif permettant de:
- lire/écrire des fichiers ATARI ST (texte ou binaires .PRG, .TTP, .TOS)
- créer/lire/extraire des images disques ATARI ST de type .st/.msa à partir de/vers des répertoires PC
- visualiser, modifier, compiler, assembler, désassembler, décompiler les programmes ATARI ST et/ou les disques démos
- émuler les binaires ATARI ST via divers vues (CPU, Mémoire, écran) en pas à pas, temps ralenti ou temps réel 


## 1.1 - OC-ST4-001 : La console

***Command Line Interface (CLI)***: La console est l'élément central de l'application ST4Ever : l'utilisateur peut intéragir avec les différentes commandes et vues graphiques à partir de la ligne de commande de la console. Chaque commande est accessible par son nom complet, son alias monogramme et un raccourci CTRL, quand cela est possible. L'éditeur de ligne offre les fonctionnalités suivantes: 
- gestion de l'historique ↑↓: conservé dans un fichier et le retrouver entre les différentes sessions.
- fonction d'édition de la ligne de commande: Home/End, tab-completion fichiers/répertoires, pré-affichage en gris de la complétion.
- fonction de scripting de commandes par appel de l'application ST4Ever avec l'option `--script`
- un bandeau d'accueil affichant la version de l'application, son titre, et l'indication 'help' pour obtenir plus d'information sur les commandes disponibles

**Commandes console :**

| Commande | Alias | CTRL       | Comportement résumé |
|----------|-------|------------|---------------------|
| `help`    | `h`   | -     | Liste toutes les commandes avec synopsis ; ignore les arguments (warning) |
| `quit`    | `q`   | CTRL+Q<br>CTRL+C   | Ferme toutes les vues et quitte proprement ; ignore les arguments (warning)<br>CTRL+Q remplace le texte en cours d'édition une commande `quit`<br>CTRL+C vide la ligne de commande, puis 2ème CTRL+C sur ligne vide = `quit` |
| `trace`   | `t`   | CTRL+T      | Ouvre la vue des traces internes de l'application ST4Ever, les traces détaillées sont désactivées par défaut (cf section 1.2 - OC-ST4-002). <br>Sans argument, la commande `trace` / `CTRL+T`ouvre/ferme la vue. <br> `trace on` : ouvre + active les traces détaillées.<br> `trace off` : filtre les traces détaillées - affiche les infos & erreurs uniquement — la vue reste ouverte. <br>`trace clear` : vide le contenu de la fenêtre de traces. <br>`trace level trace\|info\|error` : filtre d'affichage GUI. <br>|
| `colors`  | `c`   | -           | Active/désactive les couleurs texte de la console. L'application détecte au démarrage si les couleurs peuvent être utilisées et les affiche par défaut.<br>Sans argument : alterne couleurs on/off. <br> `colors on` : active les codes ANSI. <br> `colors off` : désactive les codes ANSI, e.g.  terminal sans VT100 / pipe fichier).|
| `where`   | `w`   | CTRL+W    | Affiche le répertoire de travail courant et le fichier/répertoire de travail sélectionné via `dir`|
| `info`    | `n`   | —          | Donne un statut lié aux commandes (`trace`, `where`, `dir`, ...) |
| `history` | `y`   | —        | Liste les 10 dernières commandes, par défaut (sans argument). <br>`history N` : les N dernières commandes ou l'historique complet si N est supérieur au nombre de commandes dans l'historique |
| `dir`    | `d`   | CTRL+D          | Ouvre la vue arborescence du répertoire courant ou du répertoire donné en argument et permet la sélection d'un répertoire ou fichier de travail.<br>Sans argument: ouvre le répertoire courant et liste les répertoires et fichiers non cachés.<br>`dir <nom_de_répertoire>`: ouvre le répertoire donné en argument. <br>|
| `load`    | `l`   | CTRL+L          | Charge un fichier en mémoire émulée ATARI ST.<br>Sans argument: Charge le fichier sélectionné par la commande `dir`.<br>`load <file>` charge le fichier spécifié en argument.|
| `edit`    | `e`   | CTRL+E          | Ouvre une vue éditeur adaptée au type de fichier sélectionné.<br>Sans argument: Edite le fichier sélectionné par la commande `dir`.<br>`edit <file>` charge le fichier spécifié en argument.|
| `mount`    | `m`   | -          | Ouvre une vue arborescence disquette ATARI ST et émule la montée d'un répertoire ou d'une image .st/.msa.<br>Sans argument: émule le répertoire sélectionné par la commande `dir`.<br>`mount <file/dir>` charge le fichier image .st/.msa ou le répertoire spécifié en argument.|
| `image`  | `i`  | -      | Crée une image disk .st ou .msa à partir d'un fichier ou répertoire.<br>Sans Argument: crée l'image à partir de la sélection de la commande `dir`.<br>`image [--st\msa]`: spécifie le format de l'image créée. <br>`image --bootable`: Rend l'image disquette bootable sur ATARI ST.|
| `execute`  | `x`  | CTRL-X      | Execute un programme en mémoire émulée ATARI ST et ouvre les fenêtre de visualisation de l'exécution (video, CPU, mémoire, instructions, moniteur d'exécution).<br>Sans argument: exécute le programme ou l'image sélectionné par `dir`.<br>`execute <file>`: charge et exécute le contenu (e.g. .PRG ou .st/.msa)|
| `st2msa`  | -  | -      | Convertit une image .st en format .msa.<br>Sans Argument: convertit le fichier sélectionné par la commande `dir`; envoie un warning si le fichier actuellement sélectionné n'est pas une image '.st'.<br>`st2msa --dir [path] --all`: permet une conversion de tous les fichiers .st détectés dans \[path\].<br>Option `-r` active la récursion dans le répertoire indiqué par `--dir`|
| `msa2st`  | -  | -      | Convertit une image .msa en format .st.<br>Sans Argument: convertit le fichier sélectionné par la commande `dir`; envoie un warning si le fichier actuellement sélectionné n'est pas une image '.msa'.<br>`msa2st --dir [path] --all`: permet une conversion de tous les fichiers .msa détectés dans \[path\].<br>Option `-r` active la récursion dans le répertoire indiqué par `--dir`|
<br>

## 1.2 - OC-ST4-002 : La commande `help`
**Commande (`h` | `help`)**
Affiche la liste des commandes valides de la console principale de l'application ST4Ever, indique leur alias et, si applicable, le raccourci CTRL+<key> associé. Un synopsys en une ligne décrit sommairement l'objectif de chaque commande. La commande `help` ne prend pas d'argument et ignore les éventuels arguments placés en ligne de commande.
<br>

 ## 1.3 - OC-ST4-003 : La commande `quit`
**Commande (`q` | `quit` | `CTRL+Q` | `CTRL+C`)**
Permet de quitter l'application. <br>
CTRL+Q se comporte de manière identique. CTRL+C efface la ligne de commande en cours ou quitte l'application si la ligne de commande est vide.
<br>

## 1.4 - OC-ST4-004 : La commande `trace`
**Commande (`t` | `trace` | `CTRL+T`)**
Ouvre/ferme une fenêtre et affiche des traces internes de l'application en cours d'exécution. Plusieurs types de traces sont disponibles:
  - les erreurs levées par les fonctions implémentées dans l'application
  - les informations permettant le debug des fonctions en cours d'exécution
  - les traces d'exécution de chaque fonction de l'application permettant de visualiser l'arbre d'appel des fonctions liées à une commande - *sauf les fonctions de type 'helper' à non valeur ajoutée ou les fonctions exécutées en boucle rapides (e.g. renderer graphique (paint, ...))* 
  - les indications "TODO" liée à de futures évolutions de l'application

Par défaut, les traces d'exécution ne sont pas affichées dans la fenêtre, seules les informations, les erreurs et les TODO sont affichées. L'historique des traces n'est pas conservé, seules les traces produites depuis l'ouverture de la fenêtre sont affichées. Les options de la commande `trace` sont décrites dans le tableau récapitulatif de `OC-ST4-001`

L'ensemble des traces est intégralement enregistré dans un fichier `st4ever_trace.log`, remplacé à chaque exécution de l'application
  
La fenêtre de trace est ouverte au démarrage de l'application avec l'option `-t`.
  <br>
  
## 1.5 - OC-ST4-005 : La vue `dir`
**Commande (`d` | `dir` | `CTRL+D`)**
Cette commande ouvre une vue GUI de type tree view et affiche l'arborescence indentée du contenu du répertoire fourni en argument de la commande. La commande sans paramètre ouvre le répertoire courant. La vue GUI permet :
  - la sélection d'un fichier ou répertoire par clic gauche souris ou par touche 'espace', surlignée dans la vue GUI: la sélection devient l'argument par défaut des commandes de manipulation de fichiers/répertoires/images (e.g. `load`, `edit`, `image`, `mount`, `wf`).
  - l'expansion ou non d'un répertoire par clic gauche souris sur + devant le nom du répertoire
  - la rétractation d'un répertoire expansé par clic gauche souris sur - devant le nom du répertoire
  - la remontée de l'arborescence du répertoire parent via une première ligne '..' en haut de l'arborescence
  - l'affichage d'un menu contextuel par clic droit souris sur un fichier ou répertoire:
       - clic droit sur fichier affiche les commandes (`load`, `edit`)
       - clic droit sur répertoire affiche les commandes (`mount`, `image`)

  <br>
  
## 1.6 - OC-ST4-006 : La commande `load`
**Commande (`l` | `load` | `CTRL+L`)**
Cette commande prend en argument un nom de fichier en argument ou le fichier sélectionné via la commande `dir` et le charge en mémoire de l'émulateur ATARI ST. Si aucun fichier n'est sélectionné et aucun paramètre donné, `load` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `load` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un fichier : charge ce fichier dans la mémoire émulée de l'ATARI ST à un emplacement libre. Pour une image, un fichier texte, la copie en mémoire est conforme au contenu du fichier. Pour un fichier binaire au format ATARI ST (.PRG, .TTP, .TOS), la montée en mémoire se fait selon les conventions du TOS ATARI ST, en mettant à jour les fixups d'addresse du programme. Ce programme binaire pourra être directement exécuté depuis la mémoire virtuelle ATARI ST par l'émulateur CPU 68000 et/ou par l'émulateur machine ATARI ST.
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les répertoires
   - si l'argument fourni ou par défaut est une image disque ATARI ST .st ou .msa: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les images disques.

## 1.7 OC-ST4-007: La commande `edit`
**Commande (`e` | `edit` | `CTRL+E`)**
Cette commande prend en argument un nom de fichier (+ path) en entrée ou le fichier sélectionné via la commande `dir`; si aucun fichier n'est sélectionné et aucun paramètre donné, `edit` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `edit` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande n'a pas d'effet sur les répertoires
   - si l'argument fourni ou par défaut est un fichier : ouvre une vue d'édition en fonction de la typologie du fichier : 
       - éditeur de texte avec scroll bar et numéro de ligne pour les fichiers texte, 
       - éditeur héxadécimal tabulaire pour les binaires, images, avec scroll bar et addresse en chaque début de ligne et adresse en colonne, avec une vue additionnelle texte ASCII, ainsi qu'une vue texte assembleur 68000 au format DEVPAC3 ATARI ST lorsque la partie hexadécimale correspond à du code exécutable de type .text ou bootsector d'une image disque ou lorsque l'utilisateur sélectionne une plage d'adresse du fichier binaire.
   - Le fichier est éditable et les modifications peuvent être sauvegardées dans les deux formats, avec confirmation utilisateur pour la sauvegarde:
       - l'édition texte des fichiers texte se fait de manière classique via l'éditeur de texte. Si le fichier texte est un fichier assembleur 68000 au format DEVPAC3, un clic droit souris peut permettre la génération d'un binaire ATARI ST .PRG ou .raw (pour un binaire brut) dans le même répertoire que le fichier texte.
       - l'édition hexadécimale est possible par :
           - insertion/remplacement des valeurs hexadecimales aux adresses,
           - la modification ASCII par la vue ASCII change les octets hexadecimaux,
           - la modification de code assembleur 68000 en ligne par recompilation de la section concernée; les impacts binaires sont à déterminer par l'outil d'assemblage et demande de confirmation par utilisateur. La vue assembleur 68000 est cliquable bouton droit pour sauvegarder le fichier texte correspondant au source assembleur.

## 1.8 OC-ST4-008: La commande `mount` 
**Commande (`m` | `mount` | `CTRL+M`)**
Cette commande prend un chemin de répertoire en entrée ou le répertoire sélectionné via la commande `dir`; si aucun chemin n'est sélectionné et aucun paramètre donné, le chemin courant de l'application est utilisé, mais une demande de confirmation utilisateur est nécessaire. `mount` se comporte de la manière suivante:
   - émule la montée d'un répertoire dans un disque A:\ ATARI ST en affichant une vue arborescence disquette. 
   - permet l'ajout de fichiers dans l'émulation disquette par clic+glissement souris dans la vue à partir de la vue ouverte par la commande `dir`
   - permet la suppression de fichiers dans l'émulation disquette par clic+glissement souris hors vue
   - la vue complète permet la création d'une image disque à partir d'un clic droit sur la première ligne de l'arborescence nommée "A:\", via la commande `image` en popup. La vue intègre également un bandeau vertical de propriétés de la disquette montée : ces propriétés sont celles du header disk ATARI ST.

## 1.9 OC-ST4-009: La commande `image` 
**Commande (`i` | `image`)**
Cette commande crée le fichier image disquette ATARI ST avec le contenu de la vue `mount`. Si la vue `mount` n'est pas ouverte, la commande `image` crée une image .st ou .msa à partir du fichier/répertoire donné en argument, ou du répertoire sélectionné par `dir`:
  - La création d'une image se fait par défaut avec le contenu de la vue `mount` si celle-ci est ouverte, tout changement dans la vue `mount` sera reporté dans l'image lors du lancement de la commande.
  - La création d'une image peut se faire à partir d'un répertoire local; un message utilisateur indique si la taille des fichiers dépasse celle de l'image créée, l'image n'est pas créée en cas de dépassement.
  - La fonctionnalité de création d'image permet la conversion d'une image .st en .msa et vice-versa si l'option `--st | --msa` est fournie et l'autre type de fichier donné en argument
  - Une image peut être générée bootable avec une option `--bootable`

## 1.10 - OC-ST4-010 : La commande `where`
**Commande (`w` | `where` | `CTRL+W`)**
Affiche le répertoire courant et le répertoire/fichier sélectionné par la commande `dir`. Le fichier/répertoire sélectionné est celui qui est utilisé par les commandes `edit`, `load`, `mount`, `image`, `execute` lorsqu'elles sont utilisées sans argument.
<br>

## 1.11 - OC-ST4-011: La commande `info`
**Commande (`n` | `info`)**
Affiche les informations de la commande `where`, ainsi que le statut de la console de trace, la configuration couleur de la console, le statut de l'historique des commandes, l'identification du disque monté.
<br>

## 1.12 - OC-ST4-012: La commande `history`
**Commande (`y` | `history`)**
Affiche les 10 dernières commandes de l'historique, ou les [N] dernières commandes lorsque la valeur est donnée en argument de la commande.
<br>


## 1.13 - OC-ST4-013: La commande `execute`
**command (`x` \| `execute` \| `CTRL+X`)**
Cette commande prend en argument le fichier binaire exécutable ATARI ST (.PRG, .TTP ou .TOS) ou celui sélectionné dans la vue `mount` de l'émulation disquette ou dans la vue de la commande `dir`. L'argument peut également représenter une image .st\.msa et être exécuté au reset de l'émulateur ATARI ST. `execute` comprend les vues suivantes:
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP) visualisant les instructions en cours d'exécution
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires, avec possibilité de plusieurs vues par plages d'adresse mémoire (plage interruptions, mémoire système, RAM, Vidéo, TOS)
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

La commande `execute` privilégie l'exécution pas à pas avec l'ensemble des vues interactives entre elles, cependant l'exécution en temps ralenti, réel ou accéléré reste possible en tâche de fond avec mise en place de breakpoint pour points de visibilité. La vitesse d'exécution des binaires et la synchronisation complète des différentes vues lors de l'exécution sera limitée par la performance CPU/Video de la machine exécutant l'application ST4Ever; le moniteur d'exécution donne les informations d'exécution et de performance en instructions émulées par secondes.

## 1.14 - OC-ST4-014: La commande 'st2msa'
**Commande (`st2msa`)**
Cette commande convertit une image .st en format .msa. Elle prend en argument un fichier ou un répertoire (avec option `--dir [path]`) ou convertit le fichier sélectionné par la commande `dir` si aucun argument n'est donné. Un warning est envoyé à l'utilisateur si le fichier actuellement sélectionné n'est pas une image '.st'.<br>`st2msa --dir [path] --all`: permet une conversion de tous les fichiers .st détectés dans \[path\].<br>Option `-r` active la récursion dans le répertoire indiqué par `--dir`
<br>

  ## 1.15 - OC-ST4-015: La commande 'msa2st'
**Commande (`msa2st`)**
Cette commande convertit une image .msa en format .st. Elle prend en argument un fichier ou un répertoire (avec option `--dir [path]`) ou convertit le fichier sélectionné par la commande `dir` si aucun argument n'est donné. Un warning est envoyé à l'utilisateur si le fichier actuellement sélectionné n'est pas une image '.msa'.<br>`msa2st --dir [path] --all`: permet une conversion de tous les fichiers .msa détectés dans \[path\].<br>Option `-r` active la récursion dans le répertoire indiqué par `--dir`
<br>
  