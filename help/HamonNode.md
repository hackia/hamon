Voici une explication claire et structurée de HamonNode.cpp.

Idée générale
- Chaque HamonNode représente un nœud d’un cluster logique organisé en hypercube.
- Objectif: faire un “word count” distribué sur un fichier d’entrée, via une phase Map (chaque nœud compte ses mots) puis Reduce (les nœuds agrègent leurs comptages pair-à-pair selon la topologie hypercube).
- Le nœud 0 joue le rôle de coordinateur: il lit le fichier, découpe le texte, envoie des morceaux aux autres nœuds, puis affiche le résultat final après la réduction.

Principales structures utilisées
- topology_node: contient l’identifiant du nœud (id).
- cube: décrit la topologie hypercube (nombre de dimensions, nombre de nœuds).
- all_configs: configuration réseau de tous les nœuds (ip, port, rôle).
- local_counts: map<string,int> pour compter les mots localement sur un nœud.

Cycle de vie d’un nœud
1) run()
   - setup_server(): ouvre un socket d’écoute (TCP) sur le port dédié au nœud.
   - petite pause de 100 ms pour laisser tous les nœuds démarrer.
   - distribute_and_map():
     - Si id == 0 (coordinateur): lit “input.txt”, le découpe en N parts (N = nombre de nœuds) et envoie à chaque nœud i>0 sa portion par TCP. Le nœud 0 traite localement la première portion.
     - Sinon: attend une connexion entrante et reçoit sa portion, puis la traite.
   - reduce(): agrégation pair-à-pair selon l’hypercube (XOR des ids).
   - Si id == 0: print_final_results().
   - close_server_socket().

Détails par fonction

- setup_server()
  - Crée un socket TCP, active SO_REUSEADDR, bind sur INADDR_ANY:port_du_noeud, puis listen(backlog 16).
  - Affiche un message indiquant que le serveur écoute.

- distribute_and_map()
  - Coordinateur (id 0):
    - Ouvre input.txt; si échec, arrête tout.
    - Découpe le texte en “chunk_size = len/N”.
    - Pour chaque nœud i = 1..N-1:
      - Se connecte via TCP au port du nœud i.
      - Envoie la sous-chaîne correspondante.
    - Traite localement la portion [0, chunk_size).
  - Worker (id != 0):
    - Bloque sur accept() pour recevoir la connexion de 0.
    - Reçoit sa sous-chaîne, lance le comptage local.
  - perform_word_count_task(text_chunk):
    - Parcourt le texte via stringstream et incrémente counts[word]++.

- Protocole d’envoi/réception de chaînes
  - send_string(sock, str):
    - Envoie d’abord un uint32 longueur en big-endian (htonl), puis les octets de la chaîne.
  - receive_string(sock):
    - Lit 4 octets (taille), convertit en host-endian (ntohl).
    - Vérifie une borne de sécurité (len < 65536).
    - Lit exactement len octets et retourne la chaîne.
  - Ce framing “length-prefix” évite les découpages arbitraires des flux TCP.

- Sérialisation pour la réduction
  - serialize_map(map): produit un format simple “mot:compte,” pour chaque entrée.
  - deserialize_and_merge_map(str, map):
    - Parse par virgule, puis par “:”, convertit le compte et additionne dans la map cible.

- reduce()
  - But: agréger les résultats via une “hypercube reduction”.
  - Pour chaque dimension d de l’hypercube:
    - partner_id = id XOR (1 << d).
    - La paire (id, partner_id) échange leurs données une seule fois, en imposant un sens:
      - Si id > partner_id:
        - Le nœud “supérieur” tente de se connecter au partenaire (jusqu’à 5 tentatives avec 50 ms de pause).
        - S’il réussit, envoie serialize_map(local_counts).
        - Puis “break” pour cette dimension (évite double échange).
      - Sinon (id < partner_id):
        - Attend accept() une connexion entrante.
        - Reçoit le map sérialisé, fusionne via deserialize_and_merge_map.
    - De dimension en dimension, la map s’agrège progressivement vers le plus petit id.
  - À la fin, le nœud 0 détient la somme globale.

- print_final_results()
  - Sur le nœud 0, affiche toutes les paires “mot -> compte”.

- close_server_socket()
  - Ferme le socket d’écoute.

Points d’attention et comportements implicites
- Découpage de texte: simple partition par taille. Il peut couper au milieu d’un mot; selon l’usage, on pourrait vouloir découper sur séparateurs.
- Robustesse réseau: le code a des retries simples côté client lors du reduce, mais n’a pas de timeouts/erreurs sophistiquées. Les accept() sont bloquants.
- Encodage: serialize_map utilise un format ad hoc, sans échappement; si des mots contiennent “:” ou “,” ça casserait le parsing.
- Mémoire/performances: pour des textes très grands, on pourrait streamer; ici tout est en mémoire.
- Sécurité: une limite 64K protège receive_string, mais pas les parties map -> string; pour de gros résultats, on pourrait fragmenter ou lever la limite.
- Ordonnancement de la réduction: la logique XOR garantit des appariements corrects sur un hypercube si le nombre de nœuds est une puissance de deux.

En résumé
- Le nœud 0 lit le fichier, distribue des morceaux, chacun fait un “map” local (word count).
- La phase reduce agrège par paires selon XOR des ids pour chaque dimension de l’hypercube.
- Le nœud 0 affiche le résultat final.