#!/bin/bash
echo -e "\n\n\033[1;34m-- EMBEDDING ELEMENTS --\033[0;0m\n\n"
./qDivEmbed elements $(find ./elements -type f | sed 's/^\.\/elements\///g')
echo -e "\n\n\033[1;34m-- COMPILING --\033[0;0m\n\n"
# Linux
gcc -c qDivClient.c -o lib/qDivClient.o &
gcc -c qDivServer.c -o lib/qDivServer.o &
gcc -c qDivAudio.c -o lib/qDivAudio.o &
gcc -c qDivLanguage.c -o lib/qDivLanguage.o &
gcc -c elements.c -o lib/elements.o &
gcc -c include/glad.c -o lib/glad.o &
gcc -c include/ecdh.c -o lib/ecdh.o &
gcc -c include/open-simplex-noise.c -o lib/open-simplex-noise.o &
# Windows
x86_64-w64-mingw32-gcc -c qDivClient.c -o lib/qDivClient.exe.o &
x86_64-w64-mingw32-gcc -c qDivServer.c -o lib/qDivServer.exe.o &
x86_64-w64-mingw32-gcc -c qDivAudio.c -o lib/qDivAudio.exe.o &
x86_64-w64-mingw32-gcc -c qDivLanguage.c -o lib/qDivLanguage.exe.o &
x86_64-w64-mingw32-gcc -c elements.c -o lib/elements.exe.o &
x86_64-w64-mingw32-gcc -c include/glad.c -o lib/glad.exe.o &
x86_64-w64-mingw32-gcc -c include/ecdh.c -o lib/ecdh.exe.o &
x86_64-w64-mingw32-gcc -c include/open-simplex-noise.c -o lib/open-simplex-noise.exe.o &
wait
rm elements.c
echo -e "\n\n\033[1;34m-- LINKING --\033[0;0m\n\n"
# Linux
gcc lib/qDivClient.o lib/qDivAudio.o lib/qDivLanguage.o lib/elements.o lib/glad.o lib/ecdh.o -lglfw -lGL -ldl -lpthread -lm -o qDivClient &
gcc lib/qDivServer.o lib/ecdh.o lib/open-simplex-noise.o -lpthread -lm -o qDivServer &
# Windows
x86_64-w64-mingw32-gcc lib/qDivClient.exe.o lib/qDivAudio.exe.o lib/qDivLanguage.exe.o lib/elements.exe.o lib/glad.exe.o lib/ecdh.exe.o -lwsock32 -lws2_32 -lglfw3 -lopengl32 -lgdi32 -ldl -lm -lmingw32 -o qDivClient.exe -Wl,-Bstatic -lwinpthread &
x86_64-w64-mingw32-gcc lib/qDivServer.exe.o lib/ecdh.exe.o lib/open-simplex-noise.exe.o -lwsock32 -lws2_32 -lm -lmingw32 -o qDivServer.exe -Wl,-Bstatic -lwinpthread &
wait
rm lib/elements.o
