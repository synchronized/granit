// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application.h"

int main() {
  return granit::example::model_viewer::web::run_application({
      .default_model_url = "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/"
                           "main/Models/FlightHelmet/glTF/FlightHelmet.gltf",
  });
}
