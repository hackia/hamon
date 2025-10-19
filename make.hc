@use 4
@topology hypercube
@autoprefix 127.0.0.1:9000

@node 0 @role coordinator @cpu numa=0 core=0
@node 1 @role worker @cpu numa=0 core=1
@node 2 @role worker @cpu numa=0 core=2
@node 3 @role worker @cpu numa=0 core=3

@job CompileHamon
  @phase CompileCPP by=[0] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -c Hamon.cpp -o Hamon.o"
  @phase CompileCPP by=[1] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -c HamonCube.cpp -o HamonCube.o"
  @phase CompileCPP by=[2] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -c HamonNode.cpp -o HamonNode.o"
  @phase CompileCPP by=[2] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -c Make.cpp -o Make.o"
  @phase CompileCPP by=[3] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -c main.cpp -o main.o"
  @phase LinkExecutable to=[0] task="g++ *.o -o hamon"
@end