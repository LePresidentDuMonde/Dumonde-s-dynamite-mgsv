# SideOps partagées et compagnons locaux

La révision suivante ajoute un compagnon partagé D-Horse/D-Dog : voir [l'implémentation et ses limites](SHARED_BUDDIES_FR.md). Le présent document décrit la compilation locale précédente.

Révision du 15 novembre 2026, construite à partir du dépôt et de l'analyse locale `D:\mgsv-analysis\mgsv-analysis`. Le jeu et son installation n'ont pas été exécutés ou modifiés pendant ce travail.

## Comportement ajouté

L'Afghanistan (`30010`, variante `f30011` comprise) et l'Afrique (`30020`) chargent les ressources des deux joueurs. La définition solo est retirée des trois archives concernées. L'hôte ouvre sa session après chargement. Le délai réseau de ces cartes passe à trente secondes.

Une seule mission secondaire est partagée : l'hôte choisit la quête et conduit sa progression. L'invité suit son nom, son activation, son étape et son identifiant de visage, en conservant le chargeur asynchrone de `TppQuest`. Une perte temporaire de paquets ne décharge pas la quête. Les snapshots répétés ne rejouent pas une entrée d'étape déjà appliquée. Les écritures de récompenses et de progression de quête sont bloquées côté invité ; les deux joueurs n'obtiennent donc pas une progression personnelle commune.

D-Horse et D-Dog conservent les pools d'origine. Leur création reçoit un contexte local limité à ces deux types, sans changer le statut en ligne global ni le nombre d'instances. Le refus natif du sifflet de D-Horse est contourné uniquement pendant ce message, sur le cheval local. Les restrictions de scénario, de déverrouillage et de mort des compagnons restent gérées par le jeu.

Cette correction vise leur fonctionnement **local**. Elle ne fournit pas deux compagnons indépendants entièrement répliqués : la représentation du compagnon du partenaire, ses attaques, animations, montées/descentes et effets communs demandent encore une implémentation et des essais natifs. Quiet compagnon et D-Walker n'ont pas reçu de nouvelle réplication.

## Stabilité et protocole

- Échange challenge/écho par chargement de mission ; les paquets d'une ancienne allocation sont rejetés, même après redémarrage du même numéro de mission. Un ancien handshake ne peut pas rétablir un ancien pair.
- Réception bornée et copie possédée ; validation de la longueur réellement reçue, des chaînes, types, coordonnées, indices et tailles des structures natives avant application.
- Application des messages et lecture de proximité des ennemis sur la boucle du jeu. Le logger supporte les appels depuis plusieurs threads.
- Compteurs d'envoi atomiques, tolérance au réordonnancement et ordre des états par entité. Les véhicules conduits renouvellent leur état à l'arrêt ; un échec d'envoi ne met pas à jour leur cache.
- Descripteur de variable natif de seize octets, floats transmis par représentation binaire, contrôles de type/capacité avant écriture, tableaux fragmentés. Les écritures réseau arbitraires dans `gvars` sont refusées.
- Nettoyage de mission corrigé et idempotent ; le départ du lobby hélicoptère conserve sa connexion préparée. La réinstallation ne modifie plus son propre manifeste de suppressions.

Les véhicules et personnages visés par les corrections de position doivent déjà exister au même identifiant sur les deux machines. Une identité complète par apparition/bloc, les snapshots d'objectifs, les événements fiables de progression et l'union de streaming de joueurs éloignés restent à construire. Une arrivée au milieu d'une quête avancée ne restaure pas tous les effets des étapes précédentes. Rester dans la même zone et commencer la SideOp ensemble est le périmètre de l'essai initial.

## Installation de cette compilation

1. Utiliser MGSV TPP **1.0.15.4, version internationale**, sur les deux PC, avec la même compilation du mod.
2. Fermer le jeu, copier `dynamite-sideops-local-buddies-windows-amd64.exe` dans `MGS_TPP`, le lancer et utiliser **Save and reinstall** sur chaque PC. La DLL et les scripts doivent être installés ensemble.
3. L'hôte entre en exploration libre et attend le chargement complet. L'invité utilise le déroulement habituel : connexion depuis l'hélicoptère, puis départ sur la même carte. Si la transition native déconnecte encore la session, reconnecter après chargement avec le raccourci habituel.
4. Tester ensemble une SideOp simple, puis les appels et renvois de D-Horse/D-Dog. Vérifier les deux journaux, le résultat côté hôte et la sauvegarde personnelle de l'invité après retour au solo.

## Vérification effectuée

`powershell -NoProfile -ExecutionPolicy Bypass -File tests/run.ps1 -AnalysisRoot D:\mgsv-analysis\mgsv-analysis`

Les tests natifs compilent les véritables émetteurs et récepteurs du mod avec des fonctions moteur factices : handshake, anciennes allocations, réordonnancement, doublons, mutations différées, types/capacités, ABI float, fragmentation, rythme des véhicules et 10 000 paquets invalides. Les tests Lua 5.1 exécutent le module de production avec le **véritable `TppQuest.lua` extrait**, en remplaçant seulement les services moteur : chargement asynchrone, disponibilité des callbacks, étapes idempotentes, perte réseau, visage, protection des sauvegardes, déchargement, ouverture de session et fin de scène.

Les 51 scripts Lua du mod ont aussi passé la compilation syntaxique, et `go test ./...` vérifie la compilation des modules du lanceur. Ces vérifications hors jeu ne démontrent pas l'absence de crash en partie. La nouvelle création locale des compagnons, les effets des objectifs et la stabilité des transitions doivent encore être vérifiés avec deux jeux et deux comptes Steam.

## Preuves locales utilisées

- `coop-investigation/world_mgo/WORLD.md`, `buddies/BUDDIES.md` et `protocol/PROTOCOL.md` : limites des contextes uniques, pools et protocole précédent.
- `archives/fpk-members.csv` : noms des définitions solo dans les trois packs de monde libre.
- `archives/extracted/master_0_00_dat/direct/Assets/tpp/script/lib/TppQuest.lua` et `TppGVars.lua` : lifecycle, étapes, effets de progression et identifiants de visage.
- `native_disassembly/mgsvtpp.exe.ranges.jsonl.gz`, entrée `0x1409e5470` : descripteur de création, index de type à `+0x1c`, interrogation du statut en ligne.
- `coop-investigation/native/tpp/pass-1/assembly/0115_0x140e4d960_HorseMessageHandlerCandidate.asm` : borne d'index et refus du sifflet sur `self+0xc4 & 2`.
- `native_targeted_decompilation/bounded-pass-1/pseudocode/0111_0x1409abe20_TppGmImplScriptSystemImplGetScriptDeclVarHandle.c`, setter `0x140ae28e0` et sender `0x141a5fb60` : taille du descripteur, ABI des valeurs et résultat d'envoi natif.
