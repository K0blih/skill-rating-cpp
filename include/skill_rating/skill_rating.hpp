#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace skill_rating {

struct Rating {
    double mu;
    double sigma;
};

using Team = std::vector<Rating>;
using RatingGroups = std::vector<Team>;
using Weights = std::vector<std::vector<double>>;

class Environment {
public:
    Environment();
    Environment(double mu, double sigma, double beta, double tau, double draw_probability);

    [[nodiscard]] double mu() const noexcept;
    [[nodiscard]] double sigma() const noexcept;
    [[nodiscard]] double beta() const noexcept;
    [[nodiscard]] double tau() const noexcept;
    [[nodiscard]] double draw_probability() const noexcept;

    [[nodiscard]] Rating create_rating() const noexcept;
    [[nodiscard]] Rating create_rating(double mu, double sigma) const;
    [[nodiscard]] double expose(const Rating& rating) const;

    // Updates arbitrary teams. Lower rank values are better results, equal ranks
    // are draws, and omitted ranks default to input order. Returned groups keep
    // the same team/player ordering as rating_groups.
    [[nodiscard]] RatingGroups rate(
        const RatingGroups& rating_groups,
        const std::vector<int>& ranks = {},
        const Weights& weights = {},
        double min_delta = 0.0001) const;

    [[nodiscard]] double quality(const RatingGroups& rating_groups, const Weights& weights = {}) const;

    // Computes the predictive probability that exactly two teams draw.
    // Free-for-all draw probability is not supported.
    [[nodiscard]] double draw_probability(const RatingGroups& rating_groups, const Weights& weights = {}) const;

    // Updates a head-to-head match. If drawn is false, first_player is the
    // winner and second_player is the loser. If drawn is true, both players are
    // treated as tied. The returned pair matches the input order.
    [[nodiscard]] std::pair<Rating, Rating> rate_1vs1(
        const Rating& first_player,
        const Rating& second_player,
        bool drawn = false,
        double min_delta = 0.0001) const;

    [[nodiscard]] double quality_1vs1(const Rating& a, const Rating& b) const;
    [[nodiscard]] double draw_probability_1vs1(const Rating& a, const Rating& b) const;

private:
    double mu_;
    double sigma_;
    double beta_;
    double tau_;
    double draw_probability_;
};

[[nodiscard]] double calc_draw_margin(double draw_probability, std::size_t player_count, double beta);
[[nodiscard]] double calc_draw_probability(double draw_margin, std::size_t player_count, double beta);

} // namespace skill_rating
