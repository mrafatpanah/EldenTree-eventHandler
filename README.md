# Elden Tree - C++ Event Handler

A fast, thread-safe C++14 library for handling events concurrently and fairly. Uses a thread pool and per-destination queues.

## Requirements

* C++14 Compiler (GCC, Clang, MSVC)
* CMake (3.10+)
* Git

## Build & Run Example

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/mrafatpanah/EldenTree-eventHandler.git
    cd EldenTree-eventHandler
    ```
2.  **Build the project:**
    ```bash
    mkdir build && cd build
    cmake ..
    make
    ```

3.  **Run the example:**
    ```bash
    ./main
    ```

## Running Tests

The project uses GoogleTest for unit testing.

1. **After building:**
```bash
./EldenTreeTest
```

Tests will automatically download and build GoogleTest during the CMake process.

## Running Benchmarks
The project uses Google Benchmark for performance testing.

1. **Configure CMake with benchmarks enabled:**
(Run this in your build directory after cloning)

```bash
# Make sure you are in the build directory
cmake .. -DBUILD_BENCHMARKS=ON
```

2. **Build the project (including benchmarks):**

```bash
make
# Or: cmake --build .
```

3. **Run the benchmark executable:**
(The executable name might be slightly different based on your CMakeLists.txt)

```bash
./elden_tree_benchmark
```

Benchmarks will automatically download and build Google Benchmark during the CMake process if BUILD_BENCHMARKS is ON.

## Quick Usage

```cpp
#include "elden_tree/EldenTree.hpp"
#include <memory>

// Implement elden_tree::god::IEventProcessor
class MyProcessor : public elden_tree::god::IEventProcessor { /* ... */ };

int main() {
    elden_tree::EldenTree tree(4);
    auto processor = std::make_shared<MyProcessor>(101);
    tree.registerProcessor(processor);

    elden_tree::god::GodEvent event;
    event.targetLandId = 101;
    // ... set other event fields ...
    tree.postEvent(event);
    return 0;
}
```