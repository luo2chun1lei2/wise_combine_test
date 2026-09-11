#pragma once

#include <string>
#include <vector>

#include "model.h"
#include "sequence_generator.h"

namespace harness {

std::string generate(const model::Model &model, const std::vector<gen::Sequence> &sequences,
                     bool dylib = false, bool jsonFailures = false);

}  // namespace harness
