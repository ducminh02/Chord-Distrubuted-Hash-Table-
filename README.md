# Chord-Distrubuted-Hash-Table-
Implementation of Chord (peer-to-peer Distributed Hash Table)

for the program to work as expected the following file need to be downloaded:
https://github.com/lsof-org/lsof

The following progam simulate the protocol Chord for a peer-to-peer distributed table. 
A hash table stores tuples of the form <key, value>. The hash value calculated on the basis of the key (hash(key)) determines in which row of the table the value is stored.
In the simplest case, a hash table is stored on a single server and
server and is used by several clients. For this purpose there are the
functions set() to store or update a value, get() to read a value, and delete() to delete a value.

To run the Program:

1. Use the following command in Unix Shell (to see the activities between the peers)
./launch.sh 

2. Use the 3 functions (Get, Set, Delete)

# Store `cat.jpg` in DHT
./client localhost 4711 SET /pics/cat.jpg < cat.jpg

# Query stored image
./client localhost 4711 GET /pics/cat.jpg > cat2.jpg

# Remove image from DHT
./client localhost 4711 DELETE /pics/cat.jpg
