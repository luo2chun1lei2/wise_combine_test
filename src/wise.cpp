#include "wise.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace wct {

namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string token;
    while (in >> token) {
        out.push_back(token);
    }
    return out;
}

std::vector<std::string> split_commas(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty() || !s.empty()) {
        out.push_back(trim(cur));
    }
    return out;
}

bool starts_with(const std::string& s, const std::string& p) {
    return s.rfind(p, 0) == 0;
}

std::string base_name(const std::string& s) {
    const std::size_t open = s.find('(');
    return open == std::string::npos ? s : trim(s.substr(0, open));
}

struct CountConstraint {
    std::string func;
    std::string op;
    long value = 0;
};

bool parse_count_constraint(const std::string& expr, CountConstraint& out,
                            std::string& err) {
    const std::string t = trim(expr);
    const std::string prefix = "count(";
    if (!starts_with(t, prefix)) {
        err = "constraint must start with count(";
        return false;
    }
    const std::size_t close = t.find(')', prefix.size());
    if (close == std::string::npos) {
        err = "constraint missing ')'";
        return false;
    }
    out.func = trim(t.substr(prefix.size(), close - prefix.size()));
    if (out.func.empty()) {
        err = "constraint function name is empty";
        return false;
    }
    const std::string rest = trim(t.substr(close + 1));
    const auto tokens = split_ws(rest);
    if (tokens.size() != 2) {
        err = "constraint must be: count(<func>) <op> <number>";
        return false;
    }
    out.op = tokens[0];
    const std::set<std::string> valid_ops{"<", "<=", ">", ">=", "==", "!="};
    if (!valid_ops.count(out.op)) {
        err = "unknown constraint operator: " + out.op;
        return false;
    }
    try {
        out.value = std::stol(tokens[1]);
    } catch (...) {
        err = "invalid constraint number: " + tokens[1];
        return false;
    }
    return true;
}

bool parse_constraint_expr(const std::string& expr,
                           std::vector<CountConstraint>& out,
                           std::string& err) {
    const std::string t = trim(expr);
    if (t.empty()) {
        err = "constraint expression is empty";
        return false;
    }
    std::size_t begin = 0;
    while (begin < t.size()) {
        std::size_t end = t.find(" and ", begin);
        const std::string part =
            trim(t.substr(begin, end == std::string::npos ? std::string::npos
                                                          : end - begin));
        CountConstraint cc;
        if (!parse_count_constraint(part, cc, err)) {
            return false;
        }
        out.push_back(cc);
        if (end == std::string::npos) {
            break;
        }
        begin = end + 5;
    }
    return !out.empty();
}

std::string now_string() {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string json_escape(const std::string& s) {
    std::ostringstream out;
    for (const unsigned char c : s) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20U) {
                    out << "\\u00" << std::hex << std::setw(2)
                        << std::setfill('0') << static_cast<int>(c) << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    long number = 0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)), pos_(0) {}

    bool parse(JsonValue& out, std::string& err) {
        skip_ws();
        if (!parse_value(out, err)) {
            return false;
        }
        skip_ws();
        if (pos_ != text_.size()) {
            err = "trailing content after JSON value";
            return false;
        }
        return true;
    }

private:
    std::string text_;
    std::size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool fail(const std::string& message, std::string& err) {
        err = message + " at byte " + std::to_string(pos_);
        return false;
    }

    bool parse_value(JsonValue& out, std::string& err) {
        skip_ws();
        if (pos_ >= text_.size()) {
            return fail("unexpected end of JSON", err);
        }
        const char c = text_[pos_];
        if (c == '{') {
            return parse_object(out, err);
        }
        if (c == '[') {
            return parse_array(out, err);
        }
        if (c == '"') {
            out.type = JsonValue::Type::String;
            return parse_string(out.string, err);
        }
        if (c == 't' || c == 'f') {
            out.type = JsonValue::Type::Bool;
            if (starts_with(text_.substr(pos_), "true")) {
                out.boolean = true;
                pos_ += 4;
                return true;
            }
            if (starts_with(text_.substr(pos_), "false")) {
                out.boolean = false;
                pos_ += 5;
                return true;
            }
            return fail("invalid literal", err);
        }
        if (c == 'n') {
            out.type = JsonValue::Type::Null;
            if (starts_with(text_.substr(pos_), "null")) {
                pos_ += 4;
                return true;
            }
            return fail("invalid literal", err);
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            out.type = JsonValue::Type::Number;
            return parse_number(out.number, err);
        }
        return fail(std::string("unexpected character: ") + c, err);
    }

    bool parse_object(JsonValue& out, std::string& err) {
        out.type = JsonValue::Type::Object;
        ++pos_;
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (pos_ < text_.size()) {
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return fail("object key must be a string", err);
            }
            std::string key;
            if (!parse_string(key, err)) {
                return false;
            }
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return fail("expected ':' after object key", err);
            }
            ++pos_;
            JsonValue value;
            if (!parse_value(value, err)) {
                return false;
            }
            if (!out.object.emplace(std::move(key), std::move(value)).second) {
                return fail("duplicate object key", err);
            }
            skip_ws();
            if (pos_ >= text_.size()) {
                return fail("unterminated object", err);
            }
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or '}' in object", err);
        }
        return fail("unterminated object", err);
    }

    bool parse_array(JsonValue& out, std::string& err) {
        out.type = JsonValue::Type::Array;
        ++pos_;
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (pos_ < text_.size()) {
            JsonValue value;
            if (!parse_value(value, err)) {
                return false;
            }
            out.array.push_back(std::move(value));
            skip_ws();
            if (pos_ >= text_.size()) {
                return fail("unterminated array", err);
            }
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or ']' in array", err);
        }
        return fail("unterminated array", err);
    }

    bool parse_string(std::string& out, std::string& err) {
        if (pos_ >= text_.size() || text_[pos_] != '"') {
            return fail("expected string", err);
        }
        ++pos_;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20U) {
                return fail("unescaped control character in string", err);
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return fail("unterminated escape sequence", err);
            }
            const char esc = text_[pos_++];
            switch (esc) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    if (pos_ + 4 > text_.size()) {
                        return fail("invalid unicode escape", err);
                    }
                    const std::string hex = text_.substr(pos_, 4);
                    unsigned long cp = 0;
                    try {
                        cp = std::stoul(hex, nullptr, 16);
                    } catch (...) {
                        return fail("invalid unicode escape", err);
                    }
                    pos_ += 4;
                    if (cp <= 0x7F) {
                        out.push_back(static_cast<char>(cp));
                    } else if (cp <= 0x7FF) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default:
                    return fail("invalid escape character", err);
            }
        }
        return fail("unterminated string", err);
    }

    bool parse_number(long& out, std::string& err) {
        const std::size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') {
            ++pos_;
        }
        const std::size_t digit_start = pos_;
        while (pos_ < text_.size() &&
               std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ == digit_start) {
            return fail("invalid number", err);
        }
        if (pos_ < text_.size() &&
            (text_[pos_] == '.' || text_[pos_] == 'e' ||
             text_[pos_] == 'E')) {
            return fail("non-integer numbers are not supported", err);
        }
        try {
            out = std::stol(text_.substr(start, pos_ - start));
        } catch (...) {
            return fail("number out of range", err);
        }
        return true;
    }
};

bool parse_adapter_response(const std::string& text, std::string& status,
                            long& return_value, bool& has_return,
                            std::map<std::string, std::string>& returns,
                            std::string& stdout_text, std::string& err) {
    JsonValue root;
    JsonParser parser(text);
    if (!parser.parse(root, err)) {
        return false;
    }
    if (root.type != JsonValue::Type::Object) {
        err = "adapter response must be a JSON object";
        return false;
    }
    const auto protocol = root.object.find("protocol");
    if (protocol == root.object.end() ||
        protocol->second.type != JsonValue::Type::Number ||
        protocol->second.number != 1) {
        err = "adapter response protocol must be 1";
        return false;
    }
    const auto status_it = root.object.find("status");
    if (status_it == root.object.end() ||
        status_it->second.type != JsonValue::Type::String) {
        err = "adapter response status must be a string";
        return false;
    }
    status = status_it->second.string;
    const auto returns_it = root.object.find("returns");
    if (returns_it == root.object.end() ||
        returns_it->second.type != JsonValue::Type::Object) {
        err = "adapter response returns must be an object";
        return false;
    }
    for (const auto& [name, value] : returns_it->second.object) {
        if (value.type != JsonValue::Type::String) {
            err = "adapter return value '" + name + "' must be a string";
            return false;
        }
        returns[name] = value.string;
    }
    const auto stdout_it = root.object.find("stdout");
    if (stdout_it == root.object.end() ||
        stdout_it->second.type != JsonValue::Type::String) {
        err = "adapter response stdout must be a string";
        return false;
    }
    stdout_text = stdout_it->second.string;
    const auto return_it = root.object.find("return");
    if (return_it != root.object.end()) {
        if (return_it->second.type != JsonValue::Type::Number) {
            err = "adapter response return must be an integer";
            return false;
        }
        return_value = return_it->second.number;
        has_return = true;
    }
    return true;
}

std::string make_adapter_request(
    const std::string& flow_id, std::size_t step, const std::string& function,
    const std::map<std::string, std::string>& args,
    const std::map<std::string, std::string>& arg_types,
    const std::string& return_type) {
    std::ostringstream out;
    out << "{\"protocol\":1,\"flow_id\":\"" << json_escape(flow_id)
        << "\",\"step\":" << step << ",\"function\":\"" << json_escape(function)
        << "\",\"return_type\":\"" << json_escape(return_type)
        << "\",\"args\":{";
    bool first = true;
    for (const auto& [name, value] : args) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << json_escape(name) << "\":\"" << json_escape(value) << '"';
    }
    out << "},\"arg_types\":{";
    first = true;
    for (const auto& [name, type] : arg_types) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << json_escape(name) << "\":\"" << json_escape(type) << '"';
    }
    out << "}}\n";
    return out.str();
}

} // namespace

bool parse_guard(const std::string& text, GuardExpr& out, std::string& err) {
    const std::string t = trim(text);
    if (t.empty()) {
        err = "guard expression is empty";
        return false;
    }
    const std::string prefix = "return";
    if (!starts_with(t, prefix)) {
        err = "guard must start with return";
        return false;
    }
    const std::string rest = trim(t.substr(prefix.size()));
    if (rest.empty()) {
        err = "guard missing operator and value";
        return false;
    }
    const std::set<std::string> ops{"==", "!=", "<=", ">=", "<", ">"};
    std::string op;
    std::string value;
    bool found = false;
    for (const auto& candidate : ops) {
        if (starts_with(rest, candidate)) {
            op = candidate;
            value = trim(rest.substr(candidate.size()));
            found = true;
            break;
        }
    }
    if (!found) {
        err = "unknown guard operator";
        return false;
    }
    if (value.empty()) {
        err = "guard missing value";
        return false;
    }
    try {
        out.value = std::stol(value);
    } catch (...) {
        err = "invalid guard value: " + value;
        return false;
    }
    out.op = op;
    return true;
}

bool guard_satisfied(const GuardExpr& guard, int return_value) {
    const long actual = static_cast<long>(return_value);
    if (guard.op == "==") {
        return actual == guard.value;
    }
    if (guard.op == "!=") {
        return actual != guard.value;
    }
    if (guard.op == "<") {
        return actual < guard.value;
    }
    if (guard.op == "<=") {
        return actual <= guard.value;
    }
    if (guard.op == ">") {
        return actual > guard.value;
    }
    if (guard.op == ">=") {
        return actual >= guard.value;
    }
    return false;
}

Parser::Parser(std::string file) : file_(std::move(file)) {}

std::string Parser::peek_line() const {
    return pos_ < lines_.size() ? lines_[pos_] : std::string();
}

std::string Parser::next_line() {
    return pos_ < lines_.size() ? lines_[pos_++] : std::string();
}

void Parser::fail(int line, const std::string& message) const {
    throw ParseError{file_, line, message};
}

void Parser::skip_empty_or_comment() {
    while (pos_ < lines_.size()) {
        const std::string t = trim(lines_[pos_]);
        if (t.empty() || starts_with(t, "#")) {
            ++pos_;
        } else {
            break;
        }
    }
}

Spec Parser::parse() {
    std::ifstream in(file_);
    if (!in) {
        throw ParseError{file_, 0, "cannot open file: " + file_};
    }
    std::string line;
    while (std::getline(in, line)) {
        lines_.push_back(line);
    }

    Spec spec;
    while (pos_ < lines_.size()) {
        skip_empty_or_comment();
        if (pos_ >= lines_.size()) {
            break;
        }
        const int line_no = static_cast<int>(pos_ + 1);
        const std::string text = trim(lines_[pos_]);
        if (starts_with(text, "object ")) {
            spec.objects.push_back(parse_object(line_no));
        } else if (starts_with(text, "function ")) {
            spec.functions.push_back(parse_function(text, line_no));
            ++pos_;
        } else if (starts_with(text, "parameter ")) {
            parse_parameter(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "order ")) {
            parse_order(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "mutex ")) {
            parse_mutex(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "parallel ")) {
            parse_parallel(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "constraint ")) {
            parse_constraint(text, line_no, spec);
            ++pos_;
        } else if (text == "}") {
            fail(line_no, "unexpected closing brace");
        } else {
            fail(line_no, "unrecognized top-level statement: " + text);
        }
    }
    return spec;
}

ObjectDecl Parser::parse_object(int line) {
    const std::string text = trim(lines_[pos_]);
    const std::string rest = trim(text.substr(std::string("object").size()));
    const std::size_t brace = rest.find('{');
    if (brace == std::string::npos) {
        fail(line, "expected '{' after object name");
    }
    const std::string name = trim(rest.substr(0, brace));
    if (name.empty()) {
        fail(line, "object name is empty");
    }
    ObjectDecl object;
    object.name = name;
    object.line = line;
    ++pos_;
    parse_object_body(object);
    return object;
}

void Parser::parse_object_body(ObjectDecl& object) {
    std::unordered_set<std::string> seen_states;
    while (pos_ < lines_.size()) {
        const int line = static_cast<int>(pos_ + 1);
        const std::string text = trim(lines_[pos_]);
        if (text.empty() || starts_with(text, "#")) {
            ++pos_;
            continue;
        }
        if (text == "}") {
            ++pos_;
            return;
        }
        const auto tokens = split_ws(text);
        if (tokens.empty()) {
            ++pos_;
            continue;
        }
        if (tokens[0] == "state") {
            if (tokens.size() < 2) {
                fail(line, "state requires a name");
            }
            StateDecl state;
            state.name = tokens[1];
            state.line = line;
            for (std::size_t i = 2; i < tokens.size(); ++i) {
                if (tokens[i] == "initial") {
                    state.initial = true;
                } else if (tokens[i] == "final") {
                    state.final = true;
                } else {
                    fail(line, "unknown state modifier: " + tokens[i]);
                }
            }
            if (!seen_states.insert(state.name).second) {
                fail(line, "duplicate state: " + state.name);
            }
            object.states.push_back(state);
            ++pos_;
        } else if (tokens[0] == "transition") {
            TransitionDecl tr;
            tr.line = line;
            std::size_t i = 1;
            if (i >= tokens.size()) {
                fail(line, "transition missing source state");
            }
            tr.src = tokens[i++];
            if (i >= tokens.size() || tokens[i] != "->") {
                fail(line, "transition missing '->'");
            }
            ++i;
            if (i >= tokens.size()) {
                fail(line, "transition missing target state");
            }
            tr.dst = tokens[i++];
            if (i >= tokens.size() || tokens[i] != "by") {
                fail(line, "transition missing 'by'");
            }
            ++i;
            if (i >= tokens.size()) {
                fail(line, "transition missing function");
            }
            tr.func = base_name(tokens[i++]);
            while (i < tokens.size()) {
                if (tokens[i] == "expect") {
                    ++i;
                    if (i >= tokens.size()) {
                        fail(line, "transition expect missing value");
                    }
                    try {
                        tr.expect_return = std::stol(tokens[i]);
                    } catch (...) {
                        fail(line, "transition expect must be an integer: " +
                                        tokens[i]);
                    }
                    tr.expect_present = true;
                    ++i;
                } else if (tokens[i] == "expect_output") {
                    ++i;
                    if (i >= tokens.size()) {
                        fail(line, "transition expect_output missing value");
                    }
                    tr.expect_output = tokens[i];
                    ++i;
                    if (tr.expect_output.size() >= 1 &&
                        tr.expect_output.front() == '"' &&
                        !(tr.expect_output.size() >= 2 &&
                          tr.expect_output.back() == '"')) {
                        while (i < tokens.size()) {
                            tr.expect_output += " " + tokens[i];
                            const bool closing = !tokens[i].empty() &&
                                                 tokens[i].back() == '"';
                            ++i;
                            if (closing) {
                                break;
                            }
                        }
                    }
                    if (tr.expect_output.size() >= 2 &&
                        ((tr.expect_output.front() == '"' &&
                          tr.expect_output.back() == '"') ||
                         (tr.expect_output.front() == '\'' &&
                          tr.expect_output.back() == '\''))) {
                        tr.expect_output = tr.expect_output.substr(
                            1, tr.expect_output.size() - 2);
                    }
                    tr.expect_output_present = true;
                } else if (tokens[i] == "guard") {
                    ++i;
                    std::ostringstream g;
                    while (i < tokens.size()) {
                        if (!g.str().empty()) {
                            g << ' ';
                        }
                        g << tokens[i++];
                    }
                    tr.guard = g.str();
                } else {
                    fail(line, "unexpected transition token: " + tokens[i]);
                }
            }
            object.transitions.push_back(tr);
            ++pos_;
        } else {
            fail(line, "unrecognized object statement: " + text);
        }
    }
    fail(object.line, "unterminated object block");
}

FunctionDecl Parser::parse_function(const std::string& text, int line) {
    const std::string rest = trim(text.substr(std::string("function").size()));
    const std::size_t open = rest.find('(');
    if (open == std::string::npos) {
        fail(line, "function declaration missing '('");
    }
    const std::size_t close = rest.find(')', open);
    if (close == std::string::npos) {
        fail(line, "function declaration missing ')'");
    }
    FunctionDecl fn;
    fn.name = trim(rest.substr(0, open));
    fn.line = line;
    if (fn.name.empty()) {
        fail(line, "function name is empty");
    }
    const std::string params = rest.substr(open + 1, close - open - 1);
    if (!trim(params).empty()) {
        for (const auto& part : split_commas(params)) {
            if (part.empty()) {
                continue;
            }
            const auto tokens = split_ws(part);
            Param p;
            if (tokens.size() == 1) {
                p.name = tokens[0];
            } else if (tokens.size() == 2) {
                p.type = tokens[0];
                p.name = tokens[1];
            } else {
                fail(line, "invalid parameter declaration: " + part);
            }
            fn.params.push_back(p);
        }
    }
    const std::size_t arrow = rest.find("->", close);
    if (arrow != std::string::npos) {
        fn.return_param = trim(rest.substr(arrow + 2));
        if (fn.return_param.empty()) {
            fail(line, "return parameter is empty");
        }
    }
    return fn;
}

void Parser::parse_parameter(const std::string& text, int line, Spec& spec) {
    const std::string rest = trim(text.substr(std::string("parameter").size()));
    const std::size_t eq = rest.find('=');
    if (eq == std::string::npos) {
        fail(line, "parameter relation missing '='");
    }
    const std::string lhs = trim(rest.substr(0, eq));
    const std::string rhs = trim(rest.substr(eq + 1));
    const std::size_t ldot = lhs.find('.');
    const std::size_t rdot = rhs.find('.');
    if (ldot == std::string::npos) {
        fail(line, "parameter relation lhs must use func.param format");
    }
    ParameterRel rel;
    rel.line = line;
    rel.lhs_func = trim(lhs.substr(0, ldot));
    rel.lhs_param = trim(lhs.substr(ldot + 1));
    if (rel.lhs_func.empty() || rel.lhs_param.empty()) {
        fail(line, "parameter relation has an empty function or parameter name");
    }
    if (rdot == std::string::npos) {
        rel.rhs_is_const = true;
        rel.rhs_const = rhs;
        if (rel.rhs_const.size() >= 2 &&
            ((rel.rhs_const.front() == '"' && rel.rhs_const.back() == '"') ||
             (rel.rhs_const.front() == '\'' && rel.rhs_const.back() == '\''))) {
            rel.rhs_const = rel.rhs_const.substr(1, rel.rhs_const.size() - 2);
        }
        if (rel.rhs_const.empty()) {
            fail(line, "parameter constant is empty");
        }
    } else {
        rel.rhs_func = trim(rhs.substr(0, rdot));
        rel.rhs_param = trim(rhs.substr(rdot + 1));
        if (rel.rhs_func.empty() || rel.rhs_param.empty()) {
            fail(line, "parameter relation has an empty source function or parameter name");
        }
    }
    spec.parameters.push_back(rel);
}

void Parser::parse_order(const std::string& text, int line, Spec& spec) {
    const auto tokens = split_ws(trim(text.substr(std::string("order").size())));
    if (tokens.size() < 3 ||
        (tokens[1] != "before" && tokens[1] != "after")) {
        fail(line, "order must be: order <a> before|after <b> [if <func>]");
    }
    OrderRel rel;
    rel.line = line;
    if (tokens[1] == "before") {
        rel.before = tokens[0];
        rel.after = tokens[2];
    } else {
        rel.before = tokens[2];
        rel.after = tokens[0];
    }
    if (tokens.size() > 3) {
        if (tokens.size() != 5 || tokens[3] != "if" || tokens[4].empty()) {
            fail(line, "order condition must be: if <func>");
        }
        rel.condition = tokens[4];
    }
    spec.orders.push_back(rel);
}

void Parser::parse_mutex(const std::string& text, int line, Spec& spec) {
    const auto tokens = split_ws(trim(text.substr(std::string("mutex").size())));
    if (tokens.size() < 2) {
        fail(line, "mutex must list at least two functions");
    }
    MutexRel rel;
    rel.line = line;
    rel.funcs = tokens;
    spec.mutexes.push_back(rel);
}

void Parser::parse_parallel(const std::string& text, int line, Spec& spec) {
    const auto tokens = split_ws(trim(text.substr(std::string("parallel").size())));
    if (tokens.size() != 2) {
        fail(line, "parallel must list exactly two functions");
    }
    ParallelRel rel;
    rel.line = line;
    rel.a = tokens[0];
    rel.b = tokens[1];
    spec.parallels.push_back(rel);
}

void Parser::parse_constraint(const std::string& text, int line, Spec& spec) {
    const std::string expr = trim(text.substr(std::string("constraint").size()));
    if (expr.empty()) {
        fail(line, "constraint expression is empty");
    }
    if (starts_with(expr, "state ") || expr == "state") {
        const std::string state = trim(expr.substr(std::string("state").size()));
        if (state.empty()) {
            fail(line, "constraint state is empty");
        }
        spec.state_constraints.push_back(state);
        return;
    }
    if (starts_with(expr, "value(")) {
        const std::size_t close = expr.find(')', std::string("value(").size());
        if (close == std::string::npos) {
            fail(line, "constraint value missing ')'");
        }
        const std::string inner =
            trim(expr.substr(std::string("value(").size(),
                             close - std::string("value(").size()));
        const std::size_t dot = inner.find('.');
        if (dot == std::string::npos) {
            fail(line, "constraint value must use func.param format");
        }
        ValueConstraintRel rel;
        rel.line = line;
        rel.func = trim(inner.substr(0, dot));
        rel.param = trim(inner.substr(dot + 1));
        if (rel.func.empty() || rel.param.empty()) {
            fail(line, "constraint value has an empty function or parameter name");
        }
        const std::string rest = trim(expr.substr(close + 1));
        const auto tokens = split_ws(rest);
        if (tokens.size() != 2 || (tokens[0] != "==" && tokens[0] != "!=")) {
            fail(line, "constraint value must be: value(<func>.<param>) ==|!= <literal>");
        }
        rel.op = tokens[0];
        rel.value = tokens[1];
        if (rel.value.size() >= 2 &&
            ((rel.value.front() == '"' && rel.value.back() == '"') ||
             (rel.value.front() == '\'' && rel.value.back() == '\''))) {
            rel.value = rel.value.substr(1, rel.value.size() - 2);
        }
        spec.value_constraints.push_back(rel);
        return;
    }
    ConstraintRel rel;
    rel.line = line;
    rel.expr = expr;
    spec.constraints.push_back(rel);
}

Model::Model(Spec spec) : spec_(std::move(spec)) {
    for (std::size_t i = 0; i < spec_.functions.size(); ++i) {
        const std::string& name = spec_.functions[i].name;
        if (!function_index_.emplace(name, i).second) {
            throw ModelError{"duplicate function: " + name};
        }
    }
    for (std::size_t i = 0; i < spec_.objects.size(); ++i) {
        const std::string& name = spec_.objects[i].name;
        if (!object_index_.emplace(name, i).second) {
            throw ModelError{"duplicate object: " + name};
        }
    }
}

void Model::validate() {
    for (const auto& object : spec_.objects) {
        std::size_t initial_count = 0;
        std::unordered_set<std::string> states;
        for (const auto& state : object.states) {
            if (!states.insert(state.name).second) {
                throw ModelError{"duplicate state in object " + object.name +
                                 ": " + state.name};
            }
            if (state.initial) {
                ++initial_count;
            }
        }
        if (initial_count != 1) {
            throw ModelError{"object " + object.name +
                             " must have exactly one initial state"};
        }
        for (const auto& tr : object.transitions) {
            if (!states.count(tr.src) || !states.count(tr.dst)) {
                throw ModelError{"transition references unknown state in " +
                                 object.name + ": " + tr.src + " -> " + tr.dst};
            }
            if (!has_function(tr.func)) {
                throw ModelError{"transition references unknown function: " +
                                 tr.func};
            }
            if (!tr.guard.empty()) {
                GuardExpr guard;
                std::string err;
                if (!parse_guard(tr.guard, guard, err)) {
                    throw ModelError{"invalid guard on transition " + tr.src +
                                     " -> " + tr.dst + ": " + err};
                }
            }
        }
    }

    std::unordered_set<std::string> param_keys;
    for (const auto& rel : spec_.parameters) {
        if (!has_function(rel.lhs_func)) {
            throw ModelError{"parameter relation references unknown function: " +
                             rel.lhs_func};
        }
        const FunctionDecl& lhs = function(rel.lhs_func);
        const Param* lhs_param = nullptr;
        for (const auto& p : lhs.params) {
            if (p.name == rel.lhs_param) {
                lhs_param = &p;
                break;
            }
        }
        if (!lhs_param) {
            throw ModelError{"unknown parameter " + rel.lhs_func + "." +
                             rel.lhs_param};
        }

        const std::string lhs_key = rel.lhs_func + "." + rel.lhs_param;
        if (!param_keys.insert(lhs_key).second) {
            throw ModelError{"parameter " + lhs_key + " has multiple sources"};
        }

        if (rel.rhs_is_const) {
            continue;
        }

        if (!has_function(rel.rhs_func)) {
            throw ModelError{"parameter relation references unknown function: " +
                             rel.rhs_func};
        }
        const FunctionDecl& rhs = function(rel.rhs_func);
        const Param* rhs_param = nullptr;
        for (const auto& p : rhs.params) {
            if (p.name == rel.rhs_param) {
                rhs_param = &p;
                break;
            }
        }
        if (rhs_param) {
            if (!lhs_param->type.empty() && !rhs_param->type.empty() &&
                lhs_param->type != rhs_param->type) {
                throw ModelError{"parameter type mismatch: " + lhs_key +
                                 " (" + lhs_param->type + ") <- " +
                                 rel.rhs_func + "." + rel.rhs_param +
                                 " (" + rhs_param->type + ")"};
            }
        } else if (rhs.return_param == rel.rhs_param) {
            const std::string rhs_type =
                rhs.return_type.empty() ? rhs.return_param : rhs.return_type;
            if (!lhs_param->type.empty() && !rhs_type.empty() &&
                lhs_param->type != rhs_type) {
                throw ModelError{"parameter type mismatch: " + lhs_key +
                                 " (" + lhs_param->type + ") <- " +
                                 rel.rhs_func + "." + rel.rhs_param +
                                 " (" + rhs_type + ")"};
            }
        } else {
            throw ModelError{"unknown source parameter " + rel.rhs_func + "." +
                             rel.rhs_param};
        }
    }

    std::unordered_map<std::string, std::vector<std::string>> param_deps;
    for (const auto& rel : spec_.parameters) {
        if (rel.rhs_is_const) {
            continue;
        }
        const FunctionDecl& rhs = function(rel.rhs_func);
        bool rhs_is_param = false;
        for (const auto& p : rhs.params) {
            if (p.name == rel.rhs_param) {
                rhs_is_param = true;
                break;
            }
        }
        if (!rhs_is_param) {
            continue;
        }
        const std::string from = rel.lhs_func + "." + rel.lhs_param;
        const std::string to = rel.rhs_func + "." + rel.rhs_param;
        param_deps[from].push_back(to);
    }
    std::unordered_map<std::string, unsigned char> marks;
    std::function<void(const std::string&)> visit_param =
        [&](const std::string& node) {
            const unsigned char mark = marks[node];
            if (mark == 1U) {
                throw ModelError{"parameter relations contain a cycle"};
            }
            if (mark == 2U) {
                return;
            }
            marks[node] = 1U;
            const auto it = param_deps.find(node);
            if (it != param_deps.end()) {
                for (const auto& next : it->second) {
                    visit_param(next);
                }
            }
            marks[node] = 2U;
        };
    for (const auto& [key, value] : param_deps) {
        static_cast<void>(value);
        visit_param(key);
    }

    for (const auto& fn : spec_.functions) {
        for (const auto& param : fn.params) {
            const std::string key = fn.name + "." + param.name;
            if (param_keys.count(key) == 0) {
                throw ModelError{"parameter " + key + " has no source"};
            }
        }
    }

    std::unordered_map<std::string, std::size_t> indeg;
    for (const auto& fn : spec_.functions) {
        indeg[fn.name] = 0;
    }
    std::unordered_map<std::string, std::vector<std::string>> edges;
    for (const auto& rel : spec_.orders) {
        if (!has_function(rel.before) || !has_function(rel.after)) {
            throw ModelError{"order references unknown function"};
        }
        if (!rel.condition.empty() && !has_function(rel.condition)) {
            throw ModelError{"order condition references unknown function: " +
                             rel.condition};
        }
        if (!rel.condition.empty()) {
            continue;
        }
        edges[rel.before].push_back(rel.after);
        ++indeg[rel.after];
    }
    std::vector<std::string> ready;
    for (const auto& [name, deg] : indeg) {
        if (deg == 0) {
            ready.push_back(name);
        }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const std::string name = ready.back();
        ready.pop_back();
        ++visited;
        for (const auto& nxt : edges[name]) {
            if (--indeg[nxt] == 0) {
                ready.push_back(nxt);
            }
        }
    }
    if (visited != spec_.functions.size()) {
        throw ModelError{"order relations contain a cycle"};
    }

    for (const auto& rel : spec_.mutexes) {
        std::unordered_set<std::string> seen;
        for (const auto& name : rel.funcs) {
            if (!has_function(name)) {
                throw ModelError{"mutex references unknown function: " + name};
            }
            if (!seen.insert(name).second) {
                throw ModelError{"mutex lists a function more than once: " +
                                 name};
            }
        }
    }
    for (const auto& rel : spec_.parallels) {
        if (!has_function(rel.a) || !has_function(rel.b)) {
            throw ModelError{"parallel references unknown function"};
        }
        if (rel.a == rel.b) {
            throw ModelError{"parallel cannot reference the same function"};
        }
    }
    for (const auto& rel : spec_.constraints) {
        std::vector<CountConstraint> ccs;
        std::string err;
        if (!parse_constraint_expr(rel.expr, ccs, err)) {
            throw ModelError{"invalid constraint: " + err};
        }
        for (const auto& cc : ccs) {
            if (!has_function(cc.func)) {
                throw ModelError{"constraint references unknown function: " +
                                 cc.func};
            }
        }
    }

    for (const auto& state : spec_.state_constraints) {
        bool found = false;
        for (const auto& object : spec_.objects) {
            for (const auto& s : object.states) {
                if (s.name == state) {
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        if (!found) {
            throw ModelError{"state constraint references unknown state: " +
                             state};
        }
    }

    for (const auto& rel : spec_.value_constraints) {
        if (!has_function(rel.func)) {
            throw ModelError{"value constraint references unknown function: " +
                             rel.func};
        }
        const FunctionDecl& fn = function(rel.func);
        bool param_found = false;
        for (const auto& p : fn.params) {
            if (p.name == rel.param) {
                param_found = true;
                break;
            }
        }
        if (!param_found) {
            throw ModelError{"value constraint references unknown parameter: " +
                             rel.func + "." + rel.param};
        }
    }
}

const FunctionDecl& Model::function(const std::string& name) const {
    const auto it = function_index_.find(name);
    if (it == function_index_.end()) {
        throw ModelError{"function not found: " + name};
    }
    return spec_.functions[it->second];
}

bool Model::has_function(const std::string& name) const {
    return function_index_.count(name) != 0;
}

const ObjectDecl& Model::object(const std::string& name) const {
    const auto it = object_index_.find(name);
    if (it == object_index_.end()) {
        throw ModelError{"object not found: " + name};
    }
    return spec_.objects[it->second];
}

Generator::Generator(const Model& model, const GenerationOptions& options)
    : model_(model), options_(options), remaining_flows_(options.max_flows) {}

bool Generator::truncated() const {
    if (strategy_) {
        return strategy_->truncated();
    }
    return options_.truncated;
}

std::vector<Flow> Generator::generate_state_flows() const {
    if (strategy_) {
        return strategy_->generate_state_flows();
    }
    if (remaining_flows_ == 0) {
        options_.truncated = true;
        return {};
    }
    std::vector<Flow> out;
    for (const auto& object : model_.spec().objects) {
        std::size_t initial = 0;
        bool found = false;
        for (std::size_t i = 0; i < object.states.size(); ++i) {
            if (object.states[i].initial) {
                initial = i;
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }
        std::vector<std::string> path;
        std::unordered_map<std::string, std::size_t> visits;
        state_dfs(object, initial, path, visits, out);
    }
    remaining_flows_ -= out.size();
    return out;
}

void Generator::state_dfs(const ObjectDecl& object, std::size_t state_index,
                          std::vector<std::string>& path,
                          std::unordered_map<std::string, std::size_t>& visits,
                          std::vector<Flow>& out) const {
    if (out.size() >= remaining_flows_) {
        options_.truncated = true;
        return;
    }
    const StateDecl& state = object.states[state_index];
    std::size_t& visit = visits[state.name];
    if (visit >= options_.max_state_visits) {
        return;
    }
    ++visit;
    if (state.final && !path.empty()) {
        if (parameter_respected(path) && order_respected(path) &&
            parallel_respected(path) && !mutex_violated(path) &&
            !constraint_violated(path) &&
            state_allowed(object, state.name)) {
            out.push_back(path);
        }
    }
    if (path.size() >= options_.max_depth) {
        --visit;
        return;
    }
    for (const auto& tr : object.transitions) {
        if (tr.src != state.name) {
            continue;
        }
        if (!guard_allows(tr)) {
            continue;
        }
        std::size_t next = object.states.size();
        for (std::size_t i = 0; i < object.states.size(); ++i) {
            if (object.states[i].name == tr.dst) {
                next = i;
                break;
            }
        }
        if (next == object.states.size()) {
            continue;
        }
        path.push_back(tr.func);
        state_dfs(object, next, path, visits, out);
        path.pop_back();
        if (out.size() >= remaining_flows_) {
            options_.truncated = true;
        }
    }
    --visit;
}

std::vector<Flow> Generator::generate_function_flows() const {
    if (strategy_) {
        return strategy_->generate_function_flows();
    }
    if (remaining_flows_ == 0) {
        options_.truncated = true;
        return {};
    }
    std::vector<Flow> out;
    std::vector<std::string> current;
    std::unordered_map<std::string, std::size_t> counts;
    function_dfs(current, counts, out);
    remaining_flows_ -= out.size();
    return out;
}

void Generator::function_dfs(
    std::vector<std::string>& current,
    std::unordered_map<std::string, std::size_t>& counts,
    std::vector<Flow>& out) const {
    if (out.size() >= remaining_flows_) {
        options_.truncated = true;
        return;
    }
    if (valid_function_flow(current)) {
        out.push_back(current);
        if (out.size() >= remaining_flows_) {
            options_.truncated = true;
            return;
        }
    }
    if (current.size() >= options_.max_depth ||
        function_flow_irreparable(current)) {
        return;
    }
    for (const auto& fn : model_.spec().functions) {
        if (counts[fn.name] >= options_.max_function_repeats) {
            continue;
        }
        current.push_back(fn.name);
        ++counts[fn.name];
        function_dfs(current, counts, out);
        --counts[fn.name];
        current.pop_back();
        if (out.size() >= remaining_flows_) {
            options_.truncated = true;
            return;
        }
    }
}

bool Generator::valid_function_flow(const Flow& flow) const {
    return parameter_respected(flow) && order_respected(flow) &&
           parallel_respected(flow) && !mutex_violated(flow) &&
           !constraint_violated(flow);
}

bool Generator::function_flow_irreparable(const Flow& flow) const {
    if (!parameter_respected(flow) || !order_respected(flow) ||
        mutex_violated(flow)) {
        return true;
    }
    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& name : flow) {
        ++counts[name];
    }
    for (const auto& rel : model_.spec().constraints) {
        std::vector<CountConstraint> ccs;
        std::string err;
        if (!parse_constraint_expr(rel.expr, ccs, err)) {
            continue;
        }
        for (const auto& cc : ccs) {
            const long actual = static_cast<long>(counts[cc.func]);
            if (cc.op == "<" && actual >= cc.value) {
                return true;
            }
            if (cc.op == "<=" && actual > cc.value) {
                return true;
            }
            if (cc.op == "==" && actual > cc.value) {
                return true;
            }
        }
    }
    return false;
}

bool Generator::order_respected(const Flow& flow) const {
    std::unordered_map<std::string, std::size_t> pos;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        pos[flow[i]] = i;
    }
    for (const auto& rel : model_.spec().orders) {
        if (!rel.condition.empty() && pos.count(rel.condition) == 0) {
            continue;
        }
        const auto before_it = pos.find(rel.before);
        const auto after_it = pos.find(rel.after);
        if (before_it == pos.end() || after_it == pos.end()) {
            continue;
        }
        if (before_it->second >= after_it->second) {
            return false;
        }
    }
    return true;
}

bool Generator::parameter_respected(const Flow& flow) const {
    std::unordered_map<std::string, std::size_t> pos;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        pos[flow[i]] = i;
    }
    for (const auto& rel : model_.spec().parameters) {
        if (rel.rhs_is_const) {
            continue;
        }
        const auto consumer = pos.find(rel.lhs_func);
        if (consumer == pos.end()) {
            continue;
        }
        const auto producer = pos.find(rel.rhs_func);
        if (producer == pos.end()) {
            return false;
        }
        if (producer->second >= consumer->second) {
            return false;
        }
    }
    return true;
}

bool Generator::guard_allows(const TransitionDecl& transition) const {
    if (transition.guard.empty()) {
        return true;
    }
    if (!transition.expect_present) {
        return true;
    }
    GuardExpr guard;
    std::string err;
    if (!parse_guard(transition.guard, guard, err)) {
        return true;
    }
    return guard_satisfied(guard, transition.expect_return);
}

bool Generator::state_allowed(const ObjectDecl& object,
                              const std::string& state) const {
    (void)object;
    if (model_.spec().state_constraints.empty()) {
        return true;
    }
    for (const auto& allowed : model_.spec().state_constraints) {
        if (allowed == state) {
            return true;
        }
    }
    return false;
}

bool Generator::parallel_respected(const Flow& flow) const {
    std::unordered_set<std::string> present(flow.begin(), flow.end());
    for (const auto& rel : model_.spec().parallels) {
        if (present.count(rel.a) != present.count(rel.b)) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> Generator::resolve_param_const(
    const std::string& func, const std::string& param,
    std::unordered_set<std::string>& visiting) const {
    const std::string key = func + "." + param;
    if (!visiting.insert(key).second) {
        return std::nullopt;
    }
    for (const auto& rel : model_.spec().parameters) {
        if (rel.lhs_func != func || rel.lhs_param != param) {
            continue;
        }
        if (rel.rhs_is_const) {
            return rel.rhs_const;
        }
        const FunctionDecl& rhs_fn = model_.function(rel.rhs_func);
        bool rhs_is_param = false;
        for (const auto& p : rhs_fn.params) {
            if (p.name == rel.rhs_param) {
                rhs_is_param = true;
                break;
            }
        }
        if (!rhs_is_param) {
            return std::nullopt;
        }
        return resolve_param_const(rel.rhs_func, rel.rhs_param, visiting);
    }
    return std::nullopt;
}

bool Generator::mutex_violated(const Flow& flow) const {
    std::unordered_set<std::string> present(flow.begin(), flow.end());
    for (const auto& rel : model_.spec().mutexes) {
        std::size_t present_count = 0;
        for (const auto& name : rel.funcs) {
            if (present.count(name)) {
                ++present_count;
            }
        }
        if (present_count > 1) {
            return true;
        }
    }
    return false;
}

bool Generator::constraint_violated(const Flow& flow) const {
    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& name : flow) {
        ++counts[name];
    }
    for (const auto& rel : model_.spec().constraints) {
        std::vector<CountConstraint> ccs;
        std::string err;
        if (!parse_constraint_expr(rel.expr, ccs, err)) {
            continue;
        }
        for (const auto& cc : ccs) {
            const long actual = static_cast<long>(counts[cc.func]);
            bool ok = false;
            if (cc.op == "<") {
                ok = actual < cc.value;
            } else if (cc.op == "<=") {
                ok = actual <= cc.value;
            } else if (cc.op == ">") {
                ok = actual > cc.value;
            } else if (cc.op == ">=") {
                ok = actual >= cc.value;
            } else if (cc.op == "==") {
                ok = actual == cc.value;
            } else if (cc.op == "!=") {
                ok = actual != cc.value;
            }
            if (!ok) {
                return true;
            }
        }
    }
    for (const auto& rel : model_.spec().value_constraints) {
        std::unordered_set<std::string> visiting;
        const auto resolved = resolve_param_const(rel.func, rel.param, visiting);
        if (!resolved.has_value()) {
            continue;
        }
        const bool matches = resolved.value() == rel.value;
        if ((rel.op == "==" && !matches) || (rel.op == "!=" && matches)) {
            return true;
        }
    }
    return false;
}

Runner::Runner(const RunnerOptions& options) : options_(options) {}

std::vector<FlowResult> Runner::run(const std::vector<Flow>& flows) const {
    std::vector<FlowResult> results;
    results.reserve(flows.size());
    for (std::size_t i = 0; i < flows.size(); ++i) {
        const Flow& flow = flows[i];
        if (options_.dry_run) {
            results.push_back(run_not_executed(flow, i));
        } else if (!options_.adapter_path.empty()) {
            results.push_back(run_adapter(flow, i));
        } else {
            if (flow_has_parameterized_call(flow)) {
                FlowResult r;
                r.flow = flow;
                r.id = flow_id_indexed(flow, i);
                r.status = "failed";
                r.detail =
                    "direct mode does not support parameterized calls or expect_output; "
                    "use --adapter or --mode standalone";
                r.bindings = flow_bindings(flow);
                results.push_back(r);
                continue;
            }
            results.push_back(run_direct(flow, i));
        }
    }
    return results;
}

bool Runner::flow_has_parameterized_call(const Flow& flow) const {
    for (const auto& name : flow) {
        if (options_.expected_outputs.count(name) != 0) {
            return true;
        }
    }
    if (!options_.spec) {
        return false;
    }
    for (const auto& name : flow) {
        const auto it = std::find_if(
            options_.spec->functions.begin(),
            options_.spec->functions.end(),
            [&name](const FunctionDecl& fn) { return fn.name == name; });
        if (it != options_.spec->functions.end() && !it->params.empty()) {
            return true;
        }
    }
    return false;
}

FlowResult Runner::run_not_executed(const Flow& flow,
                                    std::size_t index) const {
    FlowResult r;
    r.flow = flow;
    r.id = flow_id_indexed(flow, index);
    r.status = "not_executed";
    r.detail = "dry-run: no library loaded";
    r.bindings = flow_bindings(flow);
    return r;
}

std::string Runner::flow_bindings(const Flow& flow) const {
    if (!options_.spec) {
        return {};
    }
    std::unordered_set<std::string> present(flow.begin(), flow.end());
    std::ostringstream out;
    for (const auto& rel : options_.spec->parameters) {
        if (present.count(rel.lhs_func) == 0) {
            continue;
        }
        if (!rel.rhs_is_const && present.count(rel.rhs_func) == 0) {
            continue;
        }
        out << rel.lhs_func << "." << rel.lhs_param << " = ";
        if (rel.rhs_is_const) {
            out << "\"" << rel.rhs_const << "\"";
        } else {
            out << rel.rhs_func << "." << rel.rhs_param;
        }
        out << ";";
    }
    return out.str();
}

FlowResult Runner::run_direct(const Flow& flow, std::size_t index) const {
    FlowResult result;
    result.flow = flow;
    result.id = flow_id_indexed(flow, index);
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.status = "failed";
        result.detail = "pipe failed";
        return result;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.status = "failed";
        result.detail = "fork failed";
        return result;
    }
    if (pid == 0) {
        close(pipefd[0]);
        alarm(options_.timeout_seconds > 0 ? options_.timeout_seconds : 1);
        void* handle = dlopen(options_.lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            const char* raw = dlerror();
            const std::string err = raw ? raw : "unknown dlopen error";
            const char* msg = err.c_str();
            const ssize_t ignored1 = write(pipefd[1], msg, err.size());
            (void)ignored1;
            close(pipefd[1]);
            _exit(127);
        }
        for (const auto& name : flow) {
            dlerror();
            using Fn = int (*)();
            const Fn fn = reinterpret_cast<Fn>(dlsym(handle, name.c_str()));
            const char* err = dlerror();
            if (err) {
                const std::string msg = std::string("symbol not found: ") + name + ": " + err;
                const ssize_t ignored2 = write(pipefd[1], msg.c_str(), msg.size());
                (void)ignored2;
                dlclose(handle);
                close(pipefd[1]);
                _exit(126);
            }
            const int ret = fn();
            const auto guard = options_.guards.find(name);
            if (guard != options_.guards.end() &&
                !guard_satisfied(guard->second, ret)) {
                const std::string msg =
                    "guard not satisfied for " + name +
                    " (return=" + std::to_string(ret) + ")";
                const ssize_t ignored_guard =
                    write(pipefd[1], msg.c_str(), msg.size());
                (void)ignored_guard;
                dlclose(handle);
                close(pipefd[1]);
                _exit(125);
            }
            const auto expected = options_.expected_returns.find(name);
            bool mismatch = false;
            if (expected != options_.expected_returns.end() &&
                expected->second.has_value()) {
                mismatch = ret != expected->second.value();
            } else {
                mismatch = ret != 0;
            }
            if (mismatch) {
                const std::string msg =
                    expected != options_.expected_returns.end() &&
                            expected->second.has_value()
                        ? "return mismatch: expected " +
                              std::to_string(expected->second.value()) +
                              ", got " + std::to_string(ret) + " for " + name
                        : "function returned non-zero: " + name + " (" +
                              std::to_string(ret) + ")";
                const ssize_t ignored4 = write(pipefd[1], msg.c_str(), msg.size());
                (void)ignored4;
                dlclose(handle);
                close(pipefd[1]);
                _exit(1);
            }
        }
        dlclose(handle);
        close(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);
    std::string detail;
    char buf[512];
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        detail.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        if (result.exit_code == 0) {
            result.status = "passed";
        } else {
            result.status = "failed";
        }
    } else if (WIFSIGNALED(status)) {
        result.exit_code = WTERMSIG(status);
        if (WTERMSIG(status) == SIGALRM) {
            result.status = "timeout";
            result.detail = "timed out";
        } else {
            result.status = "crashed";
            result.detail = "signal " + std::to_string(WTERMSIG(status));
        }
    }
    if (!detail.empty()) {
        result.detail = detail;
    }
    result.bindings = flow_bindings(flow);
    return result;
}

FlowResult Runner::run_adapter(const Flow& flow, std::size_t index) const {
    FlowResult result;
    result.flow = flow;
    result.id = flow_id_indexed(flow, index);
    if (!options_.spec) {
        result.status = "failed";
        result.detail = "adapter mode requires an internal model";
        return result;
    }

    std::map<std::string, std::map<std::string, std::string>> returned;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        const std::string& function = flow[i];
        std::map<std::string, std::string> args;

        for (const auto& rel : options_.spec->parameters) {
            if (rel.lhs_func != function) {
                continue;
            }
            if (rel.rhs_is_const) {
                args[rel.lhs_param] = rel.rhs_const;
                continue;
            }
            const auto producer = returned.find(rel.rhs_func);
            if (producer == returned.end()) {
                result.status = "failed";
                result.detail = "producer return unavailable for " + function +
                                "." + rel.lhs_param;
                result.bindings = flow_bindings(flow);
                return result;
            }
            const auto value = producer->second.find(rel.rhs_param);
            if (value == producer->second.end()) {
                result.status = "failed";
                result.detail = "producer return missing: " + rel.rhs_func +
                                "." + rel.rhs_param;
                result.bindings = flow_bindings(flow);
                return result;
            }
            args[rel.lhs_param] = value->second;
        }

        int in_pipe[2];
        int out_pipe[2];
        int err_pipe[2];
        if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
            result.status = "failed";
            result.detail = "pipe failed";
            result.bindings = flow_bindings(flow);
            return result;
        }

        const pid_t pid = fork();
        if (pid < 0) {
            close(in_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);
            result.status = "failed";
            result.detail = "fork failed";
            result.bindings = flow_bindings(flow);
            return result;
        }

        if (pid == 0) {
            setpgid(0, 0);
            dup2(in_pipe[0], STDIN_FILENO);
            dup2(out_pipe[1], STDOUT_FILENO);
            dup2(err_pipe[1], STDERR_FILENO);
            close(in_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);

            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(options_.adapter_path.c_str()));
            for (const auto& arg : options_.adapter_args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);
            const char* env[] = {"PATH=/usr/bin:/bin", "LC_ALL=C", nullptr};
            execve(options_.adapter_path.c_str(), argv.data(),
                   const_cast<char* const*>(env));
            _exit(127);
        }

        setpgid(pid, pid);
        close(in_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[1]);

        std::map<std::string, std::string> arg_types;
        std::string return_type = "int";
        if (options_.spec) {
            const auto fn_it = std::find_if(
                options_.spec->functions.begin(),
                options_.spec->functions.end(),
                [&function](const FunctionDecl& fn) {
                    return fn.name == function;
                });
            if (fn_it != options_.spec->functions.end()) {
                for (const auto& param : fn_it->params) {
                    arg_types[param.name] =
                        param.type.empty() ? "string" : param.type;
                }
                return_type = fn_it->return_type.empty()
                                  ? (fn_it->return_param.empty()
                                         ? "int"
                                         : fn_it->return_param)
                                  : fn_it->return_type;
            }
        }
        const std::string request =
            make_adapter_request(result.id, i, function, args, arg_types,
                                 return_type);
        const ssize_t written =
            write(in_pipe[1], request.data(), request.size());
        (void)written;
        close(in_pipe[1]);

        fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

        const auto deadline =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(options_.timeout_seconds > 0
                                     ? options_.timeout_seconds
                                     : 10);
        std::string output;
        std::string errors;
        bool out_open = true;
        bool err_open = true;
        bool timed_out = false;

        while (out_open || err_open) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                if (kill(-pid, SIGTERM) != 0) {
                    kill(pid, SIGTERM);
                }
                timed_out = true;
                break;
            }
            pollfd fds[2]{{out_pipe[0], POLLIN, 0}, {err_pipe[0], POLLIN, 0}};
            const int wait_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                      now)
                    .count());
            const int poll_rc = poll(fds, 2, std::max(1, wait_ms));
            if (poll_rc < 0 && errno != EINTR) {
                break;
            }
            for (int j = 0; j < 2; ++j) {
                if (!(fds[j].revents & (POLLIN | POLLHUP))) {
                    continue;
                }
                const int fd = j == 0 ? out_pipe[0] : err_pipe[0];
                char buffer[4096];
                const ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0) {
                    (j == 0 ? output : errors)
                        .append(buffer, static_cast<std::size_t>(n));
                } else if (n == 0) {
                    if (j == 0) {
                        out_open = false;
                    } else {
                        err_open = false;
                    }
                }
            }
        }
        close(out_pipe[0]);
        close(err_pipe[0]);

        int status = 0;
        if (timed_out) {
            usleep(100000);
            if (kill(-pid, SIGKILL) != 0) {
                kill(pid, SIGKILL);
            }
        }
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        if (timed_out) {
            result.status = "timeout";
            result.detail = "adapter step timed out";
            result.bindings = flow_bindings(flow);
            return result;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            result.status = "failed";
            result.detail = "adapter exited with error";
            result.bindings = flow_bindings(flow);
            return result;
        }

        std::string response_status;
        std::string stdout_text;
        std::map<std::string, std::string> returns;
        long return_value = 0;
        bool has_return = false;
        std::string protocol_err;
        if (!parse_adapter_response(output, response_status, return_value,
                                    has_return, returns, stdout_text,
                                    protocol_err)) {
            result.status = "failed";
            result.detail = "malformed adapter response: " + protocol_err;
            result.bindings = flow_bindings(flow);
            return result;
        }
        if (response_status != "ok") {
            result.status = "failed";
            result.detail = "adapter reported " + response_status;
            result.expected = "ok";
            result.actual = response_status;
            result.bindings = flow_bindings(flow);
            return result;
        }

        const auto guard = options_.guards.find(function);
        if (guard != options_.guards.end() &&
            !guard_satisfied(guard->second, static_cast<int>(return_value))) {
            result.status = "failed";
            result.detail = "guard not satisfied for " + function +
                            " (return=" + std::to_string(return_value) + ")";
            result.expected = "guard " + guard->second.op + " " +
                              std::to_string(guard->second.value);
            result.actual = std::to_string(return_value);
            result.bindings = flow_bindings(flow);
            return result;
        }

        const auto expected = options_.expected_returns.find(function);
        if (expected != options_.expected_returns.end() &&
            expected->second.has_value()) {
            if (return_value != expected->second.value()) {
                result.status = "failed";
                result.detail = "return mismatch for " + function +
                                ": expected " +
                                std::to_string(expected->second.value()) +
                                ", got " + std::to_string(return_value);
                result.expected = std::to_string(expected->second.value());
                result.actual = std::to_string(return_value);
                result.bindings = flow_bindings(flow);
                return result;
            }
        } else if (has_return && return_value != 0) {
            result.status = "failed";
            result.detail = "function returned non-zero: " + function + " (" +
                            std::to_string(return_value) + ")";
            result.expected = "0";
            result.actual = std::to_string(return_value);
            result.bindings = flow_bindings(flow);
            return result;
        }

        const auto expected_output = options_.expected_outputs.find(function);
        if (expected_output != options_.expected_outputs.end() &&
            stdout_text != expected_output->second) {
            result.status = "failed";
            result.detail = "output mismatch for " + function + ": expected \"" +
                            expected_output->second + "\", got \"" +
                            stdout_text + "\"";
            result.expected = expected_output->second;
            result.actual = stdout_text;
            result.bindings = flow_bindings(flow);
            return result;
        }

        returned[function] = std::move(returns);
    }

    result.status = "passed";
    result.bindings = flow_bindings(flow);
    return result;
}

std::string Runner::generate_standalone(const std::vector<Flow>& flows) const {
    for (const auto& flow : flows) {
        if (flow_has_parameterized_call(flow)) {
            throw std::runtime_error(
                "standalone mode does not support parameterized calls or expect_output; "
                "use --adapter");
        }
    }
    std::ostringstream out;
    out << "#include <cstdio>\n";
    out << "#include <cstdlib>\n\n";
    std::set<std::string> functions;
    for (const auto& flow : flows) {
        functions.insert(flow.begin(), flow.end());
    }
    for (const auto& name : functions) {
        out << "extern \"C\" int " << name << "();\n";
    }
    out << "\n";
    for (std::size_t i = 0; i < flows.size(); ++i) {
        out << "static int run_flow_" << i << "() {\n";
        if (options_.spec) {
            std::unordered_set<std::string> present(flows[i].begin(),
                                                    flows[i].end());
            for (const auto& rel : options_.spec->parameters) {
                if (present.count(rel.lhs_func) == 0) {
                    continue;
                }
                if (rel.rhs_is_const) {
                    out << "  // " << rel.lhs_func << "." << rel.lhs_param
                        << " = \"" << rel.rhs_const << "\"\n";
                } else if (present.count(rel.rhs_func) != 0) {
                    out << "  // " << rel.lhs_func << "." << rel.lhs_param
                        << " <- " << rel.rhs_func << "." << rel.rhs_param << "\n";
                }
            }
        }
        for (const auto& name : flows[i]) {
            out << "  {\n";
            out << "    int r = " << name << "();\n";
            const auto guard = options_.guards.find(name);
            if (guard != options_.guards.end()) {
                out << "    if (!(r " << guard->second.op << " "
                    << guard->second.value << ")) return 1;\n";
            }
            const auto expected = options_.expected_returns.find(name);
            if (expected != options_.expected_returns.end() &&
                expected->second.has_value()) {
                out << "    if (r != " << expected->second.value()
                    << ") return 1;\n";
            } else {
                out << "    if (r != 0) return 1;\n";
            }
            out << "  }\n";
        }
        out << "  return 0;\n";
        out << "}\n\n";
    }
    out << "int main() {\n";
    out << "  int failures = 0;\n";
    for (std::size_t i = 0; i < flows.size(); ++i) {
        out << "  int r" << i << " = run_flow_" << i << "();\n";
        out << "  std::printf(\"flow " << i << " %s\\n\", r" << i << " == 0 ? \"passed\" : \"failed\");\n";
        out << "  failures += r" << i << ";\n";
    }
    out << "  return failures == 0 ? 0 : 1;\n";
    out << "}\n";
    return out.str();
}

Logger::Logger(const LogOptions& options) : options_(options) {}

void Logger::rotate_if_needed() {
    struct stat st{};
    if (stat(options_.file.c_str(), &st) != 0) {
        return;
    }
    if (static_cast<std::size_t>(st.st_size) < options_.max_size) {
        return;
    }
    for (std::size_t i = options_.rotate_count; i > 0; --i) {
        const std::string src = options_.file + (i == 1 ? "" : "." + std::to_string(i - 1));
        const std::string dst = options_.file + "." + std::to_string(i);
        if (i == 1) {
            rename(src.c_str(), dst.c_str());
        } else {
            rename(src.c_str(), dst.c_str());
        }
    }
}

void Logger::log(const std::string& level, const std::string& module,
                 const std::string& flow, const std::string& message) {
    rotate_if_needed();
    std::ofstream out(options_.file, std::ios::app);
    if (!out) {
        throw std::runtime_error("cannot write log file: " + options_.file);
    }
    out << "[" << now_string() << "][" << level << "][" << module
        << "][" << flow << "] " << message << "\n";
    if (!out) {
        throw std::runtime_error("failed while writing log file: " +
                                 options_.file);
    }
}

std::string flow_id(const Flow& flow) {
    std::ostringstream out;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        if (i) {
            out << "->";
        }
        out << flow[i];
    }
    return out.str();
}

std::string flow_id_indexed(const Flow& flow, std::size_t index) {
    return std::to_string(index) + ":" + flow_id(flow);
}

std::string spec_digest(const Spec& spec) {
    std::ostringstream serialized;
    for (const auto& fn : spec.functions) {
        serialized << "fn:" << fn.name << "(";
        for (const auto& p : fn.params) {
            serialized << p.type << ":" << p.name << ",";
        }
        serialized << ")->" << fn.return_param << ":" << fn.return_type << ";";
    }
    for (const auto& object : spec.objects) {
        serialized << "obj:" << object.name << "{";
        for (const auto& state : object.states) {
            serialized << "s:" << state.name << ":" << state.initial << ":"
                       << state.final << ";";
        }
        for (const auto& tr : object.transitions) {
            serialized << "t:" << tr.src << "->" << tr.dst << ":" << tr.func
                       << ":" << tr.guard << ":" << tr.expect_present << ":"
                       << tr.expect_return << ":" << tr.expect_output_present
                       << ":" << tr.expect_output << ";";
        }
        serialized << "}";
    }
    for (const auto& rel : spec.parameters) {
        serialized << "p:" << rel.lhs_func << "." << rel.lhs_param << "=";
        if (rel.rhs_is_const) {
            serialized << rel.rhs_const;
        } else {
            serialized << rel.rhs_func << "." << rel.rhs_param;
        }
        serialized << ";";
    }
    for (const auto& rel : spec.orders) {
        serialized << "o:" << rel.before << "<" << rel.after << ":"
                   << rel.condition << ";";
    }
    for (const auto& rel : spec.mutexes) {
        serialized << "m:";
        for (const auto& fn : rel.funcs) {
            serialized << fn << ",";
        }
        serialized << ";";
    }
    for (const auto& rel : spec.parallels) {
        serialized << "pa:" << rel.a << "," << rel.b << ";";
    }
    for (const auto& rel : spec.constraints) {
        serialized << "c:" << rel.expr << ";";
    }
    for (const auto& rel : spec.state_constraints) {
        serialized << "sc:" << rel << ";";
    }
    for (const auto& rel : spec.value_constraints) {
        serialized << "vc:" << rel.func << "." << rel.param << rel.op
                   << rel.value << ";";
    }

    std::uint64_t hash = 1469598103934665603ULL;
    const std::string data = serialized.str();
    for (const unsigned char c : data) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::vector<Flow> parse_trace_flows(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open trace file: " + path);
    }
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    JsonValue root;
    JsonParser parser(text);
    std::string err;
    if (!parser.parse(root, err)) {
        throw std::runtime_error("invalid trace file: " + err);
    }
    if (root.type != JsonValue::Type::Object) {
        throw std::runtime_error("trace file root must be an object");
    }
    const auto flows_it = root.object.find("flows");
    if (flows_it == root.object.end() ||
        flows_it->second.type != JsonValue::Type::Array) {
        throw std::runtime_error("trace file missing flows array");
    }
    std::vector<Flow> flows;
    for (const auto& entry : flows_it->second.array) {
        if (entry.type != JsonValue::Type::Object) {
            throw std::runtime_error("trace flow entry must be an object");
        }
        const auto flow_it = entry.object.find("flow");
        if (flow_it == entry.object.end() ||
            flow_it->second.type != JsonValue::Type::Array) {
            throw std::runtime_error("trace flow entry missing flow array");
        }
        Flow flow;
        for (const auto& fn : flow_it->second.array) {
            if (fn.type != JsonValue::Type::String) {
                throw std::runtime_error("trace flow function must be a string");
            }
            flow.push_back(fn.string);
        }
        flows.push_back(std::move(flow));
    }
    return flows;
}

std::string render_report(const std::vector<FlowResult>& results,
                          const std::string& format,
                          const ReportMeta& meta) {
    const auto escape_json = [](const std::string& value) {
        std::ostringstream out;
        for (const unsigned char c : value) {
            switch (c) {
                case '"':
                    out << "\\\"";
                    break;
                case '\\':
                    out << "\\\\";
                    break;
                case '\n':
                    out << "\\n";
                    break;
                case '\r':
                    out << "\\r";
                    break;
                case '\t':
                    out << "\\t";
                    break;
                default:
                    if (c < 0x20U) {
                        out << "\\u00" << std::hex << std::setw(2)
                            << std::setfill('0') << static_cast<int>(c) << std::dec;
                    } else {
                        out << static_cast<char>(c);
                    }
            }
        }
        return out.str();
    };
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const auto& r : results) {
        if (r.status == "passed") {
            ++passed;
        } else if (r.status != "not_executed") {
            ++failed;
        }
    }
    if (format == "json") {
        std::ostringstream out;
        out << "{\n";
        out << "  \"seed\": " << meta.seed << ",\n";
        out << "  \"seed_set\": " << (meta.seed_set ? "true" : "false") << ",\n";
        out << "  \"version\": \"" << escape_json(meta.version) << "\",\n";
        out << "  \"model_digest\": \""
            << escape_json(meta.model_digest) << "\",\n";
        out << "  \"files\": [";
        for (std::size_t i = 0; i < meta.files.size(); ++i) {
            if (i) {
                out << ", ";
            }
            out << '"' << escape_json(meta.files[i]) << '"';
        }
        out << "],\n";
        out << "  \"total\": " << results.size() << ",\n";
        out << "  \"passed\": " << passed << ",\n";
        out << "  \"failed\": " << failed << ",\n";
        out << "  \"flows\": [\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            const std::string id =
                r.id.empty() ? flow_id(r.flow) : r.id;
            out << "    {\"id\": \"" << escape_json(id)
                << "\", \"flow\": [";
            for (std::size_t j = 0; j < r.flow.size(); ++j) {
                if (j) {
                    out << ", ";
                }
                out << '"' << escape_json(r.flow[j]) << '"';
            }
            out << "]"
                << ", \"status\": \"" << escape_json(r.status)
                << "\", \"exit_code\": " << r.exit_code
                << ", \"detail\": \"" << escape_json(r.detail)
                << "\", \"expected\": \"" << escape_json(r.expected)
                << "\", \"actual\": \"" << escape_json(r.actual)
                << "\", \"bindings\": \"" << escape_json(r.bindings) << "\"}";
            if (i + 1 < results.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        return out.str();
    }
    std::ostringstream out;
    out << "# version=" << meta.version << " seed=" << meta.seed
        << " model_digest=" << meta.model_digest;
    if (!meta.files.empty()) {
        out << " files=";
        for (std::size_t i = 0; i < meta.files.size(); ++i) {
            if (i) {
                out << ",";
            }
            out << meta.files[i];
        }
    }
    out << "\n";
    out << "total=" << results.size() << " passed=" << passed
        << " failed=" << failed << "\n";
    for (const auto& r : results) {
        out << "[" << r.status << "] "
            << (r.id.empty() ? flow_id(r.flow) : r.id);
        if (!r.detail.empty()) {
            out << " (" << r.detail << ")";
        }
        if (!r.bindings.empty()) {
            out << " {" << r.bindings << "}";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace wct
