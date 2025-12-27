@use 4
@topology hypercube
@autoprefix 127.0.0.1:9000

@node 0 @role coordinator @cpu numa=0 core=0
@node 1 @role worker @cpu numa=0 core=1
@node 2 @role worker @cpu numa=0 core=2
@node 3 @role worker @cpu numa=0 core=3

@job CompileHamon
  @phase Hamon by=[0] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/Hamon.cpp -o Hamon.o"
  @phase HamonCube by=[1] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/HamonCube.cpp -o HamonCube.o"
  @phase HamonNode by=[2] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/HamonNode.cpp -o HamonNode.o"
  @phase Make by=[3] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c src/Make.cpp -o Make.o"
  @phase Main by=[0] task="g++ -std=c++26 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion -Werror -Iinclude -c apps/hamon/main.cpp -o main.o"
  @phase LinkExecutable to=[0] task="g++ Hamon.o HamonCube.o HamonNode.o Make.o main.o -o hamon"
@end