
output: main.o
	g++ -O3 -o Floyd_Steinberg_Dithering main.o -lX11 -lGL -lpthread -lpng -lstdc++fs

main.o: main.cpp
	g++ -c main.cpp -std=c++17

clean: 
	rm *.o