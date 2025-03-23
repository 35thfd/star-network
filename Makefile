all: satellite sta_main

satellite: satellite.o
	g++  satellite.o -o satellite

sta_main: sta_main.o station.o
	g++ sta_main.o station.o -o sta_main -lpthread

sta_main.o: sta_main.cpp station.h satellite.h
	g++ -Wall -std=c++11 -c sta_main.cpp

station.o: station.cpp station.h
	g++ -Wall -std=c++11 -c station.cpp

satellite.o: satellite.cpp satellite.h
	g++ -Wall -std=c++11 -c satellite.cpp

clean:
	rm -f *.o satellite sta_main