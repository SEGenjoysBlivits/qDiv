#!/bin/bash
gcc qDivEmbed.c -o qDivEmbed
./qDivEmbed
gcc qDivClient.c include/glad.c include/ecdh.c -lglfw -lGL -ldl -lpthread -lm -o qDivClient &
gcc qDivServer.c include/open-simplex-noise.c include/ecdh.c -lpthread -lm -o qDivServer &
x86_64-w64-mingw32-gcc qDivClient.c include/glad.c include/ecdh.c -lmingw32 -lwsock32 -lws2_32 -lglfw3 -lopengl32 -lgdi32 -ldl -lm -o qDivClient.exe -Wl,-Bstatic -lwinpthread &
x86_64-w64-mingw32-gcc qDivServer.c include/open-simplex-noise.c include/ecdh.c -lmingw32 -lwsock32 -lws2_32 -lm -lmingw32 -o qDivServer.exe -Wl,-Bstatic -lwinpthread &
wait
rm qDivClientElements.h
