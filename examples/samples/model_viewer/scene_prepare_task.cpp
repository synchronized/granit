// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/scene_prepare_task.h"

#include "model_viewer/viewer_session.h"

#include <chrono>
#include <future>
#include <new>

namespace granit::example::model_viewer {

struct scene_prepare_task::state {
  granit::result result{granit::result::not_ready};
  bool complete{};
#if !defined(__EMSCRIPTEN__)
  std::future<granit::result> operation;
#endif
};

scene_prepare_task::scene_prepare_task() = default;

scene_prepare_task::~scene_prepare_task() { reset(); }

granit::result scene_prepare_task::begin(viewer_session& session,
                                         gltf::import_progress_callback progress,
                                         void* progress_user_data) noexcept {
  if (state_)
    return granit::result::invalid_argument;
  try {
    auto candidate = std::make_unique<state>();
#if defined(__EMSCRIPTEN__)
    candidate->result = session.prepare_scene(progress, progress_user_data);
    candidate->complete = true;
#else
    candidate->operation = std::async(std::launch::async, [&session, progress, progress_user_data] {
      return session.prepare_scene(progress, progress_user_data);
    });
#endif
    state_ = std::move(candidate);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::initialization_failed;
  }
}

granit::result scene_prepare_task::poll() noexcept {
  if (!state_)
    return granit::result::invalid_argument;
#if defined(__EMSCRIPTEN__)
  return state_->complete ? state_->result : granit::result::not_ready;
#else
  if (state_->complete)
    return state_->result;
  if (state_->operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready)
    return granit::result::not_ready;
  try {
    state_->result = state_->operation.get();
  } catch (const std::bad_alloc&) {
    state_->result = granit::result::out_of_memory;
  } catch (...) {
    state_->result = granit::result::internal;
  }
  state_->complete = true;
  return state_->result;
#endif
}

void scene_prepare_task::reset() noexcept {
#if !defined(__EMSCRIPTEN__)
  if (state_ && state_->operation.valid()) {
    try {
      static_cast<void>(state_->operation.get());
    } catch (...) {
    }
  }
#endif
  state_.reset();
}

bool scene_prepare_task::running() const noexcept {
  if (!state_)
    return false;
  return !state_->complete;
}

} // namespace granit::example::model_viewer
