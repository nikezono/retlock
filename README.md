[![Actions Status](https://github.com/nikezono/retlock/workflows/MacOS/badge.svg)](https://github.com/nikezono/retlock/actions)
[![Actions Status](https://github.com/nikezono/retlock/workflows/Ubuntu/badge.svg)](https://github.com/nikezono/retlock/actions)
[![Actions Status](https://github.com/nikezono/retlock/workflows/Style/badge.svg)](https://github.com/nikezono/retlock/actions)
[![codecov](https://codecov.io/gh/nikezono/retlock/graph/badge.svg?token=9PBB727WMZ)](https://codecov.io/gh/nikezono/retlock)

# ReTLock

Fast and Efficient C++ Implementation of Reentrant Locking

## Usage

You can use `include/retlock/retlock.hpp` as the single-file header-only library.

## Example

It's compatible with [`std::recursive_mutex`](https://en.cppreference.com/w/cpp/thread/recursive_mutex). 

```c++
#include "retlock/retlock.hpp"

retlock::RetLock lock;
lock.lock();
// something critical...

lock.lock(); // recursive locking is allowed

lock.unlock();
lock.unlock();
```

```c++
#include "retlock/retlock.hpp"
#include <mutex>

retlock::ReTLock lock;
// recursive locking with scoped lock pattern

{
    std::unique_lock<retlock::ReTLock> ul(lock);
    std::unique_lock<retlock::ReTLock> ul2(lock);
} // unlocked
```
See `example/` directory for more details.

## Build (No need to do it, except for developers)
To build the benchmark & test cases, use the followings:

```bash
# build
cmake -S all -B build
cmake --build build

# run benchmark
./build/benchmark/ReTLockBench --help
# run tests
ctest --test-dir build
or
./build/test/ReTLockTests

# format code
cmake --build build --target format
cmake --build build --target fix-format
```

# Benchmark Result
Here's the throughput from testing on AWS EC2 m8g.16xlarge. All implementations except `std::recursive_mutex` are in this repository, where options are varied.
You can use `+AFS,AS` with the simplest declaration `retlock::RetLock`.
Check out the code in the `benchmark` directory for details.

<img width="868" height="547" alt="image" src="https://github.com/user-attachments/assets/1449ce43-f8b1-42bb-bd4c-e5e518a6534e" />

