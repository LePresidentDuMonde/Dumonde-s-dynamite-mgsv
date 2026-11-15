# Compagnon partagé D-Horse / D-Dog — expérimental

Cette révision applique le choix d'un **seul compagnon partagé par l'équipe**. L'hôte choisit le type et l'équipement. Chaque machine conserve le pool d'origine d'une instance. Le protocole passe à la version 2 ; les deux PC doivent installer la même DLL et les mêmes scripts.

Le code ajoute :

- le chargement asynchrone du même D-Horse ou D-Dog chez l'invité, avec équipement et apparence ;
- les snapshots de position et de quaternion à 20 Hz, avec interpolation côté invité ;
- les ordres natifs autorisés, reconstruits depuis des champs typés, avec file bornée, retransmission et accusé de réception après application ;
- une révision de configuration qui invalide les anciens ordres lors d'un remplacement, en plus des époques de mission ;
- les propositions de position du cheval conduit par l'invité, contrôlées par l'hôte selon la proximité du joueur et une concession de 750 ms ; le cavalier local n'est pas déplacé par une correction distante ;
- l'attente de la descente du cavalier avant un rechargement de compagnon ;
- la restauration de l'équipement personnel à la déconnexion et à la fin de mission. Le passage Lua par `SaveVarsToSlot` utilise l'équipement personnel, même si l'appel échoue. Le callback Lua de checkpoint du compagnon partagé est bloqué chez l'invité.

La création de D-Dog est corrigée séparément : elle passe par `0x140a58310`, et non uniquement par la création générique utilisée pour D-Horse.

## Limites précises

**Ce travail ne permet pas de qualifier la synchronisation de complète ou sans crash.** L'adaptateur natif a été contrôlé statiquement et avec des acteurs simulés, sans exécuter le jeu. Les essais sur deux jeux restent indispensables pour vérifier les appels de physique, les contacts, le chargement réel des ressources et les montées/descentes.

Le protocole transmet la pose globale et les ordres autorisés, pas les poses du squelette, les phases d'animation, les points de vie ou tous les états de l'IA. L'IA native existe toujours sur les deux machines : les effets d'attaque de D-Dog ne bénéficient pas encore d'un filtre complet qui garantisse leur application unique par l'hôte. Les montées simultanées ne sont pas résolues par un verrou natif de siège. Quiet et D-Walker n'ont pas d'adaptateur dans cette révision. Ces éléments sont du travail restant, au-delà d'une simple validation en partie.

La restauration des huit champs d'équipement est testée. Elle ne constitue pas un audit de tous les chemins de sauvegarde natifs, ni de toute la progression d'affinité des compagnons. Les restrictions de mission et de ressources du moteur peuvent encore empêcher une apparition. Les joueurs doivent rester dans la même zone chargée ; cette révision ne crée pas une union de streaming pour deux joueurs très éloignés.

## Preuves et contrôles

Source native : `D:\mgsv-analysis\mgsv-analysis\native_targeted_decompilation\input\mgsvtpp.exe`, SHA-256 `085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`, version internationale 1.0.15.4.

| Point vérifié | Adresse préférée |
| --- | --- |
| Création générique Horse | `0x1409e5470` |
| Création Dog, wrapper et Quark | `0x140a58310`, `0x140a587c0` |
| Dispatch Lua générique, résolution d'un type | `0x140bfa380`, `0x147b5c9d0` |
| Message natif Horse / Dog | `0x140e4d960`, `0x140c56b10` |
| Wrapper de dispatch Dog | `0x140a58e70` |
| Groupe modèle Dog / Horse | `0x140c55bd0`, `0x140e4c5a0` |
| Lecture de physique Horse | `0x140e4bf90` |
| Lecture de quaternion/position Dog | cas `0xd0a5d0c7e373` dans `0x140c56b10` |
| Contrat du setter de physique utilisé par le joueur | `0x1409ceae0`, virtuels `+0x388` puis `+0x68` |
| Construction des ordres ciblés : cible à `+8`, joueur à `+10` | `0x141135c01`, `0x141226548` |

L'emploi du même setter de physique pour les compagnons est une inférence fondée sur les interfaces modèle communes et leurs lectures dans les handlers. Ce n'est pas une preuve d'exécution réussie dans MGSV. Les accès recherchent les objets à chaque appel, vérifient les vtables, la réalisation du modèle et les pages mémoire ; aucun pointeur natif reçu ou conservé après déchargement n'est utilisé.

`tools/native-query.go` lit les inventaires et l'exécutable statique. `tools/decompile-buddies.ps1` décompile dans un projet Ghidra en lecture seule ; les sorties de recherche sont ignorées par Git sous `.research/buddies`.

`tests/run.ps1` compile la DLL et exécute le protocole réel avec services moteur factices, l'adaptateur natif avec un graphe d'acteurs simulés et les modules Lua de production. Les scénarios couvrent les doublons, ACK perdus, files pleines, anciennes révisions, déchargements, types non pris en charge, quaternions invalides, rechargement asynchrone, conduite distante, sauvegarde et restauration de l'équipement.

## Compilation livrée

`dynamite-shared-buddies-experimental-windows-amd64.exe` contient la DLL et les scripts de cette révision. Utiliser la procédure habituelle **Save and reinstall**, jeu fermé, sur les deux PC. Aucun fichier de l'installation du jeu n'a été modifié pendant le développement et aucun jeu n'a été lancé pour ces contrôles.
