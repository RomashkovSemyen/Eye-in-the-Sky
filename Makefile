.PHONY: all clean install uninstall

all: earth

clean:
	rm *.txt earth elements elems.txt elements *.csv

install:
	install ./
