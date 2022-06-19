# DSM

Ce projet a pour but de développer un système de mémoire partagée et distribuée sur des machines
en réseau. Il est découpé en deux phases, dont la première a pour but le développement d’un lanceur
de programmes, qui a pour tâche de lancer différents processus sur des machines distantes via SSH,
tout en récupérant et en centralisant les sorties standards et d’erreur de ces machines. Ce premier
programme permet aussi de mettre en place tout le nécessaire pour la DSM dans l’étape suivante,
comme la mise en place de sockets d’écoute menant à une interconnexion de tous les processus entre
eux.
La deuxième phase de ce projet a pour but la mise en place du protocole de DSM grâce à tout ce qui
a été mis en place lors de la première phase. Dans cette phase ont donc été implémentés l’échange des
pages et la gestion des accès mémoire par les différents processus en réseau.
Nous nous proposons donc dans ce rapport d’expliciter les solutions choisies et les difficultés rencontrées
au cours de ce projet dans un premier temps pour la première phase, puis dans une deuxième partie
nous parlerons de la deuxième phase du projet.
