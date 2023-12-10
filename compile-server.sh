#!/bin/bash
echo "–– Linux ––

"
gcc qDivServer.c include/open-simplex-noise.c -lpthread -lm -o qDivServer
echo "

–– Windows ––

"
x86_64-w64-mingw32-gcc qDivServer.c include/open-simplex-noise.c -lmingw32 -lwsock32 -lws2_32 -lm -lmingw32 -o qDivServer.exe -Wl,-Bstatic -lwinpthread
