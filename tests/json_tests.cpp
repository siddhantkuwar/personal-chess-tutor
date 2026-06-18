#include "test.hpp"

#include "pct/common/json.hpp"

using namespace pct::json;

TEST_CASE("JSON parser reads structured request data") {
    const Value value =
        parse(R"json({"url":"https://chess.com","enabled":true,"items":[1,2,3]})json");
    CHECK_EQ(value.at("url").as_string(), "https://chess.com");
    CHECK(value.at("enabled").as_bool());
    CHECK_EQ(value.at("items").as_array().size(), 3ULL);
}

TEST_CASE("JSON dump round trips strings and arrays") {
    const Value original(Value::Object{
        {"message", "line one\n\"line two\""},
        {"values", Value::Array{1, 2, 3}},
    });
    const std::string encoded = dump(original);
    const Value decoded = parse(encoded);
    CHECK_EQ(decoded.at("message").as_string(), "line one\n\"line two\"");
    CHECK_EQ(decoded.at("values").as_array().size(), 3ULL);
}

TEST_CASE("JSON parser rejects malformed and duplicate data") {
    CHECK_THROWS(parse("{\"a\":1,}"));
    CHECK_THROWS(parse("{\"a\":1,\"a\":2}"));
    CHECK_THROWS(parse("[1 2]"));
}
