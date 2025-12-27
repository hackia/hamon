# Fichier: make_r710_2cpu.hc (Optimisé)
# Compile Hamon sur un Dell R710 (2x 6c/12t = 24 threads)
# Utilise 16 nœuds (8 par socket NUMA) pour la topologie hypercube.

@use 16
@topology hypercube
@autoprefix 127.0.0.1:9000

# --- Placement NUMA ---
# Socket/NUMA 0
@node 0  @role coordinator @cpu numa=0 core=0
@node 1  @role worker      @cpu numa=0 core=1
@node 2  @role worker      @cpu numa=0 core=2
@node 3  @role worker      @cpu numa=0 core=3
@node 4  @role worker      @cpu numa=0 core=4
@node 5  @role worker      @cpu numa=0 core=5
@node 6  @role worker      @cpu numa=0 core=6
@node 7  @role worker      @cpu numa=0 core=7
# Socket/NUMA 1
@node 8  @role worker      @cpu numa=1 core=0
@node 9  @role worker      @cpu numa=1 core=1
@node 10 @role worker      @cpu numa=1 core=2
@node 11 @role worker      @cpu numa=1 core=3
@node 12 @role worker      @cpu numa=1 core=4
@node 13 @role worker      @cpu numa=1 core=5
@node 14 @role worker      @cpu numa=1 core=6
@node 15 @role worker      @cpu numa=1 core=7

@let CXXFLAGS = "-std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -O2"

@job CompileHamon
  @phase Hamon by=[0] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/Hamon.cpp -o Hamon.o"
  @phase HamonCube by=[1] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/HamonCube.cpp -o HamonCube.o"
  @phase HamonNode by=[2] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/HamonNode.cpp -o HamonNode.o"
  @phase Make by=[3] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/Make.cpp -o Make.o"
  @phase Main by=[0] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c apps/hamon/main.cpp -o main.o"
  @phase LinkExecutable to=[0] task="g++ Hamon.o HamonCube.o HamonNode.o Make.o main.o -o hamon -pthread"
@end