#include <mutex>
#include <retlock/retlock.hpp>

int main() {
  retlock::ReTLock lock;
  {  // lock & unlock manually
    lock.lock();
    // something critical processing...
    lock.unlock();
  }

  {  // recursive
    lock.lock();
    lock.lock();
    // something critical processing...
    lock.unlock();
    lock.unlock();
  }

  {  // lock & unlock with RAII
    retlock::ReTLock lock;
    std::unique_lock<retlock::ReTLock> ul(lock);
    // something critical processing...
  }

  {  // recursive with RAII
    retlock::ReTLock lock;
    std::unique_lock<retlock::ReTLock> ul(lock);
    std::unique_lock<retlock::ReTLock> ul2(lock);
    // something critical processing...
  }

  return 0;
}