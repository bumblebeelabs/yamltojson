CFLAGS=`pkg-config --cflags json-glib-1.0`
LFLAGS=`pkg-config --libs json-glib-1.0` -lyaml

yamltojson: yamltojson.o
	gcc $(LFLAGS) yamltojson.o -o yamljson

yamltojson.o: yamltojson.c
	gcc -c $(CFLAGS) yamltojson.c 

clean:
	rm yamljson
	rm *.o
