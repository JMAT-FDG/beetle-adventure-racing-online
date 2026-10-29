# Beetle Adventure Racing! — PC et online entre amis

Portage Windows x64 expérimental de **Beetle Adventure Racing!**, avec menu intégré **Héberger / Rejoindre**, clavier/Xbox et choix d'un moniteur par joueur local.

Le programme PC et son runtime sont fournis dans la version Windows à télécharger : aucun émulateur supplémentaire n'est nécessaire. Le code et les dépendances sont dans `source/`.

## Jouer

1. Dans **Releases**, télécharger `Beetle-Online-Windows-x64.zip`, puis **extraire tout**.
2. Sur le PC hôte, lancer `Importer ROM.cmd` et sélectionner sa copie locale de la ROM USA, au format `.z64` ou dans un ZIP. Son empreinte est vérifiée. La ROM commerciale n'est pas distribuée ici.
3. Ouvrir `Lancer Beetle.cmd`, puis **Multijoueur : Héberger / Rejoindre**.
4. L'hôte choisit ses deux joueurs locaux, leurs écrans et **3 pilotes (expérimental)**, puis **Héberger**. Le clavier pilote J1 et la première manette Xbox J2.
5. L'ami choisit **1 joueur sur ce PC**, saisit l'adresse de l'hôte et son code, puis **Rejoindre**. Les clients distants n'ont pas besoin de ROM.
6. L'ami rejoint **avant Start Game**. L'hôte démarre le jeu et choisit **Single Race**. En course, **F9** active le découpage des vues.

Le panneau est aussi accessible dans **Paramètres → En ligne**. L'hébergement se configure avant de démarrer la simulation. Échap ferme les vues client.

## Connexion

Connexion directe **TCP 28765**. Sur un même réseau, utiliser l'adresse locale du PC hôte ; `127.0.0.1` désigne toujours le PC sur lequel on la saisit. Entre deux réseaux, il faut une connexion directe routable ou un réseau privé reliant les PC. Il n'y a pas de relais ou de traversée NAT automatique. Le code de partie est requis ; le flux vidéo/audio n'est pas chiffré.

## Sauvegardes et réglages

La sauvegarde Controller Pak et les réglages partagés sont dans `profiles/shared/`. La version Windows est portable : ses données restent dans `tools/host-build/`. `Installer sauvegarde.cmd` restaure la sauvegarde partagée après avoir sauvegardé les fichiers existants ; fermer le jeu avant de l'utiliser. Une ROM déjà importée n'est jamais écrasée par cette opération.

Les codes de connexion, identifiants GitHub, journaux de test et autres projets ne sont pas inclus.

## Inviter les amis au dépôt privé

Dans GitHub : **Settings → Collaborators → Add people**, rechercher le pseudo GitHub de l'ami, puis confirmer l'ajout. Il accepte l'invitation et peut ensuite ouvrir **Releases** pour télécharger la version Windows.

Sur un dépôt personnel privé, les collaborateurs disposent également du droit d'écriture ; GitHub ne propose pas de rôle lecture seule dans ce cas. Voir les [instructions GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository) et les [permissions des dépôts personnels](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/permission-levels-for-a-personal-account-repository).

## État réel du prototype

- Trois objets joueurs, trois vues et les commandes du troisième joueur reçues par le vrai moteur ont été vérifiés dans des tests locaux.
- Le panneau intégré et l'ouverture d'un client recevant deux vues ont été testés dans une fenêtre cachée ; les suites réseau automatiques passent.
- Une course complète avec arrivée/résultats, les essais physiques clavier/Xbox et Internet entre deux PC restent à valider.
- Toute la grille humaine (huit places visées) **n'est pas disponible**. Ne pas confondre la limite de quatre ports avec des courses validées à quatre.
- L'alpha amont comporte encore des instabilités. Le choix de voiture de J1 est copié aux pilotes ajoutés dans le mode expérimental.

## Code et compilation

Base : [beetle-adventure-racing-recomp](https://github.com/danielgomesvieira2000/beetle-adventure-racing-recomp), révision indiquée dans `PROVENANCE.json`. Les auteurs, licences et notices d'origine sont conservés. Les modifications réseau, multi-écran et de course sont dans `source/src/`, `source/lib/rt64/` et `source/scripts/fix-recompiled.sh`.

Voir `source/BUILDING.md`, `source/docs/technical/` et `PROJET.md`. La chaîne locale vérifiée utilise LLVM 18.1.8, VS2019 Build Tools, CMake 3.31.10, Ninja 1.13.0 et Ubuntu/WSL pour reconstruire l'ELF depuis une ROM fournie localement. Les outils locaux vont dans `tools/`, ignoré par Git. `generate-game.ps1` génère le code ; `build-game.cmd` construit les exécutables. `build-support/directx-headers` et les fichiers Windows DXC/zstd restaurés sont fournis. Aucun ELF ni code généré extrait du jeu n'est ajouté au dépôt.

Les bibliothèques vendues dans `source/lib/` conservent leurs licences. Le portage est **AGPL-3.0** ; voir `LICENSE` et `source/THIRD_PARTY_NOTICES.md`.
