// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace {

class json_object_parser final {
public:
  explicit json_object_parser(std::string_view input) : input_(input) {}

  bool parse() {
    if (!parse_object() || !skip_space() || position_ != input_.size())
      return false;
    return keys_.contains("schema_version") && keys_.contains("sequence") &&
           keys_.contains("timestamp_ns") && keys_.contains("kind") &&
           allowed_kinds_.contains(kind_);
  }

private:
  bool skip_space() noexcept {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_])) != 0)
      ++position_;
    return true;
  }

  bool consume(char value) noexcept {
    skip_space();
    if (position_ >= input_.size() || input_[position_] != value)
      return false;
    ++position_;
    return true;
  }

  bool parse_string(std::string* value = nullptr) {
    if (!consume('"'))
      return false;
    if (value != nullptr)
      value->clear();
    while (position_ < input_.size()) {
      const auto character = input_[position_++];
      if (character == '"')
        return true;
      if (static_cast<unsigned char>(character) < 0x20)
        return false;
      if (character != '\\') {
        if (value != nullptr)
          value->push_back(character);
        continue;
      }
      if (position_ >= input_.size())
        return false;
      const auto escaped = input_[position_++];
      if (escaped == 'u') {
        if (position_ + 4 > input_.size())
          return false;
        for (unsigned index = 0; index < 4; ++index) {
          if (std::isxdigit(static_cast<unsigned char>(input_[position_++])) == 0)
            return false;
        }
        if (value != nullptr)
          value->push_back('?');
      } else if (std::string_view{"\"\\/bfnrt"}.find(escaped) == std::string_view::npos) {
        return false;
      } else if (value != nullptr) {
        value->push_back(escaped);
      }
    }
    return false;
  }

  bool parse_number() noexcept {
    skip_space();
    const auto start = position_;
    if (position_ < input_.size() && input_[position_] == '-')
      ++position_;
    if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_])))
      return false;
    if (input_[position_] == '0')
      ++position_;
    else
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_])) != 0)
        ++position_;
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      if (position_ >= input_.size() ||
          std::isdigit(static_cast<unsigned char>(input_[position_])) == 0)
        return false;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_])) != 0)
        ++position_;
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
        ++position_;
      if (position_ >= input_.size() ||
          std::isdigit(static_cast<unsigned char>(input_[position_])) == 0)
        return false;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_])) != 0)
        ++position_;
    }
    return position_ > start;
  }

  bool parse_literal() noexcept {
    skip_space();
    for (const auto literal :
         {std::string_view{"true"}, std::string_view{"false"}, std::string_view{"null"}}) {
      if (input_.substr(position_).starts_with(literal)) {
        position_ += literal.size();
        return true;
      }
    }
    return false;
  }

  bool parse_array() {
    if (!consume('['))
      return false;
    skip_space();
    if (position_ < input_.size() && input_[position_] == ']') {
      ++position_;
      return true;
    }
    while (true) {
      if (!parse_value())
        return false;
      skip_space();
      if (position_ < input_.size() && input_[position_] == ']') {
        ++position_;
        return true;
      }
      if (!consume(','))
        return false;
    }
  }

  bool parse_object() {
    if (!consume('{'))
      return false;
    skip_space();
    if (position_ < input_.size() && input_[position_] == '}') {
      ++position_;
      return true;
    }
    while (true) {
      std::string key;
      if (!parse_string(&key) || !consume(':'))
        return false;
      keys_.insert(key);
      if (key == "kind") {
        if (!parse_string(&kind_))
          return false;
      } else if (!parse_value()) {
        return false;
      }
      skip_space();
      if (position_ < input_.size() && input_[position_] == '}') {
        ++position_;
        return true;
      }
      if (!consume(','))
        return false;
    }
  }

  bool parse_value() {
    skip_space();
    if (position_ >= input_.size())
      return false;
    switch (input_[position_]) {
    case '"':
      return parse_string();
    case '{':
      return parse_object();
    case '[':
      return parse_array();
    case 't':
    case 'f':
    case 'n':
      return parse_literal();
    default:
      return parse_number();
    }
  }

  std::string_view input_;
  std::size_t position_{};
  std::set<std::string> keys_;
  std::string kind_;
  const std::set<std::string> allowed_kinds_{
      "frame",     "pass",          "command",    "resource",       "sync",
      "timestamp", "gpu_timestamp", "diagnostic", "resource_stats", "trace_summary"};
};

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "用法：granit_frame_trace_validator <trace.jsonl>\n";
    return 2;
  }
  std::ifstream input{argv[1]};
  if (!input) {
    std::cerr << "无法打开 Trace：" << argv[1] << '\n';
    return 2;
  }
  std::string line;
  std::uint64_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty() || !json_object_parser{line}.parse()) {
      std::cerr << "Trace 第 " << line_number << " 行无效\n";
      return 1;
    }
  }
  if (line_number == 0) {
    std::cerr << "Trace 为空\n";
    return 1;
  }
  return 0;
}
