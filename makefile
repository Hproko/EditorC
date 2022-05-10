#Henrique Prokopenko      GRR20186712

CFLAGS = -Wall #-DDEBUG

LDLIBS = -lm -pthread

client_objects = client.o client_functions.o socket_functions.o functions.o

server_objects = server.o socket_functions.o functions.o server_functions.o


all: server client


server: $(server_objects)
	gcc $(server_objects) -o server $(LDLIBS)



client: $(client_objects)
	gcc $(client_objects) -o client $(LDLIBS)


server_functions.o: server_functions.c server_functions.h
	gcc -c server_functions.c $(CFLAGS) $(LDLIBS)


server.o: server.c
	gcc -c server.c $(CFLAGS)

client.o: client.c 
	gcc  -c client.c $(CFLAGS)

socket_functions.o: socket_functions.c socket_functions.h
	gcc -c socket_functions.c $(CFLAGS)


client_functions.o: client_functions.h client_functions.c
	gcc -c client_functions.c $(CFLAGS) $(LDLIBS)

functions.o: functions.c functions.h
	gcc -c functions.c $(CFLAGS)

clean: 
	-rm -f *~ *.o


purge: clean
	-rm -f server client
