# System_Programming_Stock_Server_Project

This repository contains a concurrent stock server project built for a system programming course. The project extends the classic echo server structure into a multi-client stock trading service that supports `show`, `buy`, `sell`, and `exit` commands while keeping stock state persistent in `stock.txt`.

The original course report is excluded from the public repository because it contains personal course metadata.

## Overview

The project implements the same stock server problem with two concurrency models:

- `task1`: event-driven server using `select()`
- `task2`: thread-based server using a master thread, a shared descriptor buffer, and a worker thread pool

Both versions:

- load stock data from `stock.txt` into an in-memory binary search tree
- process client commands over TCP sockets
- update stock quantities for buy and sell requests
- return the required success or failure messages
- write updated stock data back to `stock.txt` on `SIGINT`

## Key Engineering Points

- Event-driven concurrency with `select()` for multiplexed client handling
- Thread-pool based concurrency with pthread workers and a producer-consumer buffer
- Fine-grained synchronization using a readers-writers approach on stock tree nodes
- Persistent server state backed by file I/O
- Comparative performance analysis across client counts and workload patterns

## Project Structure

```text
.
|-- task1
|   |-- stockserver.c
|   |-- stockclient.c
|   |-- multiclient.c
|   |-- echo.c
|   |-- csapp.c
|   |-- csapp.h
|   |-- stock.txt
|   `-- Makefile
`-- task2
    |-- stockserver.c
    |-- stockclient.c
    |-- multiclient.c
    |-- echo.c
    |-- csapp.c
    |-- csapp.h
    |-- stock.txt
    `-- Makefile
```

## Task 1: Event-Driven Server

`task1` implements a single-process concurrent server with `select()`. The server tracks the listening socket and connected client descriptors in an `fd_set`, accepts new connections when the listening socket becomes ready, and processes client requests only on ready descriptors.

This version is designed around I/O multiplexing and avoids thread-management overhead while still handling multiple active clients.

## Task 2: Thread-Based Server

`task2` implements a concurrent server with a worker thread pool. The main thread accepts incoming client connections and inserts descriptors into a shared bounded buffer. Worker threads remove descriptors from the buffer and serve clients concurrently.

This version uses semaphore-based synchronization both for the shared connection buffer and for stock-node level readers-writers control.

## Build and Run

Each task is self-contained and can be built separately:

```bash
cd task1
make
./stockserver <port>
```

For the thread-based version:

```bash
cd task2
make
./stockserver <port>
```

Example client programs included in each task:

```bash
./stockclient <host> <port>
./multiclient <host> <port> <client_num>
```

## Notes

- The code is intended for Linux-like environments such as the course server.
- The repository keeps the original task split so the two concurrency approaches can be compared directly.
- The performance report itself is not published here, but the implementation reflects the same evaluation setup described in the course project.
