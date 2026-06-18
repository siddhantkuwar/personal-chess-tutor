#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pct::json {

class Value {
  public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool value) : value_(value) {}
    Value(int value) : value_(static_cast<double>(value)) {}
    Value(std::size_t value) : value_(static_cast<double>(value)) {}
    Value(double value) : value_(value) {}
    Value(const char* value) : value_(std::string(value)) {}
    Value(std::string value) : value_(std::move(value)) {}
    Value(Array value) : value_(std::move(value)) {}
    Value(Object value) : value_(std::move(value)) {}

    [[nodiscard]] bool is_null() const {
        return std::holds_alternative<std::nullptr_t>(value_);
    }
    [[nodiscard]] bool is_bool() const {
        return std::holds_alternative<bool>(value_);
    }
    [[nodiscard]] bool is_number() const {
        return std::holds_alternative<double>(value_);
    }
    [[nodiscard]] bool is_string() const {
        return std::holds_alternative<std::string>(value_);
    }
    [[nodiscard]] bool is_array() const {
        return std::holds_alternative<Array>(value_);
    }
    [[nodiscard]] bool is_object() const {
        return std::holds_alternative<Object>(value_);
    }

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] int as_int() const;
    [[nodiscard]] std::size_t as_size() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] Array& as_array();
    [[nodiscard]] Object& as_object();

    [[nodiscard]] const Value& at(std::string_view key) const;
    [[nodiscard]] const Value& get(std::string_view key, const Value& fallback) const;

  private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_{nullptr};
};

[[nodiscard]] Value parse(std::string_view input);
[[nodiscard]] std::string dump(const Value& value);

} // namespace pct::json
