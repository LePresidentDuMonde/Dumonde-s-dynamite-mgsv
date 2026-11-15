# Compagnons personnels et scans communs — protocole 3

Cette version remplace le compagnon unique choisi par l'hôte. Chaque joueur garde son propre D-Dog ou D-Horse, son équipement et ses ordres locaux. Aucune copie du choix de l'hôte, aucun rechargement forcé ni déplacement du compagnon par les coordonnées du partenaire.

Les détections automatiques de chaque D-Dog sont transmises dans les deux sens. Les deux chiens peuvent détecter la même cible : la sortie de portée d'un chien ne retire pas la contribution de l'autre. Le marquage manuel conserve son chemin distinct et sa durée en virgule flottante.

Le jeu conserve un acteur de compagnon local par PC. Cette version ne crée pas deux copies visibles de compagnons sur chaque PC et ne réplique pas leurs squelettes ou leurs animations. Les modèles et les cibles doivent être chargés par le moteur ; les joueurs très éloignés restent soumis au streaming normal du jeu. Quiet et D-Walker ne font pas l'objet de cette correction.

## Installation

Fermer le jeu sur les deux PC, utiliser **dynamite-personal-buddies-windows-amd64.exe** puis **Save and reinstall** sur chacun. Choisir son compagnon dans la préparation de sortie habituelle. Le protocole 3 exige cette même version sur les deux PC ; il refuse les messages de l'ancien compagnon partagé.

## Fonctionnement et protections

- Capture du véritable message natif de détection de D-Dog, avec activation et disparition. Une détection distante ne devient jamais une détection locale à retransmettre.
- Ensembles complets de détections envoyés toutes les 100 ms, y compris un ensemble vide. Un paquet perdu est rattrapé par le suivant. Les ensembles anciens ou dupliqués sont ignorés.
- Maximum de 64 cibles par paquet ; le chien natif dispose de 32 enregistrements. Validation des identifiants, ordre unique, taille et époques de mission avant application.
- Application sur le fil de jeu. Le thread réseau ne touche pas aux objets du moteur. Les verrous de capture sont relâchés avant les appels moteur et réseau.
- Recherche de l'objet et du gestionnaire courant à chaque application. Les cibles absentes sont réessayées lors des mises à jour suivantes ; aucun pointeur d'acteur n'est conservé.
- À la déconnexion ou après 3 secondes sans ensemble reçu, retrait des seules détections distantes. Les détections locales et les marqueurs manuels sont préservés selon le comportement natif.
- Aucun doublement des pools de compagnons, aucune correction de physique distante. Les nouveaux hooks vérifient les octets attendus avant installation.

## Analyse native

Source : décompilation fournie, exécutable international 1.0.15.4, SHA-256 `085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`.

| Contrat observé | Adresse / valeur |
| --- | --- |
| Initialisation du détecteur Dog : source et raison | `0x140c7ba00`, source `3`, raison `0x594e8b44` (`BuddyDog`) |
| Construction du message de détection réutilisé | `0x140c79af0`, hash `0xd797144f5e39` |
| Émission d'une détection Dog, branche d'un unwind chaîné | `0x140c7b1ba`, appel virtuel `+0x38` |
| Retrait des détections lors du nettoyage Dog | `0x140c7d8f0` |
| Dispatcher GameObject commun | `0x140bf3a30`, vtable `0x142305e78` |
| Réception des détections temporaires et distinction du marquage manuel | `0x1405c56f0` |
| Message de marquage manuel, différent des scans | `0x73f1d2c50cda`, émission `0x14127fb10` |
| Préparation, tâches et fin de mise à jour Dog | `0x140c55690`, `0x140c55530`, `0x140c55580` |

Le payload natif minimal lu fait 16 octets : hash à `+0`, source à `+8`, indicateurs à `+9`, raison à `+12`. Le réseau transmet seulement des identifiants typés, jamais ce payload ni ses pointeurs. La résolution du joueur zéro vers le propriétaire local est limitée aux callbacks de compagnon ; le ciblage des ennemis conserve sa logique existante.

## Validation

`tests/run.ps1` exécute les émetteurs/récepteurs de production avec appels moteur simulés, le décodage des événements natifs, la fusion des contributions et le contrôleur Lua réel. Les contrôles couvrent les scans dans les deux sens, les chevauchements, les suppressions, les pertes de paquets, le désordre, les doublons, les anciens mondes, les tailles limites, les durées fractionnaires et le maintien des choix personnels. Ils incluent les régressions Side Ops, mission 13 et initialisation de session.

Le jeu n'a pas été lancé pour cette validation. La compilation et ces tests ne prouvent pas l'absence de crash en partie : l'apparition, le suivi, le scan et le rendu doivent encore être vérifiés avec deux jeux réels.
