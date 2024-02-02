#!/bin/bash
echo -e "\n\033[1;34m-- EMBEDDING ELEMENTS --\033[0;0m\n"
./qDivEmbed elements $(find ./elements -type f | sed 's/^\.\/elements\///g')
echo -e "\n\033[1;34m-- COMPILING --\033[0;0m\n"
# Linux
gcc -g -c qDivClient.c -o lib/qDivClient.o &
gcc -g -c qDivServer.c -o lib/qDivServer.o &
gcc -g -c qDivNoiseSimulator.c -o lib/qDivNoiseSimulator.o &
gcc -g -c qDivAudio.c -o lib/qDivAudio.o &
gcc -g -c qDivWorldGen.c -o lib/qDivWorldGen.o &
gcc -g -c elements.c -o lib/elements.o &
gcc -g -c include/glad.c -o lib/glad.o &
gcc -g -c include/aes.c -o lib/aes.o &
gcc -g -c include/open-simplex-noise.c -o lib/open-simplex-noise.o &
gcc qDivNoiseSimulator.c qDivWorldGen.c elements.c include/open-simplex-noise.c include/glad.c -lglfw -lGL -ldl -lm -o qDivNoiseSimulator -mcmodel=large &
# Windows
x86_64-w64-mingw32-gcc -c qDivClient.c -o lib/qDivClient.exe.o &
x86_64-w64-mingw32-gcc -c qDivServer.c -o lib/qDivServer.exe.o &
x86_64-w64-mingw32-gcc -c qDivAudio.c -o lib/qDivAudio.exe.o &
x86_64-w64-mingw32-gcc -c qDivWorldGen.c -o lib/qDivWorldGen.exe.o &
x86_64-w64-mingw32-gcc -c elements.c -o lib/elements.exe.o &
x86_64-w64-mingw32-gcc -c include/glad.c -o lib/glad.exe.o &
x86_64-w64-mingw32-gcc -c include/aes.c -o lib/aes.exe.o &
x86_64-w64-mingw32-gcc -c include/open-simplex-noise.c -o lib/open-simplex-noise.exe.o &
wait
echo -e "\n\033[1;34m-- LINKING --\033[0;0m\n"
# Linux
gcc lib/qDivClient.o lib/qDivAudio.o lib/elements.o lib/open-simplex-noise.o lib/glad.o lib/aes.o -lglfw -lGL -ldl -lpthread -lm -o qDivClient &
gcc lib/qDivServer.o lib/qDivWorldGen.o lib/open-simplex-noise.o lib/aes.o -lpthread -lm -o qDivServer &
# Windows
x86_64-w64-mingw32-gcc lib/qDivClient.exe.o lib/qDivAudio.exe.o lib/qDivWorldGen.exe.o lib/elements.exe.o lib/open-simplex-noise.exe.o lib/glad.exe.o lib/aes.exe.o -lwsock32 -lws2_32 -lglfw3 -lopengl32 -lgdi32 -ldl -lm -lmingw32 -o qDivClient.exe -Wl,--subsystem,windows,-Bstatic -lwinpthread &
x86_64-w64-mingw32-gcc lib/qDivServer.exe.o lib/qDivWorldGen.exe.o lib/open-simplex-noise.exe.o lib/aes.exe.o -lwsock32 -lws2_32 -lm -lmingw32 -o qDivServer.exe -Wl,-Bstatic -lwinpthread &
wait
rm lib/elements.o
