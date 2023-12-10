#!/bin/bash
gcc qDivEmbed.c -o qDivEmbed
./qDivEmbed
echo "

–– Linux ––

"
gcc qDivClient.c include/glad.c -lglfw -lGL -lpthread -lm -o qDivClient
echo "

–– Windows ––

"
x86_64-w64-mingw32-gcc qDivClient.c include/glad.c -lmingw32 -lwsock32 -lws2_32 -lglfw3 -lopengl32 -lgdi32 -ldl -lm -o qDivClient.exe -Wl,-Bstatic -lwinpthread

rm elements.h
