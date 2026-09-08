#pragma once
#include "github_pull.h"
namespace bootstrap { namespace github {
// No network, TLS or target-resource claim comes from the main.cpp adapter fake.
class Worker final : public Port {
 public:
  bool start(const CheckRequest&) override { return false; }
  bool take(Message&) override { return false; }
  void reply(uint32_t, bool) override {}
  void cancel() override {}
};
}}
