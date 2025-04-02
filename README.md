# Proyecto: Chat WebSocket en C (Servidor y Cliente)

Este proyecto implementa un sistema de chat completo utilizando el protocolo WebSocket en C. Incluye tanto el **servidor** como el **cliente**, permitiendo a múltiples usuarios conectarse, enviar mensajes, cambiar su estado, consultar usuarios y obtener el historial de mensajes.

---

## Características

### 🚀 Servidor
- Maneja conexiones WebSocket con handshake manual (HTTP Upgrade).
- Soporta hasta 100 usuarios simultáneos.
- Asigna estado a los usuarios (ACTIVO, OCUPADO, INACTIVO, DESCONECTADO).
- Almacena historial de mensajes por chat entre usuarios (hasta 100 mensajes por chat).
- Permite:
  - Listar usuarios conectados (mensaje tipo 1).
  - Obtener información de un usuario (mensaje tipo 2).
  - Cambiar estatus (mensaje tipo 3).
  - Enviar mensaje (mensaje tipo 4).
  - Solicitar historial (mensaje tipo 5).
- Envia notificaciones tipo broadcast cuando un usuario cambia de estatus.

### 📲 Cliente
- Realiza handshake WebSocket con el servidor.
- Presenta un menú interactivo con opciones para:
  - Listar usuarios.
  - Obtener información de un usuario.
  - Cambiar su estatus.
  - Enviar mensajes individuales o al chat general.
  - Solicitar historial de conversaciones.
- Decodifica y enmascara mensajes WebSocket conforme al protocolo.

---

## Estructura del Proyecto

```
├── server.c     // Implementación completa del servidor WebSocket
├── server     // Ejecutable del server
├── client.c     // Implementación completa del servidor WebSocket
├── client     // Ejecutable del cliente
├── README.md    // Este archivo
```

---

## Compilación

### Requisitos:
- GCC
- OpenSSL (para SHA1 y base64)

```bash
sudo apt install libssl-dev
```

### Compilar Servidor:
```bash
gcc server.c -o server -lpthread -lssl -lcrypto
```

### Compilar Cliente:
```bash
gcc client.c -o client
```

---

## Ejecución

### Iniciar servidor:
```bash
./server <puerto>
```
Ejemplo:
```bash
./server 8080
```

### Iniciar cliente:
```bash
./client <host> <puerto> <nombre>
```
Ejemplo:
```bash
./client localhost 8080 juan
```

---

## Protocolo de Comunicación

Se utilizan frames binarios y de texto del protocolo WebSocket para enviar mensajes con los siguientes tipos:

| Tipo | Significado                       | Dirección |
|------|-----------------------------------|------------|
| 1    | Solicitar lista de usuarios       | Cliente ➔ Servidor |
| 2    | Solicitar información de usuario  | Cliente ➔ Servidor |
| 3    | Cambiar estatus                   | Cliente ➔ Servidor |
| 4    | Enviar mensaje                    | Cliente ➔ Servidor |
| 5    | Solicitar historial               | Cliente ➔ Servidor |
| 50   | Mensaje de error                  | Servidor ➔ Cliente |
| 51   | Lista de usuarios                 | Servidor ➔ Cliente |
| 52   | Info de usuario                   | Servidor ➔ Cliente |
| 53   | Bienvenida                        | Servidor ➔ Cliente |
| 54   | Cambio de estatus (broadcast)     | Servidor ➔ Cliente |
| 55   | Mensaje recibido                  | Servidor ➔ Cliente |
| 56   | Historial de mensajes             | Servidor ➔ Cliente |

---

## Estados de Usuario

| Código | Estado      |
|--------|-------------|
| 0      | DESCONECTADO |
| 1      | ACTIVO       |
| 2      | OCUPADO      |
| 3      | INACTIVO     |

---

## Seguridad
- Se usa `Sec-WebSocket-Key` y `Sec-WebSocket-Accept` para handshake conforme a RFC 6455.
- Los mensajes del cliente son enmascarados como lo exige el protocolo WebSocket.

---

## Autores
- Proyecto desarrollado como parte de la clase de **Sistemas Operativos**.
- Sofía Velásquez : https://github.com/Sofiamishel2003 
- José Rodrigo Marchena : https://github.com/MarchMol 
- Nicolle Gordillo : https://github.com/nicollegordillo 

---

## Recursos
- RFC 6455 - The WebSocket Protocol: https://datatracker.ietf.org/doc/html/rfc6455
- WebSocket Handshake Explained: https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_server
- Protocolo WebSocket para el proyecto: https://docs.google.com/document/d/1LiqxPx2Z1Ptg1pnI9gBY7opORUvIHQqn9DgjF4AMgMI/edit?usp=sharing
- ### Interfaz gráfica en Qt c++ para el chat https://github.com/Sofiamishel2003/Interfaz_Grafica_Chat.git
---


