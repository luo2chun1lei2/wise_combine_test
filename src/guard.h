#pragma once

#include <map>
#include <string>

namespace guard {

// Evaluates a state-machine guard expression against a set of bound variables.
//
// Returns true when the guard is satisfied. Returns false when the guard is
// legitimately false, or when it references an unbound variable / is malformed.
// In the latter two cases a non-empty error message is written to `error`.
bool evalGuard(const std::string &expr,
               const std::map<std::string, std::string> &values,
               std::string *error);

}  // namespace guard
