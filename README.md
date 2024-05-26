# Chat Cliente-Servidor 

## Descripción
Proyecto Chat, Sistemas Operativos. Desarrollo de un chat en el lenguajde de programación C usando un protocolo de comunicación TCP. El sistema se divide en dos componentes principales: un servidor y múltiples clientes, permitiendo comunicación y manejo de múltiples sesiones de usuarios simultáneamente.

<img src="https://miro.medium.com/v2/resize:fit:1400/0*oPflQ565ZD-g_uJ9.png" height="150">

### Servidor
El servidor mantiene una lista de todos los clientes conectados y atiende sus peticiones usando multithreading. Gestiona funciones como el registro y liberación de usuarios, listado de usuarios conectados, manejo de estados de usuarios, y permite comunicaciones tanto en broadcast como mensajes directos.

### Cliente
El cliente permite a los usuarios conectarse al servidor, enviar y recibir mensajes, cambiar de estado, y consultar información sobre otros usuarios conectados. Cada cliente maneja su propia interfaz de usuario.

## Requisitos
- Linux OS para ejecución del servidor
- C Compiler (GCC recomendado)
- Conexión a internet para comunicación entre cliente y servidor

## Configuración e Instalación
Para configurar y ejecutar el servidor y cliente, sigue estos pasos:
```bash
# Clona este repositorio
$ git clone https://github.com/bl33h/clientServerChat

# Abre el proyecto
$ cd src

# Compilar el cliente y servidor
$ gcc -o server server.c chat.pb-c.c -lprotobuf-c -pthread
$ gcc -o client client.c chat.pb-c.c -lprotobuf-c -pthread

# Ejecutar el servidor, especificando el puerto
$ ./server <port>

# Ejecutar el cliente con la IP del servidor y el número de puerto
$ ./client <user> <IP> <port>
```

## Uso
### Servidor
Permite ver la actividad que tienen los usuarios:
```
Server started on port 8080

(*) New connection: fer (IP: 127.0.0.1)
(*) New connection: meli (IP: 127.0.0.1)

> Broadcast Message from fer: hola a todos!

updated status from meli from ONLINE to OFFLINE

(*) Client disconnected: meli (IP: 127.0.0.1)
```

### Cliente
Permite la interacción de los usuarios:
```
Registered as meli. Welcome to the chat!

----------------------------------
1. Enter the chatroom
2. Change Status
3. View connected users
4. See user information
5. Help
6. Exit
----------------------------------
Select an option: 1

------ Chatroom Menu ------
1. Chat with everyone (broadcast)
2. Send a direct message
3. Exit the chatroom
----------------------------------
```
