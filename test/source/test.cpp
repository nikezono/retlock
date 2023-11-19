#include <doctest/doctest.h>
#include <retlock/version.h>

#include <retlock/retlock.hpp>
#include <string>

TEST_CASE_TEMPLATE_DEFINE("signed integer stuff", T, test_id) {
  T var = T();
  --var;
  CHECK(var == -1);
}

TEST_CASE_TEMPLATE_INVOKE(test_id, char, short, int, long long int);

TEST_CASE_TEMPLATE_APPLY(test_id, std::tuple<float, double>);
