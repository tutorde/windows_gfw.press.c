Build:

open mingw64.exe    cd windows_gfw.press.c-master

gcc src/client.c src/encrypt.c -ansi -O3 -I/mingw64/include -lwsock32 -lws2_32 -lssl -lcrypto -lgdi32 -lpthread -static -o gfwpress64.exe -Wall



open mingw32.exe    cd windows_gfw.press.c-master

gcc src/client.c src/encrypt.c -ansi -O3 -I/mingw32/include -lwsock32 -lws2_32 -lssl -lcrypto -lgdi32 -lpthread -static -o gfwpress32.exe -Wall
