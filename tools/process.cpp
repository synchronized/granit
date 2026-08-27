// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "process.h"

#include <array>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace granit::tools {
namespace {

#if defined(_WIN32)
std::wstring to_wide(const std::string& value) {
  if (value.empty())
    return {};
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                        static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0)
    return {};
  std::wstring converted(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), converted.data(), size) != size)
    return {};
  return converted;
}

std::wstring quote_argument(const std::wstring& argument) {
  if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos)
    return argument;
  std::wstring quoted{L'"'};
  std::size_t backslashes = 0;
  for (const auto character : argument) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    if (character == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(character);
    } else {
      quoted.append(backslashes, L'\\');
      quoted.push_back(character);
    }
    backslashes = 0;
  }
  quoted.append(backslashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

void read_handle(HANDLE handle, std::string& output) {
  std::array<char, 4096> buffer{};
  DWORD read = 0;
  while (ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) &&
         read != 0)
    output.append(buffer.data(), read);
}
#else
void read_descriptor(int descriptor, std::string& output) {
  std::array<char, 4096> buffer{};
  for (;;) {
    const auto read_size = read(descriptor, buffer.data(), buffer.size());
    if (read_size > 0)
      output.append(buffer.data(), static_cast<std::size_t>(read_size));
    else if (read_size == 0 || errno != EINTR)
      break;
  }
}
#endif

} // namespace

bool run_process(const std::vector<std::string>& arguments, process_result& result) {
  result = {};
  if (arguments.empty() || arguments.front().empty())
    return false;
#if defined(_WIN32)
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE output_read = nullptr;
  HANDLE output_write = nullptr;
  HANDLE error_read = nullptr;
  HANDLE error_write = nullptr;
  if (!CreatePipe(&output_read, &output_write, &attributes, 0) ||
      !CreatePipe(&error_read, &error_write, &attributes, 0)) {
    if (output_read != nullptr)
      CloseHandle(output_read);
    if (output_write != nullptr)
      CloseHandle(output_write);
    if (error_read != nullptr)
      CloseHandle(error_read);
    if (error_write != nullptr)
      CloseHandle(error_write);
    return false;
  }
  SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(error_read, HANDLE_FLAG_INHERIT, 0);
  std::wstring command;
  for (const auto& argument : arguments) {
    const auto wide = to_wide(argument);
    if (wide.empty() && !argument.empty()) {
      CloseHandle(output_read);
      CloseHandle(output_write);
      CloseHandle(error_read);
      CloseHandle(error_write);
      return false;
    }
    if (!command.empty())
      command.push_back(L' ');
    command += quote_argument(wide);
  }
  command.push_back(L'\0');
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = output_write;
  startup.hStdError = error_write;
  PROCESS_INFORMATION process{};
  const auto created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(output_write);
  CloseHandle(error_write);
  if (!created) {
    CloseHandle(output_read);
    CloseHandle(error_read);
    return false;
  }
  std::thread output_thread{read_handle, output_read, std::ref(result.standard_output)};
  std::thread error_thread{read_handle, error_read, std::ref(result.standard_error)};
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 0;
  GetExitCodeProcess(process.hProcess, &exit_code);
  output_thread.join();
  error_thread.join();
  CloseHandle(output_read);
  CloseHandle(error_read);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  result.exit_code = static_cast<int>(exit_code);
  return true;
#else
  int output_pipe[2]{};
  int error_pipe[2]{};
  if (pipe(output_pipe) != 0)
    return false;
  if (pipe(error_pipe) != 0) {
    close(output_pipe[0]);
    close(output_pipe[1]);
    return false;
  }
  const auto child = fork();
  if (child < 0) {
    close(output_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[0]);
    close(error_pipe[1]);
    return false;
  }
  if (child == 0) {
    dup2(output_pipe[1], STDOUT_FILENO);
    dup2(error_pipe[1], STDERR_FILENO);
    close(output_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[0]);
    close(error_pipe[1]);
    std::vector<char*> native_arguments;
    native_arguments.reserve(arguments.size() + 1);
    for (const auto& argument : arguments)
      native_arguments.push_back(const_cast<char*>(argument.c_str()));
    native_arguments.push_back(nullptr);
    execvp(native_arguments.front(), native_arguments.data());
    _exit(127);
  }
  close(output_pipe[1]);
  close(error_pipe[1]);
  std::thread output_thread{read_descriptor, output_pipe[0], std::ref(result.standard_output)};
  std::thread error_thread{read_descriptor, error_pipe[0], std::ref(result.standard_error)};
  int status = 0;
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  output_thread.join();
  error_thread.join();
  close(output_pipe[0]);
  close(error_pipe[0]);
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
  return true;
#endif
}

} // namespace granit::tools
