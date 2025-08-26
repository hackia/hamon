cmake_minimum_required(VERSION 4.0)
project(hamon)

set(CMAKE_CXX_STANDARD 26)
# Liste des avertissements que nous voulons activer
set(GCC_WARNING_FLAGS
        -Wall                   # Le grand classique, active beaucoup de warnings utiles
        -Wextra                 # Active encore plus de warnings
        -Wpedantic              # S'assure que le code est strictement standard
        -Wshadow                # Prévient le "shadowing" de variables (ex: une variable locale qui masque une variable globale)
        -Wformat=2              # Vérifications de sécurité sur les chaînes de format (printf, etc.)
        -Wconversion            # Avertit sur les conversions implicites qui peuvent perdre des données
        -Wsign-conversion       # Avertit sur les conversions entre types signés/non-signés
        -Werror                 # Traite tous les warnings comme des erreurs de compilation
)
# Applique ces options à toutes les cibles du projet

add_library(cube STATIC HamonCube.cpp HamonCube.h HamonNode.cpp HamonNode.h)
target_compile_options(cube PRIVATE ${GCC_WARNING_FLAGS})
add_executable(hamon main.cpp)
target_compile_options(hamon PRIVATE ${GCC_WARNING_FLAGS})
target_link_libraries(hamon cube)

install(TARGETS hamon DESTINATION bin)
install(FILES HamonCube.h DESTINATION include)
install(TARGETS cube DESTINATION lib)

# Active la fonctionnalité de test de CMake
enable_testing()

# Télécharge et configure Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Pour les versions plus anciennes de CMake, vous pourriez avoir besoin de GIT_REPOSITORY et GIT_TAG
# GIT_REPOSITORY https://github.com/google/googletest.git
# GIT_TAG    release-1.12.1

FetchContent_MakeAvailable(googletest)

# --- Crée un exécutable de test ---
add_executable(
        hamon_tests
        # Ajoute tes fichiers de test ici
        tests/test_hamon_cube.cpp
        tests/test_hamon_node.cpp
)
target_compile_options(hamon_tests PRIVATE ${GCC_WARNING_FLAGS})
# Lie tes tests à tes bibliothèques et à Google Test
target_link_libraries(
        hamon_tests
        PRIVATE
        cube       # Ta bibliothèque existante
        gtest_main # La bibliothèque Google Test
)

# Ajoute les tests à CTest pour pouvoir les lancer avec `ctest`
include(GoogleTest)
gtest_discover_tests(hamon_tests)
add_custom_command(
        TARGET hamon_tests
        POST_BUILD
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests automatically after build..."
)
