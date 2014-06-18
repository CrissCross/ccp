# Concurrent C Programming Seminar, ZHAW 2014
## POSIX Shared Memory File Server
Lecturer: Nico Schottelius
Student: Cristoffel Gehring
### Server:
Compile:
```
$ make
```
Start server:
```
$ ./fserver_app
```
Stop server:
```
$ ./pkill fserver_app
```
### Client
Compile:
```
$ make tcp_client
```
Run client:
```
$ ./tcp_client < file_with_commands
```
Example:

```
$ ./tcp_client < test/listfiles
```
### Command Files
List of commands to be send to the server. Commands must be seperated by a newline.
Example:

```
LIST\n

CREATE Datei1 5\n
asdffd

READ Datei1\n

```

