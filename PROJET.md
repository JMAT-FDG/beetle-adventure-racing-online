# Beetle Adventure Racing! — PC, multi-écran et réseau

## Menu intégré — 29 octobre 2026

`Lancer Beetle.cmd` ouvre désormais directement le jeu. L'entrée « Multijoueur : Héberger / Rejoindre » du menu intégré ouvre un panneau superposé contenant l'adresse de l'hôte, le code et sa copie, le nombre de joueurs locaux, le mode de course à deux/trois et l'affectation des moniteurs. Le même panneau est accessible dans Paramètres → En ligne. Le formulaire PowerShell reste une solution de secours, sans être le point d'entrée du lanceur principal.

Configurer l'hébergement **avant Start Game** ; l'ami rejoint avant de démarrer le jeu, puis choisir Single Race. La création d'un serveur après le démarrage de la simulation est actuellement refusée avec un message dans le panneau. Rejoindre ouvre les vues vidéo du client ; les erreurs de connexion reviennent dans le panneau sans boîte de dialogue séparée. Les limites du mode trois joueurs et d'Internet décrites ci-dessous restent applicables.

`Test menu integre.ps1` vérifie en fenêtre cachée les commandes, la validation des champs et la fermeture/réouverture, avec capture interne `panel.png`. L'option `-Session` vérifie aussi le démarrage d'un serveur depuis le panneau et un client sans fenêtre sur TCP 28769. Les clients graphiques ne sont pas lancés par ces tests. L'archive client distribuée précédemment conserve son interface classique et reste compatible.

## Mise à jour du 28 octobre 2026 — état courant

**Nouvelle priorité : deux humains sur le PC hôte plus un ami distant en course, puis autant d'humains que de places sur la grille. Tests sans prendre le bureau.** Les sections datées du 26 octobre ci-dessous sont historiques.

Le port a été compilé et un prototype réseau/multi-écran est désormais implémenté. `Lancer Beetle.cmd` propose héberger/rejoindre, choix des écrans, clavier J1 et Xbox J2, et une option explicite « Course à 3 » expérimentale en mode hôte. Une simulation tourne chez l'hôte ; les clients envoient leurs commandes et reçoivent une vidéo JPEG par joueur et l'audio PCM. Les vues locales passent également par ce transport. Sortie adaptée au moniteur, capture jusque 2560 × 1440 par vue, 30 images/s par défaut. Les essais moteur ci-dessous utilisent 640 × 480 : ni le rendu haute résolution par moniteur, ni une course avec Xbox physique, ni une partie Internet ne sont validés.

Les tests réseau automatiques passent. Le nouveau `Test reseau trois joueurs.ps1` vérifie **trois processus distincts**, deux entrées locales et une distante, emplacements séparés, vidéo et audio synthétiques. Ce test ne prouve pas une course jouable.

`Test course arriere-plan.ps1` exécute le moteur avec une fenêtre cachée, sans son ni commandes physiques, profil isolé et copie du Controller Pak. Les captures sont internes au rendu et chaque exécution s'arrête automatiquement. Trois objets joueurs distincts sont créés et mis à jour ; le moteur lit l'accélérateur du client distant. Le défaut initial de la troisième vue a été corrigé en configurant les trois humains **avant le chargement du circuit**, après le nettoyage de la sélection. Le dernier essai de 150 secondes a reçu 1 856 images côté client. La capture au poll 7500 montre trois vues cohérentes, mais ne valide pas une course complète, les collisions ni l'arrivée/résultats.

Pour essayer : choisir « Héberger en ligne », cocher « Course à 3 », donner le code à l'ami et le faire rejoindre **avant Start Game**, puis choisir « Single Race ». Le découpage en grille est choisi automatiquement ; F9 active les vues individuelles une fois en course. Le choix de voiture de J1 est copié aux nouveaux pilotes. Aucun lobby prêt/pas prêt, reconnexion en course, ni accès Internet entre réseaux n'est encore validé.

Client à copier chez l'ami : `client-online.zip`. Extraire tout puis ouvrir `Rejoindre.cmd`, saisir l'adresse de l'hôte et le code. Il ne contient ni ROM, ni ELF, ni exécutable du jeu hôte. Sources du client, CMake et licences inclus. `Preparer client.ps1` le régénère ; `Verifier sources client.cmd` a recompilé ces sources indépendamment du jeu et son test réseau passe. Les modes hôte et rejoindre du lanceur ont été vérifiés par `-ValidateForm`, sans afficher de fenêtre.

La grille semble viser huit voitures (un humain et sept IA dans le réglage original), **pas huit humains déjà pris en charge**. Les réglages des voitures, le tableau de joueurs de course, les caméras, les commandes et le protocole actuel comportent des limites à quatre à modifier ensemble. L'étude et les preuves sont dans `source/docs/ONLINE_RACING_RESEARCH.md`.

Commandes : `build-game.cmd`, `Test reseau trois joueurs.ps1`, puis `Test course arriere-plan.ps1 -Players 3 -Seconds 130 -Network -ProductionSwitch` pour le test intégré avec le réglage du lanceur. Journaux et captures sous `logs/`. Le réseau Internet, la latence, les arrivées/résultats et le support de toute la grille restent à développer et valider. REA a été consulté ; il n'a pas été installé et aucun fichier du jeu ne lui a été envoyé.

## Objectif

Un participant héberge la partie. Chaque PC peut accueillir plusieurs joueurs locaux avec leurs propres manettes. Chaque joueur local peut avoir son moniteur, ou partager un écran. Commencer par les quatre ports manettes du jeu ; augmenter le nombre de joueurs demande une étude distincte.

Attention : quatre ports manettes ne garantissent pas quatre participants en course. Les menus et la documentation du code distinguent la course à deux joueurs et Beetle Battle à quatre (`docs/HUD_MENU_SYSTEM_MAP.md`, entrée `func_gamegui_00401E50`). Deux joueurs locaux plus un joueur distant en course peut donc nécessiter d'étendre les structures et le mode de course, en plus du transport réseau.

## Installation au 26 octobre 2026

- ROM USA extraite dans `roms/beetle-usa.z64`, SHA-1 vérifié : `e5ab4d226c08d22f68a2edcc48870203e67454b8`.
- Version communautaire Windows `v0.4.2-alpha` dans `runtime/` ; SHA-256 de l'archive vérifié contre l'API GitHub : `b7ad13d3518f6645d32b66bea2c8832595cfea76838299d800e20bf943a704d4`.
- Source : https://github.com/danielgomesvieira2000/beetle-adventure-racing-recomp ; révision initiale `6497e67257809401468cd47850393775952b31bb`.
- Décompilation : https://github.com/bryankruman/BeetleDecomp.
- Démarrage du lanceur et chargement de la ROM observés. Écran de création de sauvegarde atteint. Course et multijoueur pas encore validés.
- Aucun contrôleur SDL détecté lors du premier essai. Le profil clavier du frontend diffère des anciennes touches documentées dans le runtime ; voir Controls dans le lanceur.
- Le jeu écrit ses paramètres, sa ROM en cache et ses sauvegardes dans `%LOCALAPPDATA%\beetle-adventure-racing-recomp`.

Double-cliquer `Jouer.cmd` pour ouvrir la version communautaire. Cette version n'ajoute pas le jeu en ligne ni un écran par joueur.

## Points d'entrée vérifiés dans le code

- `source/src/frontend/bar_frontend.cpp` : quatre joueurs maximum ; affectation automatique des manettes ; `port_assigned` et `poll_port` exposent les entrées par joueur.
- `source/src/main/main.cpp` : une fenêtre SDL ; chemin de ROM accepté comme premier argument ; `BAR_WINDOW_SIZE` règle la taille de la fenêtre.
- `source/patches/viewport_patch.c` : ajustement de la zone de rendu ; conserve les frontières internes du split-screen.
- `source/docs/technical/06-graphics.md` : canaux caméra et rectangles de découpe (`uvchannel_rom`, `uvgfxmgr_rom`). Une vue plein écran par joueur demande aussi de corriger projection, HUD et culling.
- Frontend : rendu RT64 créé par `recompui::renderer::create_render_context`. Modifier uniquement `src/main/rt64_render_context.cpp` ne suffit pas pour cette version.

## Séquence de développement et critères de validation

1. Reproduire une compilation locale : décompilation → ELF → N64Recomp/RSPRecomp → code C généré → exécutable Windows. Comparer son démarrage à l'alpha téléchargée.
2. Valider une course solo, les commandes clavier et deux manettes locales ; noter circuit, mode, version et erreurs. Valider aussi les sauvegardes.
3. Multi-écran local : associer joueur/manette/moniteur ; prototype à deux vues, puis correction des proportions, HUD, culling et menus. Supporter débranchement d'un écran et retour au split-screen.
4. Réseau : commencer par deux processus en local puis deux PC en LAN. Établir si la simulation peut être déterministe avant de choisir un échange d'entrées. Sinon, identifier les états nécessaires à une synchronisation par l'hôte. Éviter de supposer qu'envoyer les positions suffit aux collisions et aux objets.
5. Tester un PC avec deux joueurs contre un PC distant, puis Internet, latence, perte de paquets et déconnexion. Connexion directe au prototype ; prévoir la traversée NAT ou un relais pour une connexion simple sur Internet.

## Configuration locale demandée

Clavier/souris et manette Xbox. Le mode local modifié réserve le joueur 1 au clavier et affecte la première manette au joueur 2 (`BAR_SEPARATE_KEYBOARD=1`). Les profils des autres joueurs sont réinitialisés par le code d'affectation existant ; le clavier ne pilote donc pas aussi le joueur Xbox. La limite totale reste quatre joueurs. Le lanceur `Jouer clavier et Xbox.cmd` utilise exclusivement l'exécutable recompilé et échoue explicitement s'il manque. Le changement doit encore être compilé et testé avec la manette connectée. La souris reste utilisable dans les menus ; aucune direction à la souris n'est ajoutée.

Moniteurs détectés : principal 2560 × 1440, deux secondaires 1920 × 1080 ; positions dans `monitors.json`.

Les outils N64Recomp/RSPRecomp ont été compilés avec LLVM 18.1.8 sous Windows. Dépendances MIPS/Python installées dans Ubuntu 24.04. Le dépôt amont récent de décompilation est conservé dans `decomp/` pour référence ; la reconstruction du jeu utilise la version incluse dans `source/lib/bar-decomp/`, compatible avec le portage. Son extraction et son `make recomp` sont terminés : ELF de 10 623 368 octets, 133 sections de modules. La génération N64Recomp et les contrôles de `fix-recompiled.sh` sont passés ; les patches MIPS ont également été compilés et recompilés.

Outils et correctifs de construction locaux :

- LLVM Windows 18.1.8 dans `tools/llvm18` ; compilateur hôte avec les Build Tools Visual Studio 2019 déjà présents.
- CMake 3.31.10 et Ninja 1.13.0 dans `tools/build-python`, pour les chemins Unicode. Les outils anciens de Visual Studio produisaient des fichiers de réponse incompatibles avec les accents du chemin.
- DXC Microsoft `v1.9.2609` et fichiers CMake zstd `v1.5.6`, manquants dans le dépôt du portage, restaurés depuis leurs projets officiels.
- DirectX-Headers Microsoft `v1.614.1`, révision `48a762973271c5a75869946bf1fdbc489a628a5c`, inclus via l'option CMake `BEETLE_DIRECTX_HEADERS`. Le SDK installé 10.0.19041 ne définit pas le type de heap GPU utilisé par Plume.
- En-têtes précompilés désactivés pour cette construction locale ; construction dans `tools/host-build`.

Commandes Windows : `build-tools.cmd`, puis `generate-game.ps1` lorsque l'ELF est présent, puis `build-game.cmd`. Les sorties de diagnostic sont dans `logs/`. Le lanceur clavier/Xbox utilise la version modifiée, le lanceur `Jouer.cmd` conserve l'alpha téléchargée comme référence.

## Limites actuelles

Historique du 26 octobre : réseau et multi-écran absents à cette date ; remplacé par l'état courant en tête de document. L'alpha amont annonce des gels possibles après les résultats et un plantage intermittent au démarrage. Trois joueurs et les courses split-screen ne sont pas validés par ses notes de version. Ne pas présenter l'alpha comme un portage terminé.

Conserver la ROM et les fichiers générés hors des commits. Le code du portage est AGPL-3.0 ; les bibliothèques conservent leurs propres licences. Aucun fichier utilisateur envoyé à un service externe.
