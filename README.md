## Introduction

We developed a complicated full stack application based on socket-based single-server-multi-client connection. The front-end of this application is Qt6 and back-end is SQLite3/MongoDB(experimental). Serveral highlights of this application are:

* Modern C++20
* Multiplexing with epoll (linux only) to achieve pseudo-concurrency
* Multi-threading & vectorization
* A variety of database selection


## Goal & Vision

The aim of this full-stack application is to provide a smooth and robust (high scalability & low latency) user experience while maintaining low memory occupation at the same time. We leveraged epoll connection to achieve faster pseudo-concurrency and lots of move operations to avoid unnecessary copy and reduce memory cost. We applied vectorization to obtain messages faster. We explicitly added clustered indexes in our relational database to achieve faster queries.

Other than performance, we also take safety into consideration. We had pragma key credentials in the database to avoid sensitive information leakage. We also had encrypted TCP connection with digital certificate and private key verification. 


## Dependencies & Requirements

* CMake 3.25
* g++-10

* String formatting tool: [fmt](https://github.com/fmtlib/fmt)
* JSON formatting tool: [json](https://github.com/nlohmann/json)
* Database: [SQLite3](https://github.com/SRombauts/SQLiteCpp/tree/master/sqlite3)
* Parallelism: [openmp](https://www.openmp.org/resources/)



## Quick Start

Compile server with CMakeLists.txt.

```bash
cmake CMakeLists.txt
make
```

Execute with

```bash
cd bin/
./cplusplusproject2022fall
```




## Part of C++ new features

* Concept & Requires & template
* Structural binding
* Chrono
* Constexpr
* Smart pointers
* std::as_const()
* Final
* Variant
* Explicit


## Acknowledge

We thank the assistance and guidance from Prof. Bjarne Stroustrup.