// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/instance.h"

#include "backend/vulkan/loader.h"
#include "backend/vulkan/result.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <granit/version.h>

namespace granit::detail {
namespace {

constexpr const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT,
  VkDebugUtilsMessageTypeFlagsEXT,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
  void*) {
  if (callback_data != nullptr && callback_data->pMessage != nullptr) {
    std::fprintf(stderr, "[granit][vulkan] %s\n", callback_data->pMessage);
  }
  return VK_FALSE;
}

bool validation_support_available() {
  std::uint32_t layer_count = 0;
  if (volk::vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS) {
    return false;
  }
  std::vector<VkLayerProperties> layers(layer_count);
  if (layer_count != 0 &&
      volk::vkEnumerateInstanceLayerProperties(&layer_count, layers.data()) != VK_SUCCESS) {
    return false;
  }

  bool layer_available = false;
  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, validation_layer_name) == 0) {
      layer_available = true;
      break;
    }
  }
  if (!layer_available) {
    return false;
  }

  std::uint32_t extension_count = 0;
  if (volk::vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr) != VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count != 0 && volk::vkEnumerateInstanceExtensionProperties(
                                nullptr,
                                &extension_count,
                                extensions.data()) != VK_SUCCESS) {
    return false;
  }
  for (const auto& extension : extensions) {
    if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
      return true;
    }
  }
  return false;
}

VkDebugUtilsMessengerCreateInfoEXT make_debug_messenger_create_info() noexcept {
  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = debug_callback;
  return create_info;
}

} // namespace

vulkan_instance::~vulkan_instance() { reset(); }

vulkan_instance::vulkan_instance(vulkan_instance&& other) noexcept
    : instance_(std::exchange(other.instance_, VK_NULL_HANDLE)),
      debug_messenger_(std::exchange(other.debug_messenger_, VK_NULL_HANDLE)),
      functions_(other.functions_) {
  other.functions_ = {};
}

vulkan_instance& vulkan_instance::operator=(vulkan_instance&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  reset();
  instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
  debug_messenger_ = std::exchange(other.debug_messenger_, VK_NULL_HANDLE);
  functions_ = other.functions_;
  other.functions_ = {};
  return *this;
}

granit_result vulkan_instance::initialize(const vulkan_instance_desc& desc) {
  if (valid() || desc.application_name.data() == nullptr || desc.application_name.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    return loader.result;
  }

  try {
    if (desc.enable_validation && !validation_support_available()) {
      return GRANIT_ERROR_UNSUPPORTED;
    }

    // Vulkan 需要以零结尾的名称，string_view 本身不保证这一点。
    const std::string application_name{desc.application_name};
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = application_name.c_str();
    application_info.applicationVersion = VK_MAKE_API_VERSION(
      0,
      GRANIT_VERSION_MAJOR,
      GRANIT_VERSION_MINOR,
      GRANIT_VERSION_PATCH);
    application_info.pEngineName = "Granit";
    application_info.engineVersion = application_info.applicationVersion;
    application_info.apiVersion = VK_API_VERSION_1_3;

    const char* layers[] = {validation_layer_name};
    const char* extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    auto debug_create_info = make_debug_messenger_create_info();

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    if (desc.enable_validation) {
      create_info.enabledLayerCount = 1;
      create_info.ppEnabledLayerNames = layers;
      create_info.enabledExtensionCount = 1;
      create_info.ppEnabledExtensionNames = extensions;
      create_info.pNext = &debug_create_info;
    }

    const auto create_result = volk::vkCreateInstance(&create_info, nullptr, &instance_);
    if (create_result != VK_SUCCESS) {
      instance_ = VK_NULL_HANDLE;
      return map_vulkan_result(create_result);
    }

    volk::volkLoadInstanceTable(&functions_, instance_);
    if (desc.enable_validation) {
      if (functions_.vkCreateDebugUtilsMessengerEXT == nullptr) {
        reset();
        return GRANIT_ERROR_INITIALIZATION_FAILED;
      }
      const auto debug_result = functions_.vkCreateDebugUtilsMessengerEXT(
        instance_,
        &debug_create_info,
        nullptr,
        &debug_messenger_);
      if (debug_result != VK_SUCCESS) {
        const auto mapped_result = map_vulkan_result(debug_result);
        reset();
        return mapped_result;
      }
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    reset();
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

void vulkan_instance::reset() noexcept {
  if (instance_ == VK_NULL_HANDLE) {
    return;
  }
  if (debug_messenger_ != VK_NULL_HANDLE && functions_.vkDestroyDebugUtilsMessengerEXT != nullptr) {
    functions_.vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  }
  if (functions_.vkDestroyInstance != nullptr) {
    functions_.vkDestroyInstance(instance_, nullptr);
  }
  instance_ = VK_NULL_HANDLE;
  debug_messenger_ = VK_NULL_HANDLE;
  functions_ = {};
}

} // namespace granit::detail
