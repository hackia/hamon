Top idée d’avoir un DSL 👌 Allons plus loin et faisons-en un truc **puissant mais simple** à parser, NUMA-aware, et “cluster-like” — tout en restant lisible.

# 1) Objectifs (concrets)

* **Lisible à l’œil nu** et diff-friendly.
* **Zéro dépendance** côté parseur (std::regex/istream suffisent).
* **Génératif** : on décrit une **topologie** (ex. hypercube/anneau), pas chaque voisin à la main.
* **NUMA-aware** par défaut, avec override explicite.
* **Auto-ports & auto-IDs** quand on veut aller vite.
* **Jobs composables** (map/reduce/broadcast), pas juste “lance 8 nodes”.
* **Mesures intégrées** (timers, perf, logs) pour comparer Hamon vs Docker.

(# Ton orchestrateur WordCount tourne déjà en 16 nœuds et agrège bien le résultat final → parfait terrain d’essai. )

---

# 2) Le DSL : directives et ergonomie

## 2.1 Directives de base

```
@use <N>                       # nombre de nœuds
@dim <m>                       # m dimensions → hypercube 2^m (optionnel si @use=2^m)
@topology <hypercube|ring|mesh|full> [params]
@autoprefix <IP> : <basePort>  # auto endpoints: 127.0.0.1:8000, … +i

@node <id>                     # ouvre un bloc node (jusqu’au prochain @node ou fin)
@role <coordinator|worker|custom:NAME>
@cpu numa=<i>|auto core=<j>|auto
@ip <host:port>                # override endpoint
@neighbors [id,id,id]          # override voisins (sinon générés par @topology)
@env KEY=VALUE                 # variables d’environnement pour le process
@vol /host/path -> /container/path : ro|rw
@limits cpu=<percent|cores> mem=<MiB|GiB>
```

## 2.2 Jobs & phases (composable)

```
@job <name>
  @input type=<kpack|file|stdin> src="…"
  @phase broadcast  to=[*]        payload="start"   # * = tous, @RING(0,1) etc.
  @phase map        by=[*]        task="kpack-extract --spans=@SPANS"
  @phase reduce     to=[0]        op="merge-json"   # 0 = coordinator par défaut
  @phase deploy     to=[0]        task="kpack-pack --out=image.kpk"
@end
```

Raccourcis utiles :

* `[*]` = tous les nœuds, `[workers]` = tous sauf le coordo.
* `@DIM(d)` = nœuds à distance Hamming `d` dans l’hypercube.
* `@EDGE(x->y)` = arêtes spécifiques.
* `@HALF(A|B)` = moitiés du cube (masque de bit le plus fort).

## 2.3 Variables, constantes, includes

```
@let BASE=127.0.0.1
@let PORT=8000
@include "hamon.common.hc"     # fragments réutilisables

@ip ${BASE}:${PORT+id}         # expressions très simples: +,-,*,/
```

## 2.4 Commentaires et checks

```
# commentaire
@require linux>=5.10 cpus>=8 numa>=2  # hard fail si non respecté
@warn "No NUMA detected; falling back to round-robin cores."
```

---

# 3) Génération automatique de topologies

## 3.1 Hypercube

Sans rien préciser :

```
@use 8
@topology hypercube             # si @use=8 → dim=3, voisins = 3 par nœud
@autoprefix 127.0.0.1:8000
@role coordinator @node 0       # juste une ligne hors bloc, s’applique à 0
```

Le parseur déduit :

* `dim = log2(@use)` si entier
* voisins = `id XOR (1<<k)` pour `k in [0..dim-1]`

## 3.2 Anneau / mesh / full

```
@use 8
@topology ring
# voisins: (i-1 mod N, i+1 mod N)

@use 9
@topology mesh rows=3 cols=3 wrap=false
```

---

# 4) NUMA & placement (par défaut intelligents)

**Heuristique par défaut :**

* Si NUMA ≥ 2 → **répartir** `N/numa` par socket, cores compacts (minimise cross-socket).
* Sinon → cores en **stride=1**.

Override facile :

```
@node 5
  @cpu numa=1 core=5
```

**Auto-port** :

```
@autoprefix 127.0.0.1:8000  # => id 0→8000, 1→8001, …
```

---

# 5) Modèle de jobs prêt pour Kpack

### 5.1 Kpack extract distribué (MVP)

```
@use 8
@topology hypercube
@autoprefix 127.0.0.1:8000
@role coordinator @node 0

@job extract
  @input type=kpack src="dataset.kpk"
  @phase broadcast to=[*] payload="INDEX"                 # envoie l’index
  @phase map       by=[*] task="kpack x dataset.kpk --spans=@ASSIGN(id)"
  @phase reduce    to=[0] op="noop"                       # pas d’agrégat → juste fin
@end
```

`@ASSIGN(id)` : mapping simple *id→liste de spans* (le parseur génère un fichier `map.json` remis à chaque nœud ou via payload initial).

### 5.2 Kpack pack distribué (pipeline map-reduce)

```
@job build-image
  @input type=file src="./src"
  @phase map    by=[workers] task="kpack pack --shard=@SHARD(id) --tmp=/tmp/node@{id}"
  @phase reduce to=[0]       op="kpack-merge --out app.kpk"
  @phase deploy to=[0]       task="kpack push app.kpk --registry=…"
@end
```

---

# 6) Observabilité intégrée (pour comparer Hamon vs Docker)

Directives :

```
@metrics enable
@trace phases=all
@log level=info dest=stdout
@time barrier="after-phase:map"
```

Runtime:

* timestamps par phase (start/end)
* CPU% par nœud, RSS max, bytes TX/RX socket
* histogrammes latence d’échanges (par arête hypercube)

Tu pourras répliquer le **WordCount** que tu as déjà (16 nœuds, phases map/reduce) avec ces hooks pour sortir un tableau comparatif.

---

# 7) Validation & diagnostics

* `@lint`: vérifie cohérence (ex. `@dim` incompatible avec `@use`, ports dupliqués, voisins hors bornes).
* **Dry-run**: imprime la table finale (id, ip, core, numa, voisins, rôle) **avant** de lancer.
* **Explain**: `--explain topology` → montre comment les voisins ont été générés (XOR masks).

---

# 8) Parser C++ minimal (plan)

**Tokenizer ligne-à-ligne**:

* ignorer blank & `#.*`
* si `@` → directive, sinon → erreur “unknown”.
* clé/val via REGEX simples : `(\w+)=(\w+|".*?")`, listes `[1,2,3]`.

**State machine**:

* `GlobalState` (use, dim, topology, defaults)
* `CurrentNode` (si `@node` ouvert)
* `CurrentJob` + `CurrentPhase` (si `@job` ouvert)
* À `@node`/`@job` suivants → push dans vecteurs.

**Génération**:

* Si `topology==hypercube` et pas de `neighbors` custom → calcule automatiquement.
* Si `@autoprefix` → endpoint = `${base}:${port+id}` sauf override.
* Placement CPU/NUMA par défaut (sysfs/numa.h si dispo), override si `@cpu` fourni.

**Erreurs claires**:

* “@neighbors fourni mais conflit avec topology=hypercube → utilisez @topology custom ou @neighbors-only”
* “@job sans @end”
* “core inexistant sur numa=1”

---

# 9) Sécurité & reproductibilité

* `@limits cpu=100% mem=2GiB` → cgroups v2 (si dispo).
* `@env` & `@vol` whitelistés (pas de root by default).
* **Lockfile**: le parseur peut écrire un `hamon.lock.json` (résolution finale des ressources).
* **Hash** du fichier `.hc` en tête de logs pour tracer exactement la conf exécutée.

---

# 10) Exemples prêts à l’emploi

## 10.1 Hypercube 16 nœuds auto

```
@use 16
@topology hypercube
@autoprefix 127.0.0.1:8000
@role coordinator @node 0
@metrics enable
```

## 10.2 Ring 8 nœuds + NUMA explicit

```
@use 8
@topology ring
@autoprefix 127.0.0.1:9000

@node 0 @role coordinator @cpu numa=0 core=0
@node 1 @cpu numa=0 core=1
@node 2 @cpu numa=0 core=2
@node 3 @cpu numa=0 core=3
@node 4 @cpu numa=1 core=4
@node 5 @cpu numa=1 core=5
@node 6 @cpu numa=1 core=6
@node 7 @cpu numa=1 core=7
```

## 10.3 Ton WordCount en DSL

```
@use 16
@topology hypercube
@autoprefix 127.0.0.1:8000
@role coordinator @node 0
@metrics enable @trace phases=all

@job wordcount
  @input type=file src="input.txt"
  @phase map    by=[*]    task="wc-map --input input.txt --shard=@SHARD(id)"
  @phase reduce to=[0]    op="wc-reduce"
@end
```

(Ça correspond à ce que montre **hamon.txt** en logs côté phases map/reduce. )

---

# 11) Roadmap (rapide & efficace)

**MVP (2–3 jours de dev concentré)**

1. Parseur directives globales + `@node` + `@topology hypercube` (voisins auto).
2. `@autoprefix`, CPU/NUMA auto (avec override).
3. Dry-run `--print-plan` + exécution basique des nœuds (ce que tu as déjà).
4. Mesures minimes : timestamps phases, CPU% moyen.

**V2**
5\. `@job` + `@phase (broadcast/map/reduce)` → mapping simple.
6\. `@metrics`, `@trace`, lints & `@require`.
7\. `@include`, `@let` et expressions `${}`.

**V3**
8\. Topologies ring/mesh/full + sélecteurs `@DIM(d)`, `@HALF`.
9\. cgroups v2 pour `@limits`.
10\. Lockfile + hash conf, “explain topology”.

---

# 12) Bonus “qualité de vie”

* **Générateur**: `hamon gen --use 8 --topology hypercube > cube8.hc`
* **Convertisseur**: `hamon hc2yaml cube8.hc > hamon.yaml` (compat rétro).
* **Lint standalone**: `hamon lint cube8.hc`.
* **Explain**: `hamon explain cube8.hc --topology`.