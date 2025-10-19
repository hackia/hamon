# Fichier: build-hamon-r710.hc
# Compile Hamon sur un Dell R710 (2x 6c/12t = 24 threads)
# Utilise 16 nœuds (8 par socket NUMA) pour la topologie hypercube.

@use 16
@topology hypercube
@autoprefix 127.0.0.1:9000

# --- Placement NUMA ---
# Socket/NUMA 0 (8 nœuds sur les 12 threads dispo)
@node 0  @role coordinator @cpu numa=0 core=0
@node 1  @role worker      @cpu numa=0 core=1
@node 2  @role worker      @cpu numa=0 core=2
@node 3  @role worker      @cpu numa=0 core=3
@node 4  @role worker      @cpu numa=0 core=4
# ... (nœuds 5 à 7 sur NUMA 0)
# Socket/NUMA 1 (8 nœuds sur les 12 threads dispo)
@node 8  @role worker      @cpu numa=1 core=0 # Core 0 du deuxième CPU
@node 9  @role worker      @cpu numa=1 core=1
# ... (nœuds 10 à 15 sur NUMA 1)

@job CompileHamon

  # Phase 1: Compilation parallèle (OK)
  @phase CompileCPP by=[0] task="g++ -c Hamon.cpp -o Hamon.o"
  @phase CompileCPP by=[1] task="g++ -c HamonCube.cpp -o HamonCube.o"
  @phase CompileCPP by=[2] task="g++ -c HamonNode.cpp -o HamonNode.o"
  @phase CompileCPP by=[3] task="g++ -c Make.cpp -o Make.o"
  @phase CompileCPP by=[4] task="g++ -c main.cpp -o main.o"
  # Phase 3: Lien final (OK car FS local)
  @phase LinkExecutable to=[0] task="g++ Hamon.o HamonCube.o HamonNode.o Make.o main.o -o hamon -lpthread"

@end