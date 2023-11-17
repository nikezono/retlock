#include <retlock/version.h>

#include <cxxopts.hpp>
#include <iostream>
#include <retlock/retlock.hpp>
#include <string>
#include <unordered_map>

auto main(int argc, char** argv) -> int {
  const std::unordered_map<std::string, retlock::LanguageCode> languages{
      {"en", retlock::LanguageCode::EN},
      {"de", retlock::LanguageCode::DE},
      {"es", retlock::LanguageCode::ES},
      {"fr", retlock::LanguageCode::FR},
  };

  cxxopts::Options options(*argv, "A program to welcome the world!");

  std::string language;
  std::string name;

  // clang-format off
  options.add_options()
    ("h,help", "Show help")
    ("v,version", "Print the current version number")
    ("n,name", "Name to greet", cxxopts::value(name)->default_value("World"))
    ("l,lang", "Language code to use", cxxopts::value(language)->default_value("en"))
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

  auto langIt = languages.find(language);
  if (langIt == languages.end()) {
    std::cerr << "unknown language code: " << language << std::endl;
    return 1;
  }

  retlock::ReTLock retlock(name);
  std::cout << retlock.greet(langIt->second) << std::endl;

  return 0;
}
