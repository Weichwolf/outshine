#ifdef __linux__
#include "EncoderProfile.h"
#include "TimestampRange.h"
#include <SDL3/SDL_stdinc.h>
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {
namespace {
constexpr size_t kMaximumSubmissions = 4096;
static_assert(sizeof(std::array<uint64_t, 4>) == 4 * sizeof(uint64_t));
static_assert(sizeof(std::array<uint64_t, 2>) == 2 * sizeof(uint64_t));

struct ProfiledSubmission {
  uint64_t Serial = 0;
  uint64_t Dropped = 0;
  std::vector<ProfiledPass> Passes;
};
}

struct EncoderProfileSession {
  std::filesystem::path Output;
  uint64_t Device = 0;
  uint64_t Dropped = 0;
  uint64_t AllocationFailures = 0;
  size_t Next = 0;
  std::vector<ProfiledSubmission> Submissions;

  ~EncoderProfileSession() {
    using Json = nlohmann::json;
    Json rows = Json::array();
    for (size_t at = 0; at < Submissions.size(); ++at) {
      const auto &submission = Submissions[(Next + at) % Submissions.size()];
      Json passes = Json::array();
      for (const auto &pass : submission.Passes) {
        passes.push_back({{"label", pass.Label},
                          {"kind", pass.Raster ? "render" : "compute"},
                          {"available", pass.Available},
                          {"gpu_ns", pass.Available ? Json(pass.GpuNs) : Json(nullptr)},
                          {"draw_calls", pass.DrawCalls},
                          {"input_primitives",
                           pass.InputPrimitives ? Json(*pass.InputPrimitives) : Json(nullptr)}});
      }
      rows.push_back({{"serial", submission.Serial},
                      {"dropped_passes", submission.Dropped},
                      {"passes", std::move(passes)}});
    }
    const Json report{{"schema", 1},
                      {"backend", "Vulkan"},
                      {"device", Device},
                      {"scope", "natural_render_pass_and_dispatch_boundaries"},
                      {"retention", "latest_completed_submissions"},
                      {"dropped_submissions", Dropped},
                      {"allocation_failures", AllocationFailures},
                      {"submissions", std::move(rows)}};
    const auto staging = Output.string() + ".writing";
    std::ofstream stream(staging, std::ios::trunc);
    stream << report.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
    stream.close();
    std::error_code error;
    if (stream) { std::filesystem::rename(staging, Output, error); }
    if (!stream || error) {
      std::println(stderr, "Vulkan encoder profile: cannot publish {}", Output.c_str());
    }
  }
};

bool EncoderProfilingRequested() {
  const auto *prefix =
      SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "OUTSHINE_VULKAN_ENCODER_PROFILE");
  return prefix != nullptr && *prefix != '\0';
}

std::shared_ptr<EncoderProfileSession> CreateEncoderProfileSession(Context &context) {
  if (!EncoderProfilingRequested() || context.TimestampValidBits == 0 ||
      !(context.Properties.limits.timestampPeriod > 0)) {
    return {};
  }
  const auto *prefix =
      SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "OUTSHINE_VULKAN_ENCODER_PROFILE");
  std::error_code error;
  const auto temporaryPath = std::filesystem::temp_directory_path(error);
  if (error) { return {}; }
  const auto temporary = std::filesystem::weakly_canonical(temporaryPath, error);
  if (error) { return {}; }
  auto output = std::filesystem::weakly_canonical(
      std::string(prefix) + "." + std::to_string(context.Cookie) + ".json", error);
  if (error || !output.is_absolute() || !output.native().starts_with(temporary.native() + "/")) {
    std::println(stderr, "Vulkan encoder profile: use an absolute system-temp prefix");
    return {};
  }
  auto session = std::make_shared<EncoderProfileSession>();
  session->Device = context.Cookie;
  session->Output = std::move(output);
  session->Submissions.reserve(kMaximumSubmissions);
  return session;
}

EncoderProfile::~EncoderProfile() {
  if (Timestamps_ != VK_NULL_HANDLE) { vkDestroyQueryPool(Context_.Logical, Timestamps_, nullptr); }
  if (Primitives_ != VK_NULL_HANDLE) { vkDestroyQueryPool(Context_.Logical, Primitives_, nullptr); }
}

void EncoderProfile::Initialize(VkCommandBuffer commands) {
  if (!Session_) { return; }
  RecordedAt_ = std::chrono::steady_clock::now();
  Passes_.reserve(32);
  VkQueryPoolCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  create.queryType = VK_QUERY_TYPE_TIMESTAMP;
  create.queryCount = 2 * kMaximumPasses;
  if (vkCreateQueryPool(Context_.Logical, &create, nullptr, &Timestamps_) == VK_SUCCESS) {
    vkCmdResetQueryPool(commands, Timestamps_, 0, create.queryCount);
  } else {
    ++Session_->AllocationFailures;
  }
  if (!Context_.PipelineStatistics) { return; }
  create.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
  create.queryCount = kMaximumPasses;
  create.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT;
  if (vkCreateQueryPool(Context_.Logical, &create, nullptr, &Primitives_) == VK_SUCCESS) {
    vkCmdResetQueryPool(commands, Primitives_, 0, create.queryCount);
  } else {
    ++Session_->AllocationFailures;
  }
}

std::optional<uint32_t>
EncoderProfile::Begin(VkCommandBuffer commands, std::string_view label, bool raster) {
  if (!Session_) { return std::nullopt; }
  if (Passes_.size() == kMaximumPasses) {
    ++Dropped_;
    return std::nullopt;
  }
  const auto index = static_cast<uint32_t>(Passes_.size());
  Passes_.push_back({.Label = std::string(!raster && !Labels_.empty() ? Labels_.back() : label),
                     .Raster = raster,
                     .InputPrimitives = std::nullopt});
  Active_ = index;
  if (Timestamps_ != VK_NULL_HANDLE) {
    vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, Timestamps_, index * 2);
  }
  if (raster && Primitives_ != VK_NULL_HANDLE) { vkCmdBeginQuery(commands, Primitives_, index, 0); }
  return index;
}

void EncoderProfile::End(VkCommandBuffer commands, std::optional<uint32_t> pass) {
  if (!pass) { return; }
  if (Passes_[*pass].Raster && Primitives_ != VK_NULL_HANDLE) {
    vkCmdEndQuery(commands, Primitives_, *pass);
  }
  if (Timestamps_ != VK_NULL_HANDLE) {
    vkCmdWriteTimestamp2(
        commands, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, Timestamps_, *pass * 2 + 1);
  }
  Active_.reset();
}

void EncoderProfile::Draw(uint32_t calls) {
  if (Active_) { Passes_[*Active_].DrawCalls += calls; }
}

void EncoderProfile::BeginLabel(std::string_view label) {
  if (Session_) { Labels_.emplace_back(label); }
}

void EncoderProfile::EndLabel() {
  if (!Labels_.empty()) { Labels_.pop_back(); }
}

void EncoderProfile::Collect(uint64_t serial) {
  if (!Session_ || Collected_ || Passes_.empty()) { return; }
  Collected_ = true;
  std::vector<std::array<uint64_t, 4>> values(Passes_.size());
  VkResult result = VK_NOT_READY;
  if (Timestamps_ != VK_NULL_HANDLE && !values.empty()) {
    result = vkGetQueryPoolResults(Context_.Logical,
                                   Timestamps_,
                                   0,
                                   static_cast<uint32_t>(values.size()) * 2,
                                   values.size() * sizeof(values[0]),
                                   values.data(),
                                   2 * sizeof(uint64_t),
                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  }
  const auto period = Context_.Properties.limits.timestampPeriod;
  const auto bits = Context_.TimestampValidBits;
  const auto mask = std::numeric_limits<uint64_t>::max() >> (64 - bits);
  const double upperNs =
      std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - RecordedAt_)
          .count();
  for (size_t at = 0; at < Passes_.size(); ++at) {
    auto &pass = Passes_[at];
    const auto &ticks = values[at];
    pass.Available = (result == VK_SUCCESS || result == VK_NOT_READY) && ticks[1] != 0 &&
                     ticks[3] != 0 &&
                     TimestampIntervalFits(ticks[0], ticks[2], bits, period, upperNs);
    if (pass.Available) { pass.GpuNs = static_cast<double>((ticks[2] - ticks[0]) & mask) * period; }
    if (pass.Raster && Primitives_ != VK_NULL_HANDLE) {
      std::array<uint64_t, 2> count{};
      const auto read =
          vkGetQueryPoolResults(Context_.Logical,
                                Primitives_,
                                static_cast<uint32_t>(at),
                                1,
                                sizeof(count),
                                count.data(),
                                sizeof(count),
                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
      if (read == VK_SUCCESS && count[1] != 0) { pass.InputPrimitives = count[0]; }
    }
  }
  ProfiledSubmission submission{
      .Serial = serial, .Dropped = Dropped_, .Passes = std::move(Passes_)};
  if (Session_->Submissions.size() < kMaximumSubmissions) {
    Session_->Submissions.push_back(std::move(submission));
  } else {
    Session_->Submissions[Session_->Next] = std::move(submission);
    Session_->Next = (Session_->Next + 1) % kMaximumSubmissions;
    ++Session_->Dropped;
  }
}

}
#endif
