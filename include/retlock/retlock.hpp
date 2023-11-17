#pragma once

#include <fmt/format.h>

#include <string>

namespace retlock {

  /**  Language codes to be used with the ReTLock class */
  enum class LanguageCode { EN, DE, ES, FR };

  /**
   * @brief A class for saying hello in multiple languages
   */
  class ReTLock {
    std::string name;

  public:
    /**
     * @brief Creates a new retlock
     * @param name the name to greet
     */
    ReTLock(std::string name);

    /**
     * @brief Creates a localized string containing the greeting
     * @param lang the language to greet in
     * @return a string containing the greeting
     */
    std::string greet(LanguageCode lang = LanguageCode::EN) const;
  };

  ReTLock::ReTLock(std::string _name) : name(std::move(_name)) {}

  std::string ReTLock::greet(LanguageCode lang) const {
    switch (lang) {
      default:
      case LanguageCode::EN:
        return fmt::format("Hello, {}!", name);
      case LanguageCode::DE:
        return fmt::format("Hallo {}!", name);
      case LanguageCode::ES:
        return fmt::format("¡Hola {}!", name);
      case LanguageCode::FR:
        return fmt::format("Bonjour {}!", name);
    }
  }

}  // namespace retlock
