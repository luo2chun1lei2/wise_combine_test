#include "guard.h"

#include <cctype>
#include <cstdlib>
#include <functional>
#include <vector>

namespace guard {

namespace {

struct Token {
  std::string text;
  std::string kind;  // "id" | "num" | "str" | "op"
};

bool isNumber(const std::string &s) {
  if (s.empty()) {
    return false;
  }
  std::size_t i = (s[0] == '-') ? 1 : 0;
  if (i == s.size()) {
    return false;
  }
  for (; i < s.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
      return false;
    }
  }
  return true;
}

bool tokenize(const std::string &s, std::vector<Token> &out) {
  out.clear();
  for (std::size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (std::isspace(c)) {
      ++i;
      continue;
    }
    if (std::isalpha(c) || c == '_') {
      std::size_t j = i;
      while (j < s.size() &&
             (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) {
        ++j;
      }
      out.push_back({s.substr(i, j - i), "id"});
      i = j;
    } else if (std::isdigit(c)) {
      std::size_t j = i;
      while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) {
        ++j;
      }
      out.push_back({s.substr(i, j - i), "num"});
      i = j;
    } else if (c == '"') {
      std::size_t j = i + 1;
      while (j < s.size() && s[j] != '"') {
        ++j;
      }
      if (j >= s.size()) {
        return false;
      }
      out.push_back({s.substr(i, j - i + 1), "str"});
      i = j + 1;
    } else if (i + 1 < s.size() &&
               (s.compare(i, 2, "==") == 0 || s.compare(i, 2, "!=") == 0 ||
                s.compare(i, 2, ">=") == 0 || s.compare(i, 2, "<=") == 0 ||
                s.compare(i, 2, "&&") == 0 || s.compare(i, 2, "||") == 0)) {
      out.push_back({s.substr(i, 2), "op"});
      i += 2;
    } else if (c == '>' || c == '<' || c == '(' || c == ')') {
      out.push_back({std::string(1, c), "op"});
      ++i;
    } else {
      return false;
    }
  }
  return true;
}

bool compareValues(const std::string &left, const std::string &right,
                   const std::string &op) {
  if (op == "==") {
    return left == right;
  }
  if (op == "!=") {
    return left != right;
  }
  if (isNumber(left) && isNumber(right)) {
    const long long a = std::stoll(left);
    const long long b = std::stoll(right);
    if (op == ">") return a > b;
    if (op == ">=") return a >= b;
    if (op == "<") return a < b;
    if (op == "<=") return a <= b;
    return false;
  }
  if (op == ">") return left > right;
  if (op == ">=") return left >= right;
  if (op == "<") return left < right;
  if (op == "<=") return left <= right;
  return false;
}

bool isComparisonOp(const Token &token) {
  return token.kind == "op" &&
         (token.text == "==" || token.text == "!=" || token.text == ">" ||
          token.text == ">=" || token.text == "<" || token.text == "<=");
}

}  // namespace

bool evalGuard(const std::string &expr,
               const std::map<std::string, std::string> &values,
               std::string *error) {
  if (error != nullptr) {
    error->clear();
  }
  if (expr.empty()) {
    return true;
  }

  std::vector<Token> tokens;
  if (!tokenize(expr, tokens)) {
    if (error != nullptr) {
      *error = "malformed guard expression";
    }
    return false;
  }

  std::size_t index = 0;
  bool failed = false;
  auto fail = [&](const std::string &message) {
    if (!failed) {
      failed = true;
      if (error != nullptr) {
        *error = message;
      }
    }
  };

  std::function<bool()> parseOr;
  std::function<bool()> parseAnd;
  std::function<bool()> parsePrimary;

  parsePrimary = [&]() -> bool {
    if (index >= tokens.size()) {
      fail("incomplete guard expression");
      return false;
    }

    if (tokens[index].kind == "op" && tokens[index].text == "(") {
      ++index;
      const bool value = parseOr();
      if (index < tokens.size() && tokens[index].kind == "op" &&
          tokens[index].text == ")") {
        ++index;
      } else {
        fail("missing ')' in guard expression");
      }
      return value;
    }

    if (tokens[index].kind != "id") {
      fail("expected variable in guard expression");
      return false;
    }
    const Token left = tokens[index++];

    if (index >= tokens.size() || !isComparisonOp(tokens[index])) {
      fail("expected comparison operator in guard expression");
      return false;
    }
    const std::string op = tokens[index++].text;

    if (index >= tokens.size()) {
      fail("missing right operand in guard expression");
      return false;
    }
    const Token right = tokens[index++];

    std::string rightValue;
    if (right.kind == "num") {
      rightValue = right.text;
    } else if (right.kind == "str") {
      rightValue = right.text.substr(1, right.text.size() - 2);
    } else if (right.kind == "id") {
      const auto it = values.find(right.text);
      rightValue = (it != values.end()) ? it->second : right.text;
    } else {
      fail("invalid right operand in guard expression");
      return false;
    }

    const auto leftIt = values.find(left.text);
    if (leftIt == values.end()) {
      fail("guard variable '" + left.text + "' is unbound");
      return false;
    }

    return compareValues(leftIt->second, rightValue, op);
  };

  parseAnd = [&]() -> bool {
    bool value = parsePrimary();
    while (index < tokens.size() && tokens[index].kind == "op" &&
           tokens[index].text == "&&") {
      ++index;
      value = parsePrimary() && value;
    }
    return value;
  };

  parseOr = [&]() -> bool {
    bool value = parseAnd();
    while (index < tokens.size() && tokens[index].kind == "op" &&
           tokens[index].text == "||") {
      ++index;
      value = parseAnd() || value;
    }
    return value;
  };

  const bool result = parseOr();
  if (failed) {
    return false;
  }
  return result;
}

}  // namespace guard
