// Copyright @nikezono, 2023-
#include <retlock/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <retlock/retlock.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct Config {
  std::string filename;
  size_t num_threads;
  size_t iteration;
  size_t duration;
};

template <typename LockType>
void reentrant_worker(LockType* lock, size_t* shared_variable,
                      std::chrono::steady_clock::time_point start_time,
                      Config c, int* local_counter) {
  std::this_thread::sleep_until(start_time);
  auto end_time = start_time + std::chrono::seconds(c.duration);
  while (std::chrono::steady_clock::now() < end_time) {
    for (int i = 0; i < c.iteration; ++i) {
      lock->lock();
      // access shared variables in the critical section
      // See LBench (Lock Cohorting, Dice et al, PPoPP'12) for more details
      (*shared_variable)++;
      lock->unlock();
    }
    (*local_counter)++;
  }
}

template <typename LockType> void benchmark(Config c, std::string lock_name) {
  std::cout << "..." << std::endl;
  std::vector<int> counters(c.num_threads, 0);
  std::vector<std::thread> threads;
  LockType lock;
  size_t shared_variable = 0;

  auto start_time
      = std::chrono::steady_clock::now()
        + std::chrono::seconds(1);  // Wait 1 second for starting up all threads

  /* Benchmarking */
  for (int i = 0; i < c.num_threads; ++i) {
    threads.emplace_back([&, i] {
      reentrant_worker<LockType>(&lock, &shared_variable, start_time, c,
                                 &counters[i]);
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  auto end_time = std::chrono::steady_clock::now();
  auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end_time - start_time)
                          .count();

  /* Calculate Results */
  auto success_count = std::accumulate(counters.begin(), counters.end(), 0);
  size_t throughput = static_cast<size_t>(std::round(
      static_cast<double>(success_count) / static_cast<double>(elapsed_time)));

  std::cout << "--- Benchmark results ---" << std::endl;
  std::cout << "Config: lock " << lock_name << " thread " << c.num_threads
            << ", iteration " << c.iteration << std::endl;
  std::cout << "Total lock acquisition count: " << success_count << std::endl;
  std::cout << "Elapsed time: " << elapsed_time << " milliseconds" << std::endl;
  std::cout << "Throughput: " << throughput << " iterations/second"
            << std::endl;
  std::cout << "-------------------------" << std::endl;

  /* Output to CSV */
  std::fstream csv_file(c.filename, std::ios::app);
  bool file_exists = csv_file.good();
  if (!csv_file.is_open()) {
    std::cerr << "Failed to open " << c.filename << " for writing.\n";
    return;
  }

  if (!file_exists) {
    csv_file << "Version,LockType,ThreadCount,Iteration,LockAcquisitionCount,"
                "ElapsedTime,OPS\n";
  }

  csv_file << RETLOCK_VERSION << "," << lock_name << "," << c.num_threads << ","
           << c.iteration << "," << success_count << elapsed_time << ","
           << throughput << "\n";
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
  while (0 < c.num_threads) {
    c.iteration = iteration;
    while (0 < c.iteration) {
      benchmark<std::recursive_mutex>(c, "std::recursive_mutex");
      c.iteration--;
    }
    c.num_threads--;
  }

  // TODO
  // benchmark<ReLock>(num_threads, max_reentrant_count, duration);
  // benchmark<ReLockTLS>(num_threads, max_reentrant_count, duration);
  // benchmark<ReQuLock>(num_threads, max_reentrant_count, duration);
  // benchmark<ReQuLockTLS>(num_threads, max_reentrant_count, duration);

  return 0;
}
