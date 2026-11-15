# Deux D-Dog visibles — version expérimentale, protocole 4

Cette version ajoute un second acteur D-Dog sur chaque PC. L'emplacement 0 reste le chien personnel simulé localement. L'emplacement 1 est destiné à afficher le chien du partenaire à partir des matrices de son modèle et de son squelette. Les scans restent partagés dans les deux sens.

Le code et les assets sont implémentés ; **l'affichage de deux chiens dans deux jeux réels n'a pas été constaté pendant le développement**. Les tests décrits ci-dessous contrôlent les scènes, le protocole, les transitions et les limites de mémoire. Ils ne constituent pas une validation du rendu ou une garantie d'absence de crash.

## Installation et essai

1. Fermer le jeu et installer **dynamite-two-ddogs-experimental-windows-amd64.exe** avec **Save and reinstall**, sur les deux PC.
2. Pour le premier essai, sélectionner D-Dog avec le même équipement et la même apparence sur les deux PC. Les deux joueurs doivent rester dans une zone chargée en commun.
3. Vérifier que chacun voit deux modèles, que seul son propre chien répond à ses ordres, puis tester marche, course, attente, aboiement et animation d'attaque. Confirmer que les scans d'un chien apparaissent sur l'autre écran sans attendre un marquage aux jumelles.
4. Tester ensuite le rappel du compagnon, la déconnexion, la reconnexion et le rechargement d'une mission. La copie distante doit disparaître sans retirer le chien local.

La version précédente reste disponible dans `dynamite-personal-buddies-windows-amd64.exe` pour réinstallation en cas de problème.

## Ce qui change

- Les cinq scènes `buddy_dog2_00` à `buddy_dog2_04` déclarent deux instances réalisées et deux locators distincts. La copie possède ses propres entités de transform et de paramètres. Les cinq packs Dog sont remplacés dans la sortie du mod ; aucun pack Horse n'est modifié.
- Le chien distant conserve le drapeau natif de désactivation. Le code demande séparément la réalisation de son modèle afin d'afficher les poses reçues. **L'isolation effective de son IA et de ses collisions reste une hypothèse d'intégration à vérifier en jeu.** Les commandes vers l'index 1 sont filtrées, sauf les lectures de transform et de statut.
- Le propriétaire envoie la matrice du modèle et les matrices de ses articulations à 20 Hz. La réception ne lance pas les ordres d'attaque du partenaire. Le rendu vise à reproduire les animations au moyen des poses ; les événements audio ne sont pas répliqués par ce flux.
- La position et le quaternion de l'acteur sont transmis séparément des matrices de rendu. Le code les applique pendant la pré-mise à jour, avant les tâches du modèle, avec le contrat de transform utilisé par le Warp natif du joueur. L'emploi de ce contrat pour le chien distant reste à valider en partie.
- Chaque pose est découpée en paquets inférieurs à la limite existante de 1 024 octets. Une pose incomplète n'atteint jamais le moteur. Une pose complète suivante permet de récupérer une perte de paquet. Les anciennes poses et générations sont rejetées.
- Les écritures portent uniquement sur les matrices d'un modèle retrouvé dans le graphe courant, avec vérification du nombre d'articulations, de l'espace de coordonnées et des pages mémoire. Aucun pointeur, vtable ou descripteur reçu par réseau n'est utilisé.
- Les poses complètes sont interpolées pendant 50 ms ; les téléportations dépassant 20 mètres sont appliquées directement. Après trois secondes sans pose complète, la copie distante est masquée.
- Le partage des scans et la gestion des quêtes secondaires restent actifs. Les deux PC doivent utiliser le protocole 4 ; une version limitée aux scans, en protocole 3, ne peut pas se lier à cette session.

## Limites

Le modèle distant réutilise les ressources de D-Dog chargées localement. Cette version ne charge pas séparément l'équipement ou la couleur choisis par le partenaire ; une incompatibilité de squelette masque la copie. La réplication entre types différents, comme D-Dog chez un joueur et D-Horse chez l'autre, ainsi que Quiet et D-Walker, ne sont pas implémentés ici.

Les matrices reproduisent une pose visuelle. Elles ne constituent pas une réplication complète de la vie, de l'affinité, du comportement, des sons ou des interactions du compagnon. La compatibilité du buffer de poses avec le rendu final et le comportement de l'acteur désactivé doivent encore être vérifiés dans le jeu.

## Contrats natifs utilisés

Analyse statique de l'exécutable international 1.0.15.4 fourni dans `D:\mgsv-analysis\mgsv-analysis`, SHA-256 `085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`.

| Contrat observé | Adresse préférée |
| --- | --- |
| Allocation Dog et nombre d'instances tiré du descripteur | `0x140a58310` |
| Pool du modèle Dog et index physique des acteurs | `0x140b42320`, `0x140af1ae0` |
| Lecture des matrices des articulations via le modèle Gr | `0x140af0e10`, `0x140215cd0` |
| Résolution du chien logique vers son emplacement réalisé | `0x140c55ac0` |
| Transformation du modèle Gr et calcul des matrices | `0x1401ce7f0`, `0x1401ce260` |
| Étape de soumission du modèle | `0x140af0240` |
| Contrat de transform de l'acteur, utilisé par le Warp du joueur | `0x1409ceae0`, méthode virtuelle `+0x68` |
| Calcul des demandes de réalisation selon le drapeau désactivé | `0x140c58550` |
| Commande Dog `SetEnabled`, indexée par acteur | `0x140c56770` |

La séparation entre désactivation de l'IA et réalisation du modèle, ainsi que l'emploi du buffer d'articulations pour piloter la copie distante, sont des choix d'intégration déduits de ces contrats. Leurs contrôles statiques et hors jeu ne prouvent pas leur fonctionnement dans le moteur en cours de partie.

## Contrôles automatisés

Les scènes FOX2 embarquées sont relues et réécrites, avec vérification des deux locators, des compteurs, des identités distinctes, des références et de leur présence dans l'installation. Le protocole de production est exercé entre deux contextes hôte/invité avec services moteur simulés : fragmentation, désordre, pertes, poses incomplètes, identités de propriétaire, matrices invalides, borne de 256 articulations, rappel et anciennes missions. Une copie de matrices réelle vers un buffer entouré de sentinelles vérifie les limites d'écriture ; les appels de rendu natifs ne sont pas exécutés.

Les contrôles Lua couvrent les mises à jour des poses et scans à leurs fréquences respectives, le retrait de la copie distante et le repli de désactivation si les hooks nécessaires sont indisponibles.
