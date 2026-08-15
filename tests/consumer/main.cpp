// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include "linkage_check.h"

int main() { return granit::library_version().major == GRANIT_VERSION_MAJOR ? 0 : 1; }
