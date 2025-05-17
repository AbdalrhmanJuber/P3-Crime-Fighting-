#!/bin/bash
# This script builds and runs the fixed version of the crime simulation

echo "Building fixed version of crime simulation..."
cd "$(dirname "$0")"
make clean

# Compile fixed version explicitly
mkdir -p build
gcc -Wall -g -pthread -Iinclude -c src/config.c -o build/config.o
gcc -Wall -g -pthread -Iinclude -c src/gang.c -o build/gang.o
gcc -Wall -g -pthread -Iinclude -c src/ipc.c -o build/ipc.o
gcc -Wall -g -pthread -Iinclude -c src/main_fixed.c -o build/main.o
gcc -Wall -g -pthread -Iinclude -c src/police.c -o build/police.o
gcc -Wall -g -pthread -Iinclude -c src/police_helper.c -o build/police_helper.o
gcc -Wall -g -pthread -Iinclude -c src/thread_safe_drawing.c -o build/thread_safe_drawing.o
gcc -Wall -g -pthread -Iinclude -c src/utils.c -o build/utils.o
gcc -Wall -g -pthread -Iinclude -c src/visualization_fixed.c -o build/visualization.o

# Link everything
gcc -o build/crime_sim build/*.o -lGL -lGLU -lglut -lm

echo "Running crime simulation..."
./build/crime_sim config/simulation_config.txt

echo "Done!"
