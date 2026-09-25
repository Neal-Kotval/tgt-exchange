# TGT Exchange

A C++20 order book for the Trading at Georgia Tech exchange warm-up project. Handles limit orders for one symbol and keeps everything in memory. Written by Neal Kotval

- [x] Matching Engine
- [x] Drogon HTTP and WebSocket

## Setup on macOS

You need a C++20 compiler and CMake 3.20 or newer. Development has been on macOS with Apple Clang.

If Apple's command-line tools are missing:

```sh
xcode-select --install
```

With [Homebrew](https://brew.sh/) installed, install CMake:

```sh
brew install cmake
```

Drogon is needed for the server, but not for the core tests:

```sh
brew install drogon
```

Check the tools:

```sh
clang++ --version
cmake --version
drogon_ctl --version
```

For a fresh checkout, run these from the directory where you want the project:

```sh
git clone https://github.com/Neal-Kotval/tgt-exchange.git
cd tgt-exchange
```

If you already have the project open, use its existing folder instead of cloning it again. The commands below assume you are in the folder containing `CMakeLists.txt`.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The first command configures the build, the second compiles it, and the third runs the tests. Compiler warnings are enabled with `-Wall -Wextra -Wpedantic`.

Run the test executable directly to see its success message:

```sh
./build/core_tests
```

Expected output:

```text
All core checks passed
```

CTest reports one test because all the checks currently live in one executable.

Run the server:

```sh
./build/exchange_server
```

It listens at `127.0.0.1:8080`. Stop it with `Control-C`.

After editing source files, rebuild and test again:

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For verbose test output:

```sh
ctest --test-dir build --verbose
```

To force a full rebuild without deleting the build folder:

```sh
cmake --build build --clean-first -j
```

A separate release build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

The earlier examples in `main.cpp` can still be compiled separately:

```sh
clang++ -std=c++20 -Wall -Wextra -Wpedantic main.cpp order_book.cpp -o build/examples
./build/examples
```

Run this after creating `build` with the CMake setup above. The examples are separate from the full core test executable.


## API

| Method | Path | Purpose |
| --- | --- | --- |
| `POST` | `/orders` | Submit a limit order |
| `DELETE` | `/orders/{id}` | Cancel a resting order |
| `GET` | `/book` | Read the best five levels |
| WebSocket | `/marketdata` | Receive book and trade updates |

Orders use JSON such as:

```json
{"side":"buy","price":1025,"quantity":3}
```

The WebSocket URL is `ws://127.0.0.1:8080/marketdata`.

## Book operations

- `submit(side, price, quantity)` matches the incoming order and stores any remainder.
- `cancel(order_id)` removes a resting order and returns `true`. It returns `false` for unknown, already cancelled, or fully filled orders.
- `snapshot(depth)` returns price levels up to depth levels, with quantities summed at each price.
- `empty()` reports whether neither side has resting orders.

## Code layout

| File | Purpose |
| --- | --- |
| `order.hpp` | Order fields and the buy/sell enum |
| `order_book.hpp` | Book interface and trade/snapshot result types |
| `order_book.cpp` | Matching, cancellation, and snapshots |
| `core_tests.cpp` | Core behavior checks, independent of a server |
| `server.cpp` | Drogon HTTP and WebSocket server |
| `main.cpp` | Earlier learning examples and basic checks; not part of the CMake build |
| `CMakeLists.txt` | Builds the core library, tests, and server |

## References

 - Bjarne Stroustrup's *A Tour of C++*
 - Klaus Iglberger's *C++ Software Design*.
 - [Drogon documentation](https://github.com/drogonframework/drogon/wiki)
 - [jsonstore](https://github.com/drogonframework/drogon/tree/master/examples/jsonstore)
 - [websocket_server](https://github.com/drogonframework/drogon/tree/master/examples/websocket_server)
