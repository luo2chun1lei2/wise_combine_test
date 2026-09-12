#pragma once

#include <map>
#include <string>
#include <vector>

#include "model.h"
#include "sequence_generator.h"
#include "state_machine_model.h"

namespace harness {

std::string generate(const model::Model &model, const std::vector<gen::Sequence> &sequences,
                     bool dylib = false, bool jsonFailures = false,
                     int timeoutSeconds = 10);

std::string generateStateMachine(const smodel::StateMachine &machine,
                                 const std::vector<std::string> &events,
                                 const std::map<std::string, std::string> &guardValues = {},
                                 int timeoutSeconds = 10);

}  // namespace harness
