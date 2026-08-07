// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <iostream>

int main() {
  const auto version = granit::library_version();
  std::cout << "granit " << version.major << '.' << version.minor << '.' << version.patch << '\n';
}
