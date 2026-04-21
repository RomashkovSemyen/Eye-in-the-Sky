.PHONY: all clean install uninstall

all: earth elements XYtoPL draw_line

install:
	sudo install ./earth /usr/local/bin

unistall:
	rm /usr/local/bin/earth

clean:
	rm -f elems.txt earth draw_line XYtoPL elements *.csv *.o points.txt picture.jpg
 
elements: elements.cpp
	g++ elements.cpp -o $@

glad.o: glad.c 
	gcc -c glad.c -I./include -o glad.o

earth_render.o: earth_render.cpp 
	g++ -c earth_render.cpp -I./include -I./stb_headers -o earth_render.o

earth: earth_render.o glad.o 
	g++ earth_render.o glad.o -o earth -lglfw -lGL

draw_line: draw_line.cpp
	g++ draw_line.cpp -o draw_line -I./stb_headers

XYtoPL: XYtoPL.cpp
	g++ XYtoPL.cpp -o XYtoPL

