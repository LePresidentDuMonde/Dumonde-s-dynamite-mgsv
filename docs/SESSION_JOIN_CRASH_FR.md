# Crash lors d'une connexion coop

Le log du 15 novembre 2026 à 19:26:05 contient une violation d'accès
`0xc0000005` à `0x140ae2754`, lecture de l'adresse zéro. La pile passe par le
hook `TppGmImplScriptDeclVarsImplOnSessionNotify`, pour le membre réseau 1.

Le désassemblage de `0x140ae2410` montre que cette notification de connexion
demande l'interface du membre à la session, puis utilise son résultat sans
vérifier s'il est nul. L'instruction qui plante est précisément cette lecture.
Cela correspond à une notification dont le membre n'est plus disponible.

Le log montre également une demande manuelle de remise à zéro à 19:24:43,
puis une nouvelle création de session. Cette commande effaçait les indicateurs
locaux sans fermer la session native : le mod pouvait donc tenter de créer
une nouvelle session alors que la précédente existait encore. C'est un défaut
confirmé du chemin de remise à zéro, et un facteur possible de notifications
obsolètes ; le log ne prouve pas à lui seul toute la chronologie interne.

Le correctif vérifie la présence du membre avant de transmettre la notification
au moteur. Il conserve les conditions natives qui ignorent une autre session ou
un callback client. La remise à zéro manuelle ferme désormais la session native
avant d'effacer son état local. Les callbacks de fermeture restent transmis.

`tests/session_notify.cpp` utilise les mêmes offsets et appels virtuels que la
fonction native pour vérifier les membres absents, valides puis partis, ainsi
que les chemins client, fermeture et arguments absents. Les tests restent hors
du moteur ; une reconnexion à deux comptes Steam doit confirmer le résultat.

La base SQLite du serveur (`dynamite.dat`, ses fichiers WAL et SHM) n'est pas
modifiée par ce correctif. L'erreur enregistrée se situe dans le jeu et la DLL.
