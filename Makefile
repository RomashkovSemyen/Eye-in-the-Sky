.PHONY: all clean install uninstall

all: earth

clean:
	rm -f elems.txt earth elements *.csv *.o

elements: elements.cpp
	g++ elements.cpp -o $@

elems.txt: elements
	./elements

glad.o: glad.c 
	gcc -c glad.c -I./include -o glad.o

earth_render.o: earth_render.cpp 
	g++ -c earth_render.cpp -I./include -I./stb_headers -o earth_render.o

earth: earth_render.o glad.o elems.txt 
	g++ earth_render.o glad.o -o earth -lglfw -lGL
