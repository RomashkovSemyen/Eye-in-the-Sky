.PHONY: all clean install uninstall

all: earth

clean:
	rm elems.txt earth elements *.csv *.o

elements: elements.cpp
	g++ elements.cpp -o $@

elems.txt: elements
	./elements

glad.o: glad.c
	gcc -c glad.c -I./include -o glad.o

earth: earth_render.cpp glad.o elems.txt
	g++ earth_render.cpp glad.o -I./include -I./stb_headers -o earth -lglfw -lGL
