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

***Command Line Interface (CLI)***: Chaque commande est accessible par son nom complet, son alias monogramme et un raccourci CTRL. L'éditeur de ligne (UC4) offrira : historique ↑↓, Home/End, tab-completion fichiers/répertoires, pré-affichage en gris de la complétion.

**Commandes validées :**

| Commande | Alias | CTRL       | UC      | Comportement résumé |
|----------|-------|------------|---------|---------------------|
| `help`    | `h`   | CTRL+H     | ✓ UC1   | Liste toutes les commandes avec synopsis ; ignore les arguments (warning) |
| `quit`    | `q`   | CTRL+Q / C | ✓ UC1   | Ferme toutes les vues et quitte proprement ; ignore les arguments (warning) |
| `trace`   | `t`   | CTRL+T     | ✓ UC2   | Sans arg : toggle open/close. **Ouverture : LOG_TRACE off par défaut** (P26). `on` : ouvre + active LOG_TRACE. `off` : filtre LOG_TRACE uniquement — **la vue reste ouverte** (P19). `clear` : vide le ring buffer GUI. `level trace\|info\|error` : filtre d'affichage GUI. Détails §6.2, §6.9, §6.10 |
| `colors`  | `c`   | —          | ✓ UC4.3 | Sans arg : toggle on/off. `on` : active les codes ANSI. `off` : désactive (utile terminal sans VT100 ou redirection fichier). Auto-détecté via `isatty()` au démarrage (P24). Détails §6.8 |
| `where`   | `w`   | CTRL+W     | ✓ UC5   | Affiche le répertoire de travail courant et le fichier sélectionné via `dir`. Détails §6.10 |
| `info`    | `n`   | —          | ✓ UC5   | Dashboard complet : cwd, sélection, état trace (ouvert/LOG_TRACE), colors, historique, disque monté (stub), binaire chargé (stub). Détails §6.10 |
| `history` | `y`   | —          | ✓ UC5   | Sans arg : 10 dernières commandes numérotées. Avec `N` : les N dernières. Détails §6.10 |

**Commandes à implémenter — spécification détaillée dans les sous-sections ci-dessous :**

#### 1.1.3 (`d` | `dir` | `CTRL+D`) 
Cette commande ouvre une vue GUI de type tree view et affiche l'arborescence indentée du contenu du répertoire fourni en argument de la commande. La commande sans paramètre ouvre le répertoire courant. La vue GUI permet :
  - la sélection d'un fichier ou répertoire par clic gauche souris ou par touche 'espace' est surlignée dans la vue GUI et la sélection devient l'argument par défaut des commandes `load`, `edit`, `image`, `mount`, `wf`.
  - l'expansion ou non d'un répertoire par clic gauche souris sur + devant le nom du répertoire
  - la rétractation d'un répertoire expansé par clic gauche souris sur - devant le nom du répertoire
  - la remontée de l'arborescence du répertoire parent via une première ligne '..' en haut de l'arborescence
  - l'affichage d'un menu contextuel par clic droit souris sur un fichier ou répertoire:
       - clic droit sur fichier affiche les commandes (`load`, `edit`)
       - clic droit sur répertoire affiche les commandes (`mount`, `image`)

#### 1.1.4 (`l` | `load` | `CTRL+L`) 
Cette commande prend en argument un nom de fichier (+ path) en entrée ou le fichier sélectionné via la commande `dir`; si aucun fichier n'est sélectionné et aucun paramètre donné, `load` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `load` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un fichier : charge ce fichier dans la mémoire émulée de l'ATARI ST à un emplacement libre. Pour une image, un fichier texte, la copie en mémoire est conforme au contenu du fichier. Pour un fichier binaire au format ATARI ST (.PRG, .TTP, .TOS), la montée en mémoire se fait selon les conventions du TOS ATARI ST, en mettant à jour les fixups d'addresse du programme. Ce programme binaire pourra être directement exécuté depuis la mémoire virtuelle ATARI ST par l'émulateur CPU 68000 et/ou par l'émulateur machine ATARI ST.
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les répertoires
   - si l'argument fourni ou par défaut est une image disque ATARI ST .st, .msa, .stx: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les images disques.

#### 1.1.5 (`e` | `edit` | `CTRL+E`) 
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

#### 1.1.6 (`m` | `mount` | `CTRL+M`) 
Cette commande prend un chemin de répertoire en entrée ou le répertoire sélectionné via la commande `dir`; si aucun chemin n'est sélectionné et aucun paramètre donné, le chemin courant de l'application est utilisé, mais une demande de confirmation utilisateur est nécessaire. `mount` se comporte de la manière suivante:
   - émule la montée d'un répertoire dans un disque A:\ ATARI ST en affichant une vue arborescence disquette. 
   - permet l'ajout de fichiers dans l'émulation disquette par clic+glissement souris dans la vue à partir de la vue ouverte par la commande `dir`
   - permet la suppression de fichiers dans l'émulation disquette par clic+glissement souris hors vue
   - la vue complète permet la création d'une image disque à partir d'un clic droit sur la première ligne de l'arborescence nommée "A:\", via la commande `image` en popup. La vue intègre également un bandeau vertical de propriétés de la disquette montée : ces propriétés sont celles du header disk ATARI ST.

#### 1.1.7 (`x` | `execute` | `CTRL+X`)
Cette commande prend en argument le fichier binaire exécutable ATARI ST (.PRG, .TTP ou .TOS) ou celui sélectionné dans la vue `mount` de l'émulation disquette ou dans la vue de la commande `dir`. `execute` comprend les vues suivantes:
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP) visualisant les instructions en cours d'exécution
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires, avec possibilité de plusieurs vues par plages d'adresse mémoire (plage interruptions, mémoire système, RAM, Vidéo, TOS)
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

La commande `execute` privilégie l'exécution pas à pas avec l'ensemble des vues interactives entre elles, cependant l'exécution en temps ralenti, réel ou accéléré reste possible en tâche de fond avec mise en place de breakpoint pour points de visibilité. La vitesse d'exécution des binaires et la synchronisation complète des différentes vues lors de l'exécution sera limitée par la performance CPU/Video de la machine exécutant l'application ST4Ever; le moniteur d'exécution donne les informations d'exécution et de performance en instructions émulées par secondes.

#### 1.1.8 (`u` | `umount` | `CTRL+U`)
Cette commande permet de clore la vue ouverte par la commande `mount` tout en démontant l'émulation disquette A:\ ATARI ST. `umount` simule le retrait de la disquette du lecteur ATARI ST. Si le contenu disquette émulé ne correspond à aucune image .st, .stx ou .msa existante, demande à l'utilisateur s'il souhaite créer une nouvelle image de la disquette en cours de retrait pour ne pas perdre les modifications réalisées lors de la commande `mount`.

#### 1.1.9 (`w` | `where` | `CTRL+W`)
Cette commande permet d'afficher le répertoire de travail ou le fichier sélectionné courant via la commande `dir` ou le répertoire par défaut dans lequel l'application est lancée. La réponse à la commande est fournie par texte console.

#### 1.1.10 (`t` | `trace` | `CTRL+T`)
Validée en UC2 — voir table §1.1 et contrats comportementaux §6.2. La fenêtre trace GUI sera ouverte via `gui_open_window(GUI_WND_TRACE)` en UC3.3 (actuellement : sortie stderr ANSI).

