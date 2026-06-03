#include "app/commands.hpp"

#include "app/json.hpp"

#include <stdexcept>
#include <string>

namespace skill_rating::app {
namespace {

const Json* find(const Json& object, std::string_view key) {
    const auto iter = object.find(std::string(key));
    if (iter == object.end()) {
        return nullptr;
    }
    return &*iter;
}

const Json& require(const Json& object, std::string_view key) {
    const Json* value = find(object, key);
    if (value == nullptr) {
        throw std::invalid_argument("missing required key: " + std::string(key));
    }
    return *value;
}

double as_number(const Json& json, std::string_view label) {
    if (!json.is_number()) {
        throw std::invalid_argument(std::string(label) + " must be a number");
    }
    return json.get<double>();
}

bool as_bool(const Json& json, std::string_view label) {
    if (!json.is_boolean()) {
        throw std::invalid_argument(std::string(label) + " must be a boolean");
    }
    return json.get<bool>();
}

RatingGroups parse_draw_probability_groups(const Json& request) {
    const Json* rating_groups = find(request, "rating_groups");
    if (rating_groups != nullptr) {
        return parse_rating_groups(*rating_groups);
    }
    return {{parse_rating(require(request, "first_player"))}, {parse_rating(require(request, "second_player"))}};
}

} // namespace

std::string run_command(std::string_view command, std::string_view request_body) {
    const Json request_json = parse_json(request_body);
    if (!request_json.is_object()) {
        throw std::invalid_argument("request must be an object");
    }
    const Json& request = request_json;
    const Environment env = parse_environment(request);

    if (command == "rate") {
        const RatingGroups groups = parse_rating_groups(require(request, "rating_groups"));
        const std::vector<int> ranks = parse_ranks(find(request, "ranks"));
        const Weights weights = parse_weights(find(request, "weights"));
        const double min_delta = find(request, "min_delta") == nullptr ? 0.0001 : as_number(*find(request, "min_delta"), "min_delta");
        return Json{{"rating_groups", rating_groups_json(env.rate(groups, ranks, weights, min_delta))}}.dump();
    }
    if (command == "rate-1vs1") {
        const Rating first = parse_rating(require(request, "first_player"));
        const Rating second = parse_rating(require(request, "second_player"));
        const bool drawn = find(request, "drawn") == nullptr ? false : as_bool(*find(request, "drawn"), "drawn");
        const double min_delta = find(request, "min_delta") == nullptr ? 0.0001 : as_number(*find(request, "min_delta"), "min_delta");
        const auto [updated_first, updated_second] = env.rate_1vs1(first, second, drawn, min_delta);
        return Json{{"first_player", rating_json(updated_first)}, {"second_player", rating_json(updated_second)}}.dump();
    }
    if (command == "quality") {
        const RatingGroups groups = parse_rating_groups(require(request, "rating_groups"));
        const Weights weights = parse_weights(find(request, "weights"));
        return Json{{"quality", env.quality(groups, weights)}}.dump();
    }
    if (command == "quality-1vs1") {
        const Rating first = parse_rating(require(request, "first_player"));
        const Rating second = parse_rating(require(request, "second_player"));
        return Json{{"quality", env.quality_1vs1(first, second)}}.dump();
    }
    if (command == "expose") {
        return Json{{"exposure", env.expose(parse_rating(require(request, "rating")))}}.dump();
    }
    if (command == "draw-probability") {
        const RatingGroups groups = parse_draw_probability_groups(request);
        const Weights weights = parse_weights(find(request, "weights"));
        return Json{{"draw_probability", env.draw_probability(groups, weights)}}.dump();
    }

    throw std::invalid_argument("unknown command: " + std::string(command));
}

} // namespace skill_rating::app
