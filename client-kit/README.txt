BEETLE ADVENTURE RACING — CLIENT WINDOWS X64 EXPERIMENTAL

Extraire tout le ZIP dans un dossier, puis ouvrir Rejoindre.cmd.
Saisir l'adresse du PC hote et le code de 32 caracteres fourni par l'hote.
Un joueur distant : laisser 1 joueur, choisir son moniteur, puis Lancer.
Pour la course a trois, rejoindre AVANT que l'hote clique sur Start Game.
L'hote choisit Heberger, Course a 3, puis Single Race. F9 separe les vues.
La ROM est necessaire sur le PC hote uniquement.

Connexion directe TCP 28765. Une adresse 127.0.0.1 designe son propre PC :
utiliser l'adresse LAN de l'hote sur un meme reseau. Pour Internet, un chemin
reseau vers l'hote est necessaire ; aucun relais ni configuration NAT automatique.
Les tests disponibles utilisent des processus sur un seul PC. Deux PC sur
Internet, une course complete et ses resultats restent a valider.
La grille entierement humaine n'est pas encore prise en charge.

Commandes : clavier physique ZQSD sur AZERTY (positions WASD), fleches,
Espace = A/accelerer, Entree = Start, Maj = B/frein. Xbox egalement prise en charge.
Echap ferme le client. Le clavier fonctionne quand une vue a le focus.
Video JPEG et audio PCM calcules chez l'hote ; authentification par code,
contenu reseau non chiffre. Les choix des nouveaux pilotes copient celui de J1.

Code source du client dans source/, licence AGPL-3.0 (COPYING-AGPL-3.0.txt).
SDL2 est sous licence zlib (COPYING-SDL2.txt) ; licences stb dans les en-tetes.
Reconstruction : outils C++ Visual Studio x64, Windows SDK et CMake >= 3.20 :
  cmake -S source -B build -A x64
  cmake --build build --config Release
Copier SDL2.dll pres du nouvel executable. Les bibliotheques systeme Windows
ws2_32 et bcrypt sont fournies par le SDK. Aucun fichier du jeu dans ce client.
