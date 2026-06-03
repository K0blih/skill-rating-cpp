#include "skill_rating/skill_rating.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TestCase {
    std::string_view name;
    std::function<void()> run;
};

bool near(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

void check(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void check_near(double actual, double expected, double tolerance, std::string_view label) {
    if (!near(actual, expected, tolerance)) {
        throw std::runtime_error(
            std::string(label) + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

template <typename Fn>
void check_throws(Fn&& fn, std::string_view label) {
    try {
        fn();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(std::string(label) + ": expected exception");
}

void defaults_and_exposure() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    check_near(rating.mu, 25.0, 1e-12, "default mu");
    check_near(rating.sigma, 25.0 / 3.0, 1e-12, "default sigma");
    check_near(env.expose(rating), 0.0, 1e-12, "default exposure");
}

void one_vs_one_quality() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    check_near(env.quality_1vs1(rating, rating), 0.447, 0.001, "1v1 quality");
    check_near(env.draw_probability_1vs1(rating, rating), 0.0448, 0.0001, "1v1 draw probability");
}

void one_vs_one_win() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const auto [winner, loser] = env.rate_1vs1(rating, rating);
    check_near(winner.mu, 29.396, 0.01, "winner mu");
    check_near(winner.sigma, 7.171, 0.01, "winner sigma");
    check_near(loser.mu, 20.604, 0.01, "loser mu");
    check_near(loser.sigma, 7.171, 0.01, "loser sigma");
}

void one_vs_one_draw() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const auto [draw_a, draw_b] = env.rate_1vs1(rating, rating, true);
    check_near(draw_a.mu, 25.0, 0.01, "draw player A mu");
    check_near(draw_b.mu, 25.0, 0.01, "draw player B mu");
    check_near(draw_a.sigma, 6.458, 0.02, "draw player A sigma");
    check_near(draw_b.sigma, 6.458, 0.02, "draw player B sigma");
}

void team_match() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const skill_rating::RatingGroups rated = env.rate({{rating}, {rating, rating}});
    check_near(rated[0][0].mu, 33.731, 0.03, "solo winner mu");
    check_near(rated[0][0].sigma, 7.317, 0.03, "solo winner sigma");
    check_near(rated[1][0].mu, 16.269, 0.03, "team loser A mu");
    check_near(rated[1][0].sigma, 7.317, 0.03, "team loser A sigma");
    check_near(rated[1][1].mu, 16.269, 0.03, "team loser B mu");
    check_near(rated[1][1].sigma, 7.317, 0.03, "team loser B sigma");
}

void ranks_and_draws() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const skill_rating::RatingGroups ranked = env.rate({{rating}, {rating}, {rating}}, {1, 0, 2});
    check(ranked[1][0].mu > ranked[0][0].mu, "best ranked player should gain most");
    check(ranked[0][0].mu > ranked[2][0].mu, "middle ranked player should beat worst ranked player");

    const skill_rating::RatingGroups drawn_groups = env.rate({{rating}, {rating}}, {0, 0});
    check_near(drawn_groups[0][0].mu, drawn_groups[1][0].mu, 1e-9, "drawn groups should stay symmetric");
}

void partial_play_weights() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const auto full_weight = env.rate({{rating}, {rating}}, {}, {{{1.0}}, {{1.0}}});
    const auto half_weight = env.rate({{rating}, {rating}}, {}, {{{0.5}}, {{1.0}}});
    check(full_weight[0][0].mu > rating.mu, "full-weight winner should gain rating");
    check(std::isfinite(half_weight[0][0].mu), "partial-weight update should remain finite");
    check(std::isfinite(env.quality({{rating}, {rating}}, {{{0.5}}, {{1.0}}})), "partial-weight quality should remain finite");
    check(std::isfinite(env.draw_probability({{rating}, {rating}}, {{{0.5}}, {{1.0}}})), "partial-weight draw probability should remain finite");
}

void zero_weight_players_are_unchanged() {
    const skill_rating::Environment env;
    const auto active_winner = env.create_rating();
    const auto benched_winner = env.create_rating(40.0, 4.0);
    const auto active_loser = env.create_rating();
    const auto benched_loser = env.create_rating(10.0, 5.0);

    const skill_rating::RatingGroups rated = env.rate(
        {{active_winner, benched_winner}, {active_loser, benched_loser}},
        {0, 1},
        {{1.0, 0.0}, {1.0, 0.0}});

    check(rated[0][0].mu > active_winner.mu, "active winner should be updated");
    check(rated[1][0].mu < active_loser.mu, "active loser should be updated");
    check_near(rated[0][1].mu, benched_winner.mu, 1e-12, "zero-weight winner mu unchanged");
    check_near(rated[0][1].sigma, benched_winner.sigma, 1e-12, "zero-weight winner sigma unchanged");
    check_near(rated[1][1].mu, benched_loser.mu, 1e-12, "zero-weight loser mu unchanged");
    check_near(rated[1][1].sigma, benched_loser.sigma, 1e-12, "zero-weight loser sigma unchanged");
}

void custom_environment_and_draw_helpers() {
    const skill_rating::Environment env(30.0, 10.0, 5.0, 0.2, 0.2);
    const auto rating = env.create_rating();
    check_near(rating.mu, 30.0, 1e-12, "custom default mu");
    check_near(rating.sigma, 10.0, 1e-12, "custom default sigma");

    const double margin = skill_rating::calc_draw_margin(0.2, 2, env.beta());
    const double probability = skill_rating::calc_draw_probability(margin, 2, env.beta());
    check_near(probability, 0.2, 1e-10, "draw probability round trip");
}

void multi_team_quality_and_draws() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    const skill_rating::RatingGroups groups = {{rating}, {rating}, {rating, rating}};
    const double quality = env.quality(groups);
    check(quality >= 0.0 && quality <= 1.0, "quality should be in [0, 1]");

    const skill_rating::RatingGroups rated = env.rate(groups, {0, 0, 2});
    check(rated[0][0].mu > rating.mu, "drawn winner team should gain rating");
    check(rated[1][0].mu > rating.mu, "other drawn winner team should gain rating");
    check(rated[2][0].mu < rating.mu, "last team should lose rating");
    check(rated[2][1].mu < rating.mu, "last team second player should lose rating");
}

void validation_errors() {
    const skill_rating::Environment env;
    const auto rating = env.create_rating();
    check_throws([&] { static_cast<void>(env.rate({{rating}})); }, "single team");
    check_throws([&] { static_cast<void>(env.rate({{}, {rating}})); }, "empty team");
    check_throws([&] { static_cast<void>(env.rate({{rating}, {rating}}, {0})); }, "rank count mismatch");
    check_throws([&] { static_cast<void>(env.rate({{rating}, {rating}}, {}, {{{1.0}}})); }, "weight count mismatch");
    check_throws([&] { static_cast<void>(env.rate({{rating}, {rating}}, {}, {{{0.0}}, {{1.0}}})); }, "team with no positive weight");
    check_throws([&] { static_cast<void>(env.quality({{rating}, {rating}}, {{{1.0}}, {{0.0}}})); }, "quality team with no positive weight");
    check_throws([&] { static_cast<void>(env.draw_probability({{rating}, {rating}, {rating}})); }, "free-for-all draw probability");
    check_throws([] { skill_rating::Environment(25.0, 0.0, 1.0, 1.0, 0.1); }, "invalid sigma");
    check_throws([] { skill_rating::Environment(25.0, 1.0, 1.0, 1.0, 1.0); }, "invalid draw probability");
}

} // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"defaults and exposure", defaults_and_exposure},
        {"1v1 quality", one_vs_one_quality},
        {"1v1 win", one_vs_one_win},
        {"1v1 draw", one_vs_one_draw},
        {"team match", team_match},
        {"ranks and draws", ranks_and_draws},
        {"partial play weights", partial_play_weights},
        {"zero-weight players are unchanged", zero_weight_players_are_unchanged},
        {"custom environment and draw helpers", custom_environment_and_draw_helpers},
        {"multi-team quality and draws", multi_team_quality_and_draws},
        {"validation errors", validation_errors},
    };

    int passed = 0;
    for (const TestCase& test : tests) {
        try {
            test.run();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
            std::cerr << passed << " of " << tests.size() << " tests passed\n";
            return 1;
        }
    }

    std::cout << passed << " of " << tests.size() << " tests passed\n";
    return 0;
}
