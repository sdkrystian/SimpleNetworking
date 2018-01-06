# Simple Networking
Simple server that supports multiple clients. 

# Use

## Creating the Server
Create a server, providing a port and optionally a list of callbacks (callbacks can also be added after creating the object).
Call the start function, the server will start.

## Creating the Client
Create a client, providing an IP and port optionally a list of callbacks (again, callbacks can also be added after creating the object).
Call the connect function, and the client will attempt to connect to the given address.

## Sending data
Data can be sent via the send function on the client and the server, callbacks will be called automatically on both.

###### Packets
Data is send as a packet, clients sending ClientPackets to the server, and the server sending ServerPackets to the clients.
Clients are identified by a clientID, so the server must specify which client to send information to in the send function, or may call the sendall function to send to all clients.
Packets must be given a request or command so they can be handled by a callback.

###### Datatypes
The datatypes that can be sent are:
```
  arrays
  doubles
  floats
  ints
  longs
  strings
```

Packets can contain more than one variable, all of which are added via the addValue and addArray functions, which take a variable name and a variable as arguments.

## Recieving data

A callback can be specified to handle a request or command, which will be called upon when the client sends the request or server sends the command.
The PacketData will be passed to the callback, which can be read from using the following functions:
```
  getArray(name)
  getDouble(name)
  getFloat(name)
  getInt(name)
  getLong(name)
  getString(name)
```
name being the name of the variable.


