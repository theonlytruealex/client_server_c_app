# Sistem de Abonare Client-Server

## Prezentare generala

Aceasta aplicatie implementeaza un sistem de abonare care faciliteaza transmiterea informatiilor de la server catre clienti. Sistemul contine doua componente principale:

1. **Server**: Primeste mesaje prin UDP despre diverse topic-uri si le transmite clientilor abonati.
2. **Client/Abonat**: Permite userilor sa se aboneze la topic-uri si sa primeasca update-uri in timp real.

## Caracteristici principale

- **Abonare cu wildcards**: Te poti abona la mai multe topic-uri folosind caractere speciale (`+` sau `*`)
- **Tipuri multiple de date**: Suport pentru mesaje INT, SHORT_REAL, FLOAT si STRING
- **Conexiuni concurente**: Poate gestiona multi clienti simultan folosind epoll
- **Protocol aplicatie**: Protocol custom peste TCP care asigura transmiterea completa a mesajelor
- **Identificare clienti**: Fiecare client are ID unic
- **Logare**: Afisare log-uri pentru conectari/deconectari clienti

## Arhitectura tehnica

### Protocol de aplicatie

Aplicatia foloseste un protocol peste TCP pentru livrarea mesajelor:

- **Structura mesajelor**: Fiecare mesaj incepe cu un camp `uint32_t` pentru lungimea restului mesajului
- **Transmitere completa**: Functia `send_all` asigura livrarea prin apelarea repetata a functiei `send` pana cand toti octetii sunt transmisi
- **Receptie completa**: Functia `recv_all` primeste intai campul de lungime, apoi foloseste valoarea pentru a sti cati octeti mai trebuie primiti

### Flow conexiuni

1. **Identificare client**:
   - Clientul se conecteaza la server prin TCP
   - Clientul trimite ID-ul folosind structura `uid`
   - Serverul verifica daca ID-ul este unic si raspunde cu un cod (1=ok, 0=deja folosit)

2. **Gestionare abonamente**:
   - Clientii trimit structuri `subscription` cu `sub_state` setat la 1 (subscribe) sau 0 (unsubscribe)
   - Wildcards (`*` sau `+`) pot fi folosite pentru a te abona la mai multe topic-uri simultan, `*` inlocuind oricate niveluri, minim unul, iar `+` inlocuind exact un nivel

3. **Publicare mesaje**:
   - Serverul primeste mesaje ca structuri `udp_payload`
   - Serverul proceseaza topic-ul si payload-ul, apoi trimite mesajul tuturor clientilor abonati relevanti
   - Topic-urile sunt stocate intr-un hashmap care leaga ID-urile clientilor de topic-urile la care sunt abonati
   - Conexiunile clientilor sunt tinute in doua hashmap-uri: `id_to_fd` si `fd_to_id`

4. **Primire mesaje**:
   - Abonatii primesc structuri `topic_update` prin TCP folosind functia `recv_all`
   - Mesajele includ metadata despre sursa lor, pe langa informatiile despre topic

### Multiplexare I/O

Serverul si clientul folosesc epoll pentru multiplexare I/O eficienta:

#### Server
- Monitorizeaza socket UDP pentru mesaje primite
- Monitorizeaza socket TCP pentru noi conexiuni
- Monitorizeaza stdin pentru comanda `exit`
- Monitorizeaza conexiunile clientilor pentru comenzi si deconectari

#### Client
- Monitorizeaza socket TCP pentru update-uri de topic-uri
- Monitorizeaza stdin pentru comenzile `exit`, `subscribe` si `unsubscribe`

## Structuri de date

Sistemul foloseste urmatoarele structuri pentru comunicare:

```c
// ID client
typedef struct __attribute__((packed)) uid {
    uint32_t size;
    char id[11];
} uid;

// Info topic
typedef struct __attribute__((packed)) topic_body {
    uint32_t size;
    char cells[51];
} topic_body;

// Update mesaj trimis clientilor
typedef struct __attribute__((packed)) topic_update {
    uint32_t len;
    char preambule[30];
    topic_body topic;
    uint32_t payload_len;
    char payload[1520];
} topic_update;

// Cerere de abonare
typedef struct __attribute__((packed)) subscription {
    uint32_t len;
    uint8_t sub_state;
    topic_body topic;
} subscription;

// Mesaj primit prin UDP
typedef struct __attribute__((packed)) udp_payload {
    char topic[50];
    uint8_t ptype;
    char message[1501];
} udp_payload;
```

Toate structurile trimise peste TCP au marimea specificata printr-un `uint32_t` in care se retine lungimea restului mesajului.


## Tratare erori si implementare

- Apelurile de sistem au mesaje descriptive prin functia `error_exit`
- Erorile de input sunt raportate la `STDERR`
- Serverul foloseste `unordered_map` si `unordered_set` pentru a stoca eficient:
  - Asocierea intre ID client si file descriptor (`id_to_fd`)
  - Asocierea intre file descriptor si ID client (`fd_to_id`)
  - Asocierea intre ID client si lista de topic-uri la care e abonat (`subscriptions`)
- Pentru manipularea eficienta a socket-urilor:
  - Dezactivarea algoritmului Nagle folosind `TCP_NODELAY`
- Buffering-ul la afisare este dezactivat

## Functii utilitare

Sistemul include functii in `utils.cpp` si `utils.h`:

- `error_exit()`: Tratare unitara a erorilor
- `add_to_epoll()`: Adaugare file descriptor la epoll
- `send_all()`: Transmitere completa a datelor prin TCP
- `recv_all()`: Receptie completa a datelor prin TCP

## Compilare si utilizare

### Comenzi de compilare
```bash
# Compilare completa
make all

# Compilare doar server
make server

# Compilare doar client
make subscriber

# Curatare fisiere compilate
make clean
```

### Pornire server
```bash
./server <PORT>
```
sau
```bash
make run_server PORT=<PORT>
```

### Pornire client
```bash
./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>
```
sau
```bash
make run_subscriber ID_CLIENT=<ID> IP_SERVER=<IP> PORT_SERVER=<PORT>
```

### Comenzi client
- `subscribe <TOPIC>` - Abonare la un topic
- `unsubscribe <TOPIC>` - Dezabonare de la un topic
- `exit` - Deconectare de la server