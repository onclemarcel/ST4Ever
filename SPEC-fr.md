# Projet : ST4Ever : The Revival Engine for the Timeless ATARI ST

Ce projet est une application console interactive multi-plateforme développée en C pur à but éducatif permettant de:
- lire/écrire des fichiers ATARI ST (texte ou binaires .PRG, .TTP, .TOS)
- créer/lire des images disques ATARI ST de type .st, .msa, .stx à partir de/vers des répertoires PC
- modifier, compiler, assembler, désassembler, décompiler les programmes ATARI ST
- émuler les binaires ATARI ST via divers vues (CPU, Mémoire, écran) en pas à pas, temps ralenti ou temps réel en fonction des commandes utilisateurs sur les vues et/ou la console interactive
- les vues comprennent : 
    - un éditeur de fichier texte et/ou fichier source d'un binaire (e.g. assembleur d'un .PRG)
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP)
    - une vue assembleur 68000 au format DevPac3 ATARI ST pour les sections binaires pertinentes de la vue hexadécimale (e.g. .text ou toute plage d'adresse fournie par utilisateur)
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires
    - un visualisateur d'arborescence fichiers/répertoires pour les sélections/gestions de fichiers et des images disques .st, .msa, .stx
    - une console de trace/logs/erreurs pour l'application et debuggage développeur de ST4Ever

 A noter que l'émulation ATARI ST de ST4Ever est totalement développée from scratch dans un but éducatif : Hatari et STeem sont de très bons émulateurs complet et performants. L'émulation ATARI ST ST4Ever est simplifiée de manière à satisfaire les besoins d'exécution pas-à-pas des fonctions citées ci-dessus.

De futures évolutions du projet seront les utilitaires annexes tels qu'une version GEN.TTP de Devpac3 portée sur PC pour la génération des binaires sur PC sans utiliser d'émulateur tel qu'Hatari ou STeem. Ou encore le développement d'un décompilateur assembleur 68000 vers C pur pour recompiler le programme sous msys2 sur PC.
 
L'objectif ultime est la production de code source en C pur compilé sous Msys2 à partir d'images disque de démos ATARI ST pour les compiler au format PC et les exécuter sous Msys2 en fenêtre graphique Windows ou Linux sans émulation ATARI ST (d'où le 'revival'...)

## 1. Objectifs fonctionnels
- Application console interactive avec commandes : `help`, `quit`, `dir`, `load`, `edit`, `image`, `mount`, `umount`, `where`, `trace on/off` (les commandes ont un équivalent monogramme: 'h', 'q', 'd', 'l', 'e', 'i', 'm', 'u', 'w', 't' ainsi que des raccourcis clavier CTRL+H, CTRL+Q ou CTRL+C, CTRL+D, CTRL+L, CTRL+E, CTRL+M, CTRL+U, CTRL+W, CTRL+T)
- Éditeur de ligne riche avec gestion de l'historique des commandes par flèches haut et bas, home et end, tab-completion pour l'affichage des fichiers et/ou répertoires, et le pré-affichage en gris de la commande en cours d'écriture avec complément par la touche tab (seul le premier mot de la ligne de commande est une commande, les arguments sont en tab-completion sur les noms de répertoire/fichiers).


### 1.1 (`h` | `help` | `CTRL+H`) 
Cette commande ouvre la liste des commandes disponibles ainsi qu'un résumé de ces commandes et de leurs arguments. Elle ne prend pas d'argument et ignore les éventuels arguments fournis en avertissant l'utilisateur.

### 1.2 (`q` | `quit` | `CTRL+Q`) 
Cette commande ferme toutes les vues ouvertes et quitte proprement l'application. Elle ne prend pas d'argument et ignore les éventuels arguments fournis en avertissant l'utilisateur.
 
### 1.3 (`d` | `dir` | `CTRL+D`) 
Cette commande ouvre une vue GUI de type tree view et affiche l'arborescence indentée du contenu du répertoire fourni en argument de la commande. La commande sans paramètre ouvre le répertoire courant. La vue GUI permet :
  - la sélection d'un fichier ou répertoire par clic gauche souris ou par touche 'espace' est surlignée dans la vue GUI et la sélection devient l'argument par défaut des commandes `load`, `edit`, `image`, `mount`, `wf`.
  - l'expansion ou non d'un répertoire par clic gauche souris sur + devant le nom du répertoire
  - la rétractation d'un répertoire expansé par clic gauche souris sur - devant le nom du répertoire
  - la remontée de l'arborescence du répertoire parent via une première ligne '..' en haut de l'arborescence
  - l'affichage d'un menu contextuel par clic droit souris sur un fichier ou répertoire:
       - clic droit sur fichier affiche les commandes (`load`, `edit`)
       - clic droit sur répertoire affiche les commandes (`mount`, `image`)

### 1.4 (`l` | `load` | `CTRL+L`) 
Cette commande prend en argument un nom de fichier (+ path) en entrée ou le fichier sélectionné via la commande `dir`; si aucun fichier n'est sélectionné et aucun paramètre donné, `load` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `load` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un fichier : charge ce fichier dans la mémoire émulée de l'ATARI ST à un emplacement libre. Pour une image, un fichier texte, la copie en mémoire est conforme au contenu du fichier. Pour un fichier binaire au format ATARI ST (.PRG, .TTP, .TOS), la montée en mémoire se fait selon les conventions du TOS ATARI ST, en mettant à jour les fixups d'addresse du programme. Ce programme binaire pourra être directement exécuté depuis la mémoire virtuelle ATARI ST par l'émulateur CPU 68000 et/ou par l'émulateur machine ATARI ST.
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les répertoires
   - si l'argument fourni ou par défaut est une image disque ATARI ST .st, .msa, .stx: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les images disques.

### 1.5 (`e` | `edit` | `CTRL+E`) 
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

### 1.6 (`m` | `mount` | `CTRL+M`) 
Cette commande prend un chemin de répertoire en entrée ou le répertoire sélectionné via la commande `dir`; si aucun chemin n'est sélectionné et aucun paramètre donné, le chemin courant de l'application est utilisé, mais une demande de confirmation utilisateur est nécessaire. `mount` se comporte de la manière suivante:
   - émule la montée d'un répertoire dans un disque A:\ ATARI ST en affichant une vue arborescence disquette. 
   - permet l'ajout de fichiers dans l'émulation disquette par clic+glissement souris dans la vue à partir de la vue ouverte par la commande `dir`
   - permet la suppression de fichiers dans l'émulation disquette par clic+glissement souris hors vue
   - la vue complète permet la création d'une image disque à partir d'un clic droit sur la première ligne de l'arborescence nommée "A:\", via la commande `image` en popup. La vue intègre également un bandeau vertical de propriétés de la disquette montée : ces propriétés sont celles du header disk ATARI ST.

### 1.7 (`x` | `execute` | `CTRL+X`)
Cette commande prend en argument le fichier binaire exécutable ATARI ST (.PRG, .TTP ou .TOS) ou celui sélectionné dans la vue `mount` de l'émulation disquette ou dans la vue de la commande `dir`. `execute` comprend les vues suivantes:
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP) visualisant les instructions en cours d'exécution
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires, avec possibilité de plusieurs vues par plages d'adresse mémoire (plage interruptions, mémoire système, RAM, Vidéo, TOS)
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

La commande `execute` privilégie l'exécution pas à pas avec l'ensemble des vues interactives entre elles, cependant l'exécution en temps ralenti, réel ou accéléré reste possible en tâche de fond avec mise en place de breakpoint pour points de visibilité. La vitesse d'exécution des binaires et la synchronisation complète des différentes vues lors de l'exécution sera limitée par la performance CPU/Video de la machine exécutant l'application ST4Ever; le moniteur d'exécution donne les informations d'exécution et de performance en instructions émulées par secondes.

### 1.8 (`u` | `umount` | `CTRL+U`)
Cette commande permet de clore la vue ouverte par la commande `mount` tout en démontant l'émulation disquette A:\ ATARI ST. `umount` simule le retrait de la disquette du lecteur ATARI ST. Si le contenu disquette émulé ne correspond à aucune image .st, .stx ou .msa existante, demande à l'utilisateur s'il souhaite créer une nouvelle image de la disquette en cours de retrait pour ne pas perdre les modifications réalisées lors de la commande `mount`.

### 1.9 (`w` | `where` | `CTRL+W`)
Cette commande permet d'afficher le répertoire de travail ou le fichier sélectionné courant via la commande `dir` ou le répertoire par défaut dans lequel l'application est lancée. La réponse à la commande est fournie par texte console.

### 1.10 (`t` | `trace` | `CTRL+T`)
Cette commande prend on/off en paramètre, ou toggle le statut courant si aucun paramètre est entré. Elle ouvre une fenêtre Trace Console (affichage coloré des logs) permettant d'afficher :
  - les logs de trace permettant d'affichier les fonctions des fichiers *.c de l'application exécutées avec un résumé très court. Les traces ont une gestion de compactation par nombre de passage lorsque les fonctions récurrentes sont tracées consécutivement, pour éviter la production de trop nombreux logs textes. 
  - les logs d'infos développeur permettant d'afficher régulièrement du contenu de progression de l'application, 
  - les logs d'erreurs internes (pointeurs null, fonctions KO, erreurs permettant le debuggage), 
  - les logs 'to-do' permettant d'identifier les fonctions/lignes où du code doit être enrichi.


## 2. Architecture & Structure de fichiers
Les vues ouvertes par les commandes sont interactives, non-modale et exécutée dans des threads séparés de l'application. Les vues sont capables de communiquer entre elles de manière dynamique selon leur besoin, par exemple la vue arborescence de la commande `dir` permet de glisser des fichiers dans l'arborescence de l'émulation disquette de la commande `mount`.
Dans les phases d'exécution des binaires, les vues d'émulation CPU 68000, mémoire ATARI ST, vue écran graphique et vue binaire hexadécimale sont mises à jour en cohérence de l'exécution.

L'application ST4Ever est développée pour Windows dans un premier temps, avec des stubs anticipés pour une plateforme Linux. Toutes les logiques qui ne dépendent pas d'une plateforme Windows ou Linux sont en code portable dans ./src de l'arborescence du projet. Le code portable utilise des fonctions d'abstraction appelant des fonctions Windows ou Linux en back-end, développé dans des fichiers ./src/win ou ./src/linux. La contrainte de développement est de maximiser le code portable et minimiser le back-end spécifique au strict nécessaire (Windows calls, DirectX calls pour Windows, Posix/system calls, X11 calls pour Linux)

### 2.1 Conventions:
- la documentation et le code sont en anglais - seul ce fichier SPEC-fr.md reste en français, il s'agit de notre fichier projet, mis à jour au fur et à mesure des conversations
- les noms de variables suivent le format hongrois par type en minuscule suivi d'un nom fonctionnel avec première lettre en majuscule, e.g. uiIndex, szMessage, ulBytes, ...
- les noms de structure se terminent par *_t, ceux qui contiennent plusieurs mots sont séparés par underscore, e.g. renderer_context_t, tree_view_node_t, ...
- les variables représentant des structures sont au format tRCView, tTVNFile, ...
- les noms de variables globales commencent par g_* et contiennent un acronyme relatif au source .c auquel il se rattache, e.g. g_edit_uiCount, g_dir_tRCView, ...
- les noms de fonctions expriment des actions, sont écrits en minuscule et construits en mots entiers séparés par des underscore, e.g. read_file(), open_new_window(), is_dir_expanded(), ...
- Privilégier la lisibilité du code à un code compact
- Privilégier les appels de fonctions plutôt que condenser le code dans une seule fonction
- Ecrire le code sur 80 colonnes
- Ne pas faire des appels de fonctions dans les paramètres de fonctions
- Les fonctions C pur portables fonctionnent avec un code retour ST_ERROR / ST_NO_ERROR pour remonter les erreurs dans l'arborescence d'appel des fonctions et sortir de l'application proprement en traçant la remontée des fonctions dans les logs
- Les retours fonctionnels se font par pointeurs dans les paramètres.
- Les fonctions contiennent toutes des vérifications de paramètres avant exécution et génère une erreur en cas de paramètre incorrect.
- Les fonctions spécifiques plateforme respectent le manuel utilisateur de la plateforme spécifique, les codes retours sont traités et loggés.
- Tous les retours de fonctions sont testés
- Toutes les fonctions sont documentées dans les includes avec un descriptif fonctionnel, la description des paramètres in/out et des codes de retour.
- Pour les logs en console Trace:
    - LOG_TRACE permet la trace d'entrée de chaque fonction avec identification du nom de la fonction et des paramètres d'entrées
    - LOG_INFO permet l'envoi d'information d'état utiles des fonctions exécutées pour suivre fonctionnellement l'implémentation de l'application
    - LOG_INTERNAL_ERROR permet l'affichage d'erreurs en identifiant la fonction concernée, la ligne de code concernée, la justification textuelle de l'erreur. Les erreurs des fonctions spécifiques plateforme sont remontées via les fonctions system de ces plateformes (e.g. GetLasterror() pour les fonctions Windows)
    - LOG_TODO affiche la fonction concernée et la ligne de code à laquelle du code additional ou futur peut être rajouté (=stubs)



### 2.2 Arborescence:
src/ 
- main.c: code d'initialisation des structures et variables globales, capture des options de la ligne de commande, lancement de la console principale, gestion de sortie et fermeture de l'applicatif.
- common.h: contient les types communs et codes retours des fonctions de l'application, abstraction mutex portable
- trace.h, trace.c: source de la console de trace et debug de la commande `trace` avec fonction de rotation, filtre (TRACE, INFO, ERROR, TODO), compaction TRACE.
- line.h, line.c: code portable de la gestion de la console principale (commandes, historique, tab-completion, pre-completion des commandes, messages utilisateurs)
- gui.h, gui.c: code portable de la gestion abstraite des fenêtres et interactions utilisateurs et événements
- renderer.h, renderer.c: code portable de la gestion abstraite for 2D rendering in GUI & ATARI ST emulation video screen
- dir.h, dir.c: code portable de la vue GUI de l'arborescence courante ou indiquée en paramètre de la commande `dir`
- mount.h, mount.c: code portable de la vue d'émulation disquette ATARI ST et arborescence de son contenu
- ST.h, ST.c: code portable de l'émulation interne de la machine ATARI ST (écriture/lecture mémoire ATARI ST, émulation disquette, composants internes)
- CPU.h, CPU.c: code portable de l'émulation du CPU 68000 de l'ATARI ST
- disassemble.h, disassemble.c : code portable du désassembleur binaire -> DEVPAC3 assembleur
- as.h, as.c : code portable de l'assembleur format DEVPAC3 -> binaire .PRG ou .raw
- edit_txt.h, edit_txt.c : code portable de l'éditeur de texte / source code assembleur
- edit_hex.h, edit_hex.c : code portable de l'éditeur hexadécimal & ASCII
- edit.h, edit.c : code portable de la vue intégrée de la commande `edit` d'édition des binaires par la vue hexa ou la vue assembleur.
- exec.h, exec.c : code portable du moteur d'exécution et synchronisation des vues d'exécution de la commande `execute`
- exec_mon.h, exec_mon.c : code portable du GUI moniteur de l'exécution (pas-à-pas, breakpoint, vitesse d'exécution, run-until, activation/veille des vues assembvleurmémoire, CPU, écran)
- exec_mem.h, exec_mem.c : code portable de la vue hexa de la zone mémoire en cours d'exécution
- exec_cpu.h, exec_cpu.c : code portable de la vue CPU 68000 et de l'état de ses registres en cours d'exécution
- exec_asm.h, exec_asm.c : code portable de la vue assembleur lors de l'exécution
- exec_screen.h, exec_screen.c : code portable de la vue écran graphique de l'ATARI ST en cours lors de l'exécution.

win/
- win_console.c : Code Windows-specific de la gestion des consoles et lignes de commandes
- win_gui.c : Code Windows-specific de la gestion des fenêtres et interactions utilisateurs
- win_D2D.c : Code Windows-specific du backend DirectX 2D du renderer abstrait portable.
- win_platform.c : Code Windows-specific des system calls de type opérations de fichiers, threads, process, pipes, mutex 

linux/
- lx_console.c : Code Linux-specific de la gestion des consoles et lignes de commandes
- lx_gui.c : Code Linux-specific de la gestion des fenêtres et interactions utilisateurs
- lx_X11.c : Code Linux-specific du backend X11 du renderer abstrait portable.
- lx_platform.c : Code Linux-specific des system calls de type opérations de fichiers, threads, process, pipes, mutex

use_cases/
- use_cases.h : le fichier des includes commun des use_case_*.c
- use_case_*.c: fichiers de test par Use Cases permettant de valider l'avancée du projet

use_cases/UC*/
- *.* : les fichiers ressources ou de tests nécessaires aux use cases (e.g. binaires ATARI ST, dummy images, fichier assembleur 68000 de test, bootsectors, ...)

Makefile : détecte automatiquement les fichiers sources et includes, gère le multi-platform, la compilation GCC sous MSys2 pour Windows ou terminal pour Linux, place les .o dans un répertoire de build, place les exécutables dans ./release, compile les use cases de tests et place les exécutables de tests dans ./tests
  - make : génère l'ensemble des fichiers ./build, ./release, ./tests
  - make tests : exécute les tests
  - make run : exécute l'applicatif
  - make clean : supprime les dossiers de ./build, ./release, ./tests

README.md : Fournit une description du projet synthétique en anglais
SPEC-fr.md : Ce fichier en français


## 3. Packaging
tar -czf project.tar.gz src Makefile README-SPEC.md

## 4. Recommendations Claude AI

## 5. Workpackages

Les étapes de développement fonctionnelles sont formalisées en Use Cases, permettant de développer et valider chaque cas d'usage de l'application et d'enrichir le projet avec de plus amples détails, dont les recommendations Claude AI de la section 4, et planifier le reste des Use Cases en TODO/stubs dans le projet. La liste actuelle des Use Cases est:
 - Use Case #1: Prototype complet du projet avec stubs des fonctions non actives et LOG_TODO pour les différentes fonctionnalités de la section 1. Le Use Case #1 génère la console principale avec les commandes `help`, `quit`. Les autres commandes sont stubbées. L'application utilise l'option -t pour lancer la console de Trace au démarrage de la console principale: la console de Trace affiche au moins log de chaque type (LOG_TRACE, LOG_INFO, LOG_ERROR, LOG_TODO).
 - Use Case #2: La commande `trace` sans paramètre toggle les LOG_TRACE de la console de Trace, la commande `trace on` active les LOG_TRACE, la commande 'trace off` désactive les LOG_TRACE. Les autres paramètres sont considérés comme sans effet avec un message utilisateur.
 - Use Case #3: la commande `dir` ouvre une fenêtre GUI de l'arborescence courante.

Il faut raffiner ces Use Cases pour qu'ils couvrent une somme raisonnable d'ajout fonctionnel afin de valider l'ajout, relire le code et tester les fonctions. Les recommendations de la section 4. sont les bienvenues.


## 6. Licence & attribution
Pas de redistribution prévue à ce jour