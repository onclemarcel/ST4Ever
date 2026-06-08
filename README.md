# Project : ST4Ever : The Revival Engine for the Timeless ATARI ST

## 1. Contexte du projet

***Contexte***: Ce projet est une application console interactive multi-plateforme développée en C pur à but éducatif permettant de:
- lire/écrire des fichiers ATARI ST (texte ou binaires .PRG, .TTP, .TOS)
- créer/lire/extraire des images disques ATARI ST de type .st/.msa à partir de/vers des répertoires PC
- visualiser, modifier, compiler, assembler, désassembler, décompiler les programmes ATARI ST et/ou les disques démos
- émuler les binaires ATARI ST via divers vues (CPU, Mémoire, écran) en pas à pas, temps ralenti ou temps réel 

***User Interaction***: La console interactive permet l'interaction utilisateur via des commandes et des vues GUI associées comprenant:
- un éditeur de fichier texte et/ou fichier source d'un binaire (e.g. assembleur d'un .PRG)
- un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP)
- une vue assembleur 68000 au format DevPac3 ATARI ST pour les sections binaires pertinentes de la vue hexadécimale (e.g. .text ou toute plage d'adresse fournie par utilisateur)
- un visualisateur d'arborescence fichiers/répertoires pour les sélections/gestions de fichiers et des images disques .st, .msa, .stx
- une console de trace/logs/erreurs pour l'application et debuggage développeur de ST4Ever
- un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

 ***Objectif éducatif***: ST4Ever est un projet totalement développé from scratch dans un but d'apprentissage du développement en C pur de chaque brique/étape. Cf section R0 pour les principes documentaires du projet afin de maîtriser et maintenir chaque étape de développement.

***Revival***: La phase de "Revival" est une extension de ST4Ever au-delà de l'émulation ATARI ST vers un portage du code d'une démo ST sous forme de C portable exécutable nativement, sans émulateur 68k, sur de multiples cibles. Le comportement de la démo portée ne sera pas strictement exacte par rapport à l'original ST, mais restera fidèle au "feeling" utilisateur : effets visuels reconnaissables, timing VBL raisonnable, son et inputs présents. 

## Current development progress

The project is developed Use Case by Use Case, with a rigorous test-driven approach
and full traceability (SRTD.md). Each UC is validated by an automated test suite
before being committed.

Use Cases status and progress is provided in ***CLAUDE.md section 6***

## Co-development with Claude AI

In terms of code production and project follow-up, this repository is assisted by Claude AI for educational purposes and my own understanding of the use of an AI assistant, ATARI ST emulation and demos development from 1990's.

The main project file shared with Claude AI is CLAUDE.md (in french) - but any AI assistant will translate it for you. Ssuccessive versions of the project file are created from my inputs and Claude's inputs; the change history will indicate which version is last updated by myself or Claude.

Note that this file is fully written manually - at least in its first issue ;)
