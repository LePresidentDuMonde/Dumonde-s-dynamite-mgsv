# Audit de la mission 13 et des téléportations

## Défauts confirmés dans le code livré

- La réception UDP reconnaissait le préfixe privé `DYNAMITE`, traitait le paquet,
  puis retournait encore ses octets au décodeur natif de MGSV. Le nouveau filtre
  consomme ces paquets et poursuit la lecture jusqu'au prochain paquet natif,
  sans modifier son contenu. Une limite de 32 paquets évite de bloquer la tâche
  réseau lors d'une rafale. Le code natif retourne `-1` en l'absence de paquet.
- Les scripts de compagnons installés appartenaient à l'ancienne implémentation
  partagée, tandis que le code source et les API récentes utilisent des compagnons
  personnels. La livraison doit reconstruire ensemble la DLL et tous les scripts,
  puis vérifier leur présence dans l'archive installée. Aucun overlay conservant
  l'ancien script n'est utilisé pour cette livraison.
- Le changement du repère pendant `BeginMission` n'avait pas de validation en
  session réelle. Il est retiré, en conservant l'initialisation au démarrage de
  la mission. Aucune position de joueur n'est corrigée artificiellement.

## Vérifications

`tests/receive_demux.cpp` appelle le filtre de production avec des flux mixtes :
paquets privés puis mouvement natif, rafales, messages courts, message privé
malformé, erreurs et réception vide. Le paquet natif doit ressortir intact et
les messages privés ne doivent jamais lui être substitués.

La suite complète couvre aussi les notifications de membres absents, le
handshake et ses générations, les messages hors ordre, les scans et poses de
compagnons, le chargement Lua, les quêtes, le bootstrap et le rechargement après
explosion. Les tests hors moteur ne démontrent pas la stabilité d'une partie
Steam à deux joueurs.

Le hook de `Player2GameObjectImplWarp` journalise l'identifiant du joueur, la
destination et la pile appelante, au plus une fois par seconde, puis transmet
l'appel d'origine sans modifier ses arguments. Les téléportations demandées par
le raccourci `WarpToPartner` possèdent déjà leurs propres traces. Ce relevé doit
permettre d'identifier l'auteur d'un déplacement réel du client ; il ne supprime
pas les déplacements normaux de mission.

## Limites

Les deux joueurs doivent installer exactement le même paquet complet. Le
protocole courant refuse les versions de protocole antérieures. Le lancement
local valide le démarrage du jeu ; une partie avec deux comptes Steam distincts
reste nécessaire pour valider les mouvements, la visibilité, la mort/reprise de
checkpoint et l'explosion des réservoirs. Un second processus utilisant le même
compte n'est pas une preuve équivalente.

La base SQLite et les sauvegardes personnelles ne sont pas réinitialisées.
Les archives et binaires précédents sont conservés avant installation.

## Validation locale effectuée

La copie isolée des sources, dans
`.build-tools/audit-mission13-20261115/source`, a été recompilée entièrement.
Tous ses tests natifs, Lua et Go passent. Les cinq modules Lua contrôlés et la
DLL installée correspondent aux sources et au binaire de cette copie.

MGSV a réellement été relancé via Steam avec cette installation. L'écran-titre
et la scène hélicoptère ont été observés, puis **Continuer** a chargé la mission
10080. Le personnage et le terrain s'affichent ; le jeu répond au clavier et aux
menus. Aucun crash ni erreur Lua/native n'apparaît dans les logs de cette courte
vérification locale, et la position au repos reste stable.

Le partenaire est absent pendant ce test (`connected=false`). La disparition
des téléportations du client en coop n'est donc **pas encore démontrée**. Il n'a
pas été effectué de partie Steam à deux joueurs ni de test réel d'explosion à
deux avec cette version. Les résultats et captures sont conservés dans
`.build-tools/audit-mission13-20261115/validation.json` et les fichiers `live-*`.
