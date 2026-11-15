#pragma once
#include "AppAbi.h"
#include "Package.h"
#include <memory>
namespace sdapps {
class ModuleImage {
 public:
  ModuleImage();
  ~ModuleImage();
  ModuleImage(const ModuleImage&) = delete;
  ModuleImage& operator=(const ModuleImage&) = delete;
  bool load(const char* root, const Package& package);
  const CpModuleV1& module() const;
  void unload();
 private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
}
