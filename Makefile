.PHONY: all clean install uninstall

all: earth trass

install:
	sudo install ./earth /usr/local/bin

unistall:
	rm /usr/local/bin/earth

clean:
	rm -f elems.txt earth draw_line XYtoPL elements *.csv *.o points.txt picture.jpg
 
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

satellite_1_orbit.csv: earth
	./earth 12 1

draw_line: draw_line.cpp
	g++ draw_line.cpp -o draw_line -I./stb_headers

points.txt: satellite_1_orbit.csv XYtoPL
	./XYtoPL satellite_1_orbit.csv

XYtoPL: XYtoPL.cpp
	g++ XYtoPL.cpp -o XYtoPL

trass: points.txt draw_line input.jpg
	./draw_line input.jpg points.txt 1
