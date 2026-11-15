# Mission 13 : rechargement après l'explosion des réservoirs

La destruction d'un réservoir de l'objectif déclenche une explosion en chaîne,
puis `FuncMissionReload` recharge le pack `afterPumpStopDemo`. L'initialisation
réseau se trouvait dans `Seq_Demo_InAfrica`, qui n'est plus exécutée après cette
transition : origine réseau, admission de l'hôte et synchronisation manquaient.
La session restait aussi active jusqu'à `OnTerminate`, alors que
`SafeStopSettingOnMissionReload` commence déjà à décharger des objets.

Le correctif initialise la coop depuis `TppMain.OnMissionGameStart`, y compris
après le changement de pack et les reprises de checkpoint. Il arrête les tâches
et ferme la session avant le déchargement, une seule fois. Les notifications
d'explosion répétées ne peuvent plus demander plusieurs rechargements.

Un client connecté avant cette transition tente de rejoindre l'hôte après son
chargement : trois essais maximum, espacés de vingt secondes. Une connexion
réussie annule les essais restants. Cette reprise automatique ne s'applique ni
à une partie sans partenaire, ni à la connexion initiale depuis l'hélicoptère.
Le changement de pack, les renforts et les objectifs de la pompe sont conservés.

Les deux joueurs doivent reconstruire le lanceur et utiliser **Save and reinstall**
pour installer les scripts embarqués dans les trois packs de la mission 13.
Remplacer uniquement la DLL ne met pas ces scripts à jour.

`tests/mission13.lua` exécute les scripts de production avec des services moteur
simulés : explosion en chaîne, rechargement unique, fermeture avant déchargement,
origine réseau, réouverture de l'hôte, reprises de connexion bornées et progression
dans les deux ordres (pompe ou réservoirs en premier). Ces tests ne remplacent
pas une validation en jeu à deux comptes Steam.

Le test `tests/bootstrap.lua` couvre aussi le parcours réel des modules par
`TppMain.OnAllocate` pour le titre, une mission histoire et l'exploration libre.
Les indicateurs de session sont conservés séparément : ajouter des booléens
à la liste des modules provoque une erreur Lua et bloque le démarrage du jeu.

Sources locales : script stock extrait de `master/chunk4.dat` dans le dossier
`mgsv-analysis/archives/extracted`, et analyse du cycle natif des blocs dans
`docs/issue_7.md`. Aucun contournement des contrôles mémoire natifs n'est ajouté.

## Envois réseau refusés pendant la mission

Les logs du 15 novembre à 19:05 montrent une connexion Steam établie, puis des
refus d'envoi toutes les demi-secondes après le démarrage de la mission 10080.
Le message `steamUDPSocket is null` regroupait plusieurs conditions et ne
permettait pas d'identifier le pointeur concerné. `SendRaw` exigeait notamment
un `SocketInfo` non nul, alors que le désassemblage de `SteamUdpSocketImpl::Send`
à `0x141a5fb60` montre que ce quatrième argument n'est pas utilisé.

Ce contrôle bloque aussi le handshake du protocole coop lorsque le moteur fournit
un `SocketInfo` nul. Le correctif autorise cette valeur et conserve les contrôles
sur le socket, l'adresse de destination et les paquets. Le test natif reproduit
le refus du handshake avant correction, puis vérifie le protocole complet avec
ce paramètre nul. Une destination absente reste refusée.

Ce défaut de synchronisation est confirmé par le code et les tests ; les logs
seuls ne permettent pas d'attribuer tous les freezes à cette cause. Une validation
en session Steam à deux reste nécessaire après remplacement de la DLL chez les
deux joueurs.

## Joueur connecté mais parfois invisible

Le log de 19:38 confirme cette fois l'établissement du protocole de la mission
10080, sans déconnexion enregistrée avant la remise à zéro manuelle à 19:43.
Une connexion établie ne garantit pas que le moteur puisse afficher le partenaire.

`TppDefine` contient déjà une réserve d'équipements de 1 777 721 octets accompagnée
d'un commentaire sur le client invisible dans Pitch Dark. Cependant, la mission
définit sa propre réserve de 1 340 000 octets. `TppPlayer.OnAllocate` ajoute
100 000 octets à cette valeur et ignore alors le défaut global : seulement
1 440 000 octets sont effectivement demandés au moteur.

Le correctif applique le minimum existant de 1 777 721 octets en mission 13 coop,
en conservant toute réserve explicitement supérieure. Le log indique la valeur
effectivement retenue. `tests/player_visibility.lua` charge le module de production
et vérifie la demande envoyée à `CreateEquipMissionBlockGroup`, ainsi que les
budgets supérieurs, les autres missions et le chemin hors coop.

Ce contournement involontaire de la réserve du mod est confirmé par le code.
Les logs ne contiennent pas de mesure d'épuisement de cette réserve : le lien avec
chaque disparition reste donc à valider à deux en jeu. Les deux installations
doivent mettre à jour les scripts avec le nouveau lanceur et **Save and reinstall**.

## Repère des positions pendant le chargement

Les logs client de la même partie confirment le protocole prêt à 19:38:36,
après des notifications de session à 19:38:27, pendant `OnAllocate`.
Le repère reste défini dans `OnMissionGameStart`. Le changement qui l'appliquait
aussi dans `BeginMission` est retiré : sa sûreté pendant la transition d'une
session native encore active n'a pas été démontrée et des téléportations ont
été signalées après sa livraison.

Le test de bootstrap vérifie qu'aucun changement de repère ne se produit pendant
l'allocation de scène. Une trace `mission 13 sync` toutes les dix secondes indique le rôle,
la connexion, l'état du protocole et les coordonnées locales. Elle utilise les
variables du joueur déjà disponibles, sans déplacer le joueur distant. Cette
trace permet de comparer les deux logs si l'immobilité persiste ; elle ne mesure
pas directement les paquets natifs de mouvement.

Le log client contient également un crash à 19:44:18 après une remise à zéro et
une nouvelle connexion. Il intervient plusieurs minutes après l'établissement
du protocole ; il ne suffit donc pas à expliquer le personnage figé auparavant.
Les logs ne prouvent pas la cause du mouvement figé. Voir l'audit des paquets
et les limites de validation dans `AUDIT_MISSION13_FR.md`.
