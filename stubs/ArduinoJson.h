// Minimal stub of ArduinoJson.h for host-side IDE/static analysis only.
//
// ESPHome's core headers transitively include <ArduinoJson.h> (a platformio
// dependency not present in the vendored source tree). This stub declares just
// enough of the API surface for clangd to parse esphome's headers on the host.
// It is NOT used at runtime -- real builds pull the genuine ArduinoJson via
// platformio. See components/wmmm/compile_flags.txt (-I../../stubs).
#pragma once

#include <cstddef>
#include <string>

namespace ArduinoJson {

class Allocator {
public:
  virtual void *allocate(size_t n) = 0;
  virtual void deallocate(void *p) = 0;
  virtual void *reallocate(void *p, size_t n) = 0;
  virtual ~Allocator() = default;
};

} // namespace ArduinoJson

class JsonVariant {
public:
  template <typename T> void set(const T &) {} // NOLINT
};

class JsonObject {
public:
  JsonObject() = default;
};

class JsonDocument {
public:
  JsonDocument() = default;
  explicit JsonDocument(ArduinoJson::Allocator *) {}
  template <typename T> T to() { return T{}; }
  template <typename T> T as() { return T{}; }
};
