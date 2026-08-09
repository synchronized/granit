// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/retirement_queue.h"

#include <memory>
#include <vector>

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::retirement_order;
using granit::detail::retirement_queue;
using granit::detail::submission_serials;

struct tracked_resource {
  tracked_resource(std::vector<int>& destroyed, int id) : destroyed{destroyed}, id{id} {}
  ~tracked_resource() { destroyed.push_back(id); }

  std::vector<int>& destroyed;
  int id;
};

TEST_CASE("提交序号只接受连续成功提交并限制完成值", "[retirement][serial]") {
  submission_serials serials;
  CHECK(serials.next() == 1);
  CHECK_FALSE(serials.commit(2));
  CHECK(serials.last_submitted() == 0);
  REQUIRE(serials.commit(1));
  REQUIRE(serials.commit(2));

  serials.mark_completed(1);
  CHECK(serials.completed() == 1);
  serials.mark_completed(8);
  CHECK(serials.completed() == 2);
  serials.mark_completed(0);
  CHECK(serials.completed() == 2);
}

TEST_CASE("退役队列仅收集已完成序号并允许乱序入队", "[retirement][collect]") {
  std::vector<int> destroyed;
  retirement_queue queue;
  queue.retire(8, retirement_order::resource, std::make_shared<tracked_resource>(destroyed, 8));
  queue.retire(2, retirement_order::resource, std::make_shared<tracked_resource>(destroyed, 2));

  CHECK(queue.size() == 2);
  CHECK(queue.earliest_serial() == 2);
  CHECK(queue.collect(1) == 0);
  CHECK(queue.collect(2) == 1);
  CHECK(destroyed == std::vector{2});
  CHECK(queue.earliest_serial() == 8);
  CHECK(queue.collect(8) == 1);
  CHECK(destroyed == std::vector{2, 8});
  CHECK(queue.empty());
}

TEST_CASE("同一完成点先释放引用对象再释放基础资源", "[retirement][order]") {
  std::vector<int> destroyed;
  retirement_queue queue;
  queue.retire(3, retirement_order::resource, std::make_shared<tracked_resource>(destroyed, 2));
  queue.retire(3, retirement_order::dependent, std::make_shared<tracked_resource>(destroyed, 1));

  CHECK(queue.collect(3) == 2);
  CHECK(destroyed == std::vector{1, 2});
}

TEST_CASE("零序号可立即收集且关闭排空全部资源", "[retirement][drain]") {
  std::vector<int> destroyed;
  retirement_queue queue;
  queue.retire(0, retirement_order::resource, std::make_shared<tracked_resource>(destroyed, 0));
  queue.retire(9, retirement_order::resource, std::make_shared<tracked_resource>(destroyed, 9));

  CHECK(queue.collect(0) == 1);
  CHECK(destroyed == std::vector{0});
  CHECK(queue.drain() == 1);
  CHECK(destroyed == std::vector{0, 9});
  CHECK(queue.empty());
}

} // namespace
