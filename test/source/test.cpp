#include <doctest/doctest.h>
#include <retlock/version.h>

#include <retlock/retlock.hpp>
#include <string>

TEST_CASE("ReTLock") {
  using namespace retlock;

  ReTLock retlock("Tests");

  CHECK(retlock.greet(LanguageCode::EN) == "Hello, Tests!");
  CHECK(retlock.greet(LanguageCode::DE) == "Hallo Tests!");
  CHECK(retlock.greet(LanguageCode::ES) == "¡Hola Tests!");
  CHECK(retlock.greet(LanguageCode::FR) == "Bonjour Tests!");
}

TEST_CASE("ReTLock version") {
  static_assert(std::string_view(RETLOCK_VERSION) == std::string_view("1.0"));
  CHECK(std::string(RETLOCK_VERSION) == std::string("1.0"));
}
