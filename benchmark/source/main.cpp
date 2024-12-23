#include <fmt/format.h>
#include <retlock/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <retlock/retlock.hpp>
#include <retlock/retlock_queue.hpp>
#include <retlock/retlock_sameline.hpp>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

std::atomic<bool> start_benchmark(false);
std::atomic<bool> stop_benchmark(false);
struct SharedVar {
  alignas(64) int foo;
  alignas(64) int bar;
};
SharedVar shared_variable;

struct Config {
  std::string filename;
  size_t num_threads;
  size_t iteration;
  size_t duration;
  bool back_and_forth;
};

template <typename LockType> void reentrant_worker(LockType* lock, Config c, int* local_counter) {
  while (!start_benchmark.load()) {
    std::this_thread::yield();
  }
  while (!stop_benchmark.load(std::memory_order_relaxed)) {
    // For non-recursive mutex: lock and unlock for each iteration
    if constexpr (std::is_same_v<LockType, std::mutex>) {
      for (int i = 0; i < c.iteration; ++i) {
        lock->lock();
        shared_variable.foo++;
        shared_variable.bar++;
        lock->unlock();
      }
      (*local_counter)++;
      continue;
    }
    if (c.back_and_forth) {
      lock->lock();

      for (int i = 0; i < c.iteration; ++i) {
        lock->lock();
        // access shared variables in the critical section
        // See LBench (Lock Cohorting, Dice et al, PPoPP'12) for more details
        shared_variable.foo++;
        shared_variable.bar++;
        lock->unlock();
      }
      lock->unlock();

      (*local_counter)++;
      // non-critical section work: 4 microseconds
      // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    } else {
      for (int i = 0; i < c.iteration; ++i) {
        lock->lock();
      }
      // access shared variables in the critical section
      // See LBench (Lock Cohorting, Dice et al, PPoPP'12) for more details
      shared_variable.foo++;
      shared_variable.bar++;

      for (int i = 0; i < c.iteration; ++i) {
        lock->unlock();
      }
      (*local_counter)++;
      // non-critical section work: 4 microseconds
      // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    }
  }
}

template <typename LockType> void benchmark(Config c, std::string lock_name) {
  std::cout << "..." << std::endl;
  std::vector<int> counters(c.num_threads, 0);
  std::vector<std::thread> threads;
  LockType lock;
  stop_benchmark.store(false);
  start_benchmark.store(false);

  auto start_time = std::chrono::steady_clock::now();

  /* Benchmarking */
  for (int i = 0; i < c.num_threads; ++i) {
    threads.emplace_back([&, i] { reentrant_worker<LockType>(&lock, c, &counters[i]); });
  }

  start_benchmark.store(true);
  std::this_thread::sleep_until(start_time + std::chrono::seconds(c.duration));
  stop_benchmark.store(true, std::memory_order_relaxed);

  for (auto& t : threads) {
    t.join();
  }
  auto end_time = std::chrono::steady_clock::now();
  auto elapsed_time
      = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  /* Calculate Results */
  auto success_count = std::accumulate(counters.begin(), counters.end(), 0);
  size_t throughput = static_cast<size_t>(std::round(
      static_cast<double>(success_count) / (static_cast<double>(elapsed_time) / 1000.0)));

  std::cout << "--- Benchmark results ---" << std::endl;
  std::cout << "Config: lock " << lock_name << " thread " << c.num_threads << ", iteration "
            << c.iteration << std::endl;
  std::cout << "Back and Forth: " << c.back_and_forth << std::endl;
  std::cout << "Total lock acquisition count: " << success_count << std::endl;
  std::cout << "Elapsed time: " << elapsed_time << " milliseconds" << std::endl;
  std::cout << "Throughput: " << throughput << " iterations/second" << std::endl;
  std::cout << "-------------------------" << std::endl;

  /* Output to CSV */
  std::ifstream infile(c.filename);
  bool file_exists = infile.good();
  std::fstream csv_file(c.filename, std::ios::app);
  if (!csv_file.is_open()) {
    std::cerr << "Failed to open " << c.filename << " for writing.\n";
    return;
  }

  if (!file_exists) {
    csv_file << "Version,LockType,Type,BackAndForth,ThreadCount,ThreadID,Iteration,"
                "LockAcquisitionCount,"
                "ElapsedTime,OPS\n";
  }

  csv_file << fmt::format("{},\"{}\",\"Sum\",{},{},{},{},{},{},{}", RETLOCK_VERSION, lock_name,
                          c.back_and_forth, c.num_threads, 0, c.iteration, c.back_and_forth,
                          elapsed_time, throughput)
           << std::endl;

  for (int i = 0; i < c.num_threads; ++i) {
    csv_file << fmt::format("{},\"{}\",\"ForEachThread\",{},{},{},{},{},{},{}", RETLOCK_VERSION,
                            lock_name, c.back_and_forth, c.num_threads, +1, c.iteration,
                            counters[i], elapsed_time, throughput)
             << std::endl;
  }
}

void work(Config c) {
  benchmark<std::mutex>(c, "std::mutex");
  benchmark<std::recursive_mutex>(c, "std::recursive_mutex");
  benchmark<retlock::ReTLockQueue>(c, "MCS");
  benchmark<retlock::ReTLockQueueAFS>(c, "MCS+Adap");
  benchmark<retlock::ReTLockVanilla>(c, "Exponential");
  benchmark<retlock::ReTLockSameLineNoSleep>(c, "NoSleep");
  benchmark<retlock::ReTLockSameLineYield>(c, "Yield");
  benchmark<retlock::ReTLockSameLineYield>(c, "Adaptive");
  benchmark<retlock::ReTLockPadding>(c, "Exp+Padding");
  benchmark<retlock::ReTLockYieldPadding>(c, "Yie+Padding");
  benchmark<retlock::ReTLockAdaptivePadding>(c, "Adap+Padding");
  benchmark<retlock::ReTLockNoSleepPadding>(c, "NoSl+Padding");
}

auto main(int argc, char** argv) -> int {
  cxxopts::Options options(*argv, "Benchmark for reentrant locking");

  Config c{"benchmark.csv", 0, 0, 0};

  // clang-format off
  options.add_options()
    ("h,help", "Show help")
    ("v,version", "Print the current version number")
    ("t,thread", "Number of the max thread", cxxopts::value(c.num_threads)->default_value("4"))
    ("r,", "Number of the recursive iteration to lock", cxxopts::value(c.iteration)->default_value("8"))
    ("d,", "Duration of benchmark (seconds)", cxxopts::value(c.duration)->default_value("10"))
  ;
  // clang-format on

  auto result = options.parse(argc, argv);

  if (result["help"].as<bool>()) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (result["version"].as<bool>()) {
    std::cout << "ReTLock, version " << RETLOCK_VERSION << std::endl;
    return 0;
  }

  const size_t threads = c.num_threads;
  const size_t iteration = c.iteration;
  for (bool back_and_forth : {false, true}) {
    c.back_and_forth = back_and_forth;
    c.iteration = iteration;
    while (0 < c.iteration) {
      c.num_threads = threads;

      while (0 < c.num_threads) {
        work(c);
        c.num_threads -= 4;
      }
      // single thread case
      c.num_threads = 1;
      work(c);

      if (c.iteration == 1) break;
      if (c.iteration <= 4) {
        c.iteration = 1;
      } else {
        c.iteration -= 4;
      }
    }
  }
  return 0;
}
