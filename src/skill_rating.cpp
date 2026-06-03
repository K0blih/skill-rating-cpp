#include "skill_rating/skill_rating.hpp"

#include "detail/factor_graph.hpp"
#include "detail/math.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace skill_rating {
namespace {

using detail::Gaussian;
using detail::SumLayer;
using detail::Variable;
using detail::determinant;
using detail::kEpsilon;
using detail::require_finite_positive;
using detail::require_probability;
using detail::solve;
using detail::square;
using detail::update_likelihood_mean;
using detail::update_likelihood_value;
using detail::update_prior;
using detail::update_sum;
using detail::update_trunc;

constexpr int kMaxRateIterations = 100;

void validate_rating(const Rating& rating) {
    if (!std::isfinite(rating.mu)) {
        throw std::invalid_argument("rating mu must be finite");
    }
    require_finite_positive(rating.sigma, "rating sigma");
}

void validate_groups(const RatingGroups& groups) {
    if (groups.size() < 2) {
        throw std::invalid_argument("at least two teams are required");
    }
    for (const auto& team : groups) {
        if (team.empty()) {
            throw std::invalid_argument("teams must not be empty");
        }
        for (const Rating& rating : team) {
            validate_rating(rating);
        }
    }
}

Weights normalized_weights(const RatingGroups& groups, const Weights& weights) {
    if (weights.empty()) {
        Weights result;
        result.reserve(groups.size());
        for (const auto& team : groups) {
            result.emplace_back(team.size(), 1.0);
        }
        return result;
    }
    if (weights.size() != groups.size()) {
        throw std::invalid_argument("weights must match rating group count");
    }
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (weights[i].size() != groups[i].size()) {
            throw std::invalid_argument("weights must match team sizes");
        }
        for (double weight : weights[i]) {
            if (!std::isfinite(weight) || weight < 0.0) {
                throw std::invalid_argument("weights must be finite non-negative numbers");
            }
        }
    }
    return weights;
}

void validate_participating_teams(const RatingGroups& groups, const Weights& weights) {
    for (std::size_t team = 0; team < groups.size(); ++team) {
        const bool has_participant = std::any_of(weights[team].begin(), weights[team].end(), [](double weight) {
            return weight > 0.0;
        });
        if (!has_participant) {
            throw std::invalid_argument("each team must have at least one positive player weight");
        }
    }
}

std::size_t positive_weight_count(const std::vector<double>& weights) {
    return static_cast<std::size_t>(std::count_if(weights.begin(), weights.end(), [](double weight) {
        return weight > 0.0;
    }));
}

std::size_t active_player_count(const Weights& weights) {
    return std::accumulate(weights.begin(), weights.end(), std::size_t{0}, [](std::size_t total, const auto& team_weights) {
        return total + positive_weight_count(team_weights);
    });
}

std::vector<int> normalized_ranks(const RatingGroups& groups, const std::vector<int>& ranks) {
    if (ranks.empty()) {
        std::vector<int> result(groups.size());
        std::iota(result.begin(), result.end(), 0);
        return result;
    }
    if (ranks.size() != groups.size()) {
        throw std::invalid_argument("ranks must match rating group count");
    }
    return ranks;
}

std::vector<std::size_t> rank_order(const std::vector<int>& ranks) {
    std::vector<std::size_t> order(ranks.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        return ranks[lhs] < ranks[rhs];
    });
    return order;
}

} // namespace

Environment::Environment()
    : Environment(25.0, 25.0 / 3.0, 25.0 / 6.0, 25.0 / 300.0, 0.10) {}

Environment::Environment(double mu, double sigma, double beta, double tau, double draw_probability)
    : mu_(mu), sigma_(sigma), beta_(beta), tau_(tau), draw_probability_(draw_probability) {
    if (!std::isfinite(mu)) {
        throw std::invalid_argument("mu must be finite");
    }
    require_finite_positive(sigma, "sigma");
    require_finite_positive(beta, "beta");
    require_finite_positive(tau, "tau");
    require_probability(draw_probability, "draw_probability");
}

double Environment::mu() const noexcept { return mu_; }
double Environment::sigma() const noexcept { return sigma_; }
double Environment::beta() const noexcept { return beta_; }
double Environment::tau() const noexcept { return tau_; }
double Environment::draw_probability() const noexcept { return draw_probability_; }

Rating Environment::create_rating() const noexcept {
    return {mu_, sigma_};
}

Rating Environment::create_rating(double mu, double sigma) const {
    Rating rating{mu, sigma};
    validate_rating(rating);
    return rating;
}

double Environment::expose(const Rating& rating) const {
    validate_rating(rating);
    return rating.mu - 3.0 * rating.sigma;
}

RatingGroups Environment::rate(
    const RatingGroups& rating_groups,
    const std::vector<int>& ranks,
    const Weights& weights,
    double min_delta) const {
    validate_groups(rating_groups);
    if (!std::isfinite(min_delta) || min_delta <= 0.0) {
        throw std::invalid_argument("min_delta must be a finite positive number");
    }
    const Weights group_weights = normalized_weights(rating_groups, weights);
    validate_participating_teams(rating_groups, group_weights);
    const std::size_t total_active_players = active_player_count(group_weights);
    const std::vector<int> group_ranks = normalized_ranks(rating_groups, ranks);
    const std::vector<std::size_t> order = rank_order(group_ranks);

    std::vector<std::vector<Variable>> rating_vars(order.size());
    std::vector<std::vector<Variable>> perf_vars(order.size());
    std::vector<std::vector<std::size_t>> active_players(order.size());
    std::vector<Variable> team_perf_vars(order.size());
    std::vector<Gaussian> prior_messages;
    std::vector<Gaussian> perf_to_rating_messages;
    std::vector<Gaussian> rating_to_perf_messages;
    prior_messages.reserve(total_active_players);
    perf_to_rating_messages.reserve(total_active_players);
    rating_to_perf_messages.reserve(total_active_players);

    for (std::size_t sorted_i = 0; sorted_i < order.size(); ++sorted_i) {
        const std::size_t original_i = order[sorted_i];
        const auto& team = rating_groups[original_i];
        active_players[sorted_i].reserve(positive_weight_count(group_weights[original_i]));
        for (std::size_t player = 0; player < team.size(); ++player) {
            if (group_weights[original_i][player] > 0.0) {
                active_players[sorted_i].push_back(player);
            }
        }
        rating_vars[sorted_i].resize(active_players[sorted_i].size());
        perf_vars[sorted_i].resize(active_players[sorted_i].size());
        for (std::size_t active_i = 0; active_i < active_players[sorted_i].size(); ++active_i) {
            const std::size_t player = active_players[sorted_i][active_i];
            prior_messages.push_back({});
            update_prior(
                rating_vars[sorted_i][active_i],
                prior_messages.back(),
                Gaussian::from_mu_sigma(team[player].mu, std::sqrt(square(team[player].sigma) + square(tau_))));

            rating_to_perf_messages.push_back({});
            perf_to_rating_messages.push_back({});
            update_likelihood_value(
                perf_vars[sorted_i][active_i],
                rating_to_perf_messages.back(),
                rating_vars[sorted_i][active_i],
                perf_to_rating_messages.back(),
                square(beta_));
        }
    }

    std::vector<SumLayer> team_layers;
    team_layers.reserve(order.size());
    for (std::size_t sorted_i = 0; sorted_i < order.size(); ++sorted_i) {
        SumLayer layer;
        layer.variables.reserve(active_players[sorted_i].size() + 1);
        layer.messages.reserve(active_players[sorted_i].size() + 1);
        layer.coeffs.reserve(active_players[sorted_i].size() + 1);
        layer.variables.push_back(&team_perf_vars[sorted_i]);
        layer.messages.push_back({});
        layer.coeffs.push_back(1.0);
        const std::size_t original_i = order[sorted_i];
        for (std::size_t active_i = 0; active_i < perf_vars[sorted_i].size(); ++active_i) {
            const std::size_t player = active_players[sorted_i][active_i];
            layer.variables.push_back(&perf_vars[sorted_i][active_i]);
            layer.messages.push_back({});
            layer.coeffs.push_back(-group_weights[original_i][player]);
        }
        update_sum(layer.variables, layer.messages, layer.coeffs, 0);
        team_layers.push_back(std::move(layer));
    }

    std::vector<Variable> diff_vars(order.size() - 1);
    std::vector<SumLayer> diff_layers;
    std::vector<Gaussian> trunc_messages(order.size() - 1);
    diff_layers.reserve(order.size() - 1);
    std::vector<double> draw_margins(order.size() - 1);
    std::vector<bool> draws(order.size() - 1);
    for (std::size_t i = 0; i + 1 < order.size(); ++i) {
        SumLayer layer;
        layer.variables = {&diff_vars[i], &team_perf_vars[i], &team_perf_vars[i + 1]};
        layer.messages = {{}, {}, {}};
        layer.coeffs = {1.0, -1.0, 1.0};
        diff_layers.push_back(std::move(layer));

        const std::size_t left = order[i];
        const std::size_t right = order[i + 1];
        const std::size_t player_count = positive_weight_count(group_weights[left]) + positive_weight_count(group_weights[right]);
        draw_margins[i] = calc_draw_margin(draw_probability_, player_count, beta_);
        draws[i] = group_ranks[left] == group_ranks[right];
    }

    for (int iteration = 0; iteration < kMaxRateIterations; ++iteration) {
        double delta = 0.0;
        for (auto& layer : diff_layers) {
            delta = std::max(delta, update_sum(layer.variables, layer.messages, layer.coeffs, 0));
        }
        for (std::size_t i = 0; i < diff_vars.size(); ++i) {
            delta = std::max(delta, update_trunc(diff_vars[i], trunc_messages[i], draw_margins[i], draws[i]));
        }
        for (std::size_t reverse_i = diff_layers.size(); reverse_i-- > 0;) {
            delta = std::max(delta, update_sum(diff_layers[reverse_i].variables, diff_layers[reverse_i].messages, diff_layers[reverse_i].coeffs, 1));
            delta = std::max(delta, update_sum(diff_layers[reverse_i].variables, diff_layers[reverse_i].messages, diff_layers[reverse_i].coeffs, 2));
        }
        if (delta <= min_delta) {
            break;
        }
        if (iteration == kMaxRateIterations - 1) {
            throw std::runtime_error("Bayesian skill rating rate update did not converge");
        }
    }

    for (std::size_t sorted_i = 0; sorted_i < team_layers.size(); ++sorted_i) {
        for (std::size_t player = 0; player < perf_vars[sorted_i].size(); ++player) {
            update_sum(team_layers[sorted_i].variables, team_layers[sorted_i].messages, team_layers[sorted_i].coeffs, player + 1);
        }
    }

    std::size_t flat = 0;
    RatingGroups result = rating_groups;
    for (std::size_t sorted_i = 0; sorted_i < order.size(); ++sorted_i) {
        const std::size_t original_i = order[sorted_i];
        for (std::size_t active_i = 0; active_i < rating_vars[sorted_i].size(); ++active_i) {
            const std::size_t player = active_players[sorted_i][active_i];
            update_likelihood_mean(
                rating_vars[sorted_i][active_i],
                perf_to_rating_messages[flat],
                perf_vars[sorted_i][active_i],
                rating_to_perf_messages[flat],
                square(beta_));
            result[original_i][player] = {rating_vars[sorted_i][active_i].value.mu(), rating_vars[sorted_i][active_i].value.sigma()};
            ++flat;
        }
    }
    return result;
}

double Environment::quality(const RatingGroups& rating_groups, const Weights& weights) const {
    validate_groups(rating_groups);
    const Weights group_weights = normalized_weights(rating_groups, weights);
    validate_participating_teams(rating_groups, group_weights);
    const std::size_t total_active_players = active_player_count(group_weights);
    const std::size_t team_count = rating_groups.size();
    const std::size_t diff_count = team_count - 1;

    std::vector<double> mean(diff_count, 0.0);
    std::vector<std::vector<double>> ata(diff_count, std::vector<double>(diff_count, 0.0));
    std::vector<std::vector<double>> start(diff_count, std::vector<double>(diff_count, 0.0));

    std::vector<std::vector<double>> player_coeffs;
    std::vector<double> variances;
    player_coeffs.reserve(total_active_players);
    variances.reserve(total_active_players);
    for (std::size_t team = 0; team < team_count; ++team) {
        for (std::size_t player = 0; player < rating_groups[team].size(); ++player) {
            if (group_weights[team][player] == 0.0) {
                continue;
            }
            std::vector<double> coeff(diff_count, 0.0);
            if (team < diff_count) {
                coeff[team] = group_weights[team][player];
                mean[team] += group_weights[team][player] * rating_groups[team][player].mu;
            }
            if (team > 0) {
                coeff[team - 1] -= group_weights[team][player];
                mean[team - 1] -= group_weights[team][player] * rating_groups[team][player].mu;
            }
            player_coeffs.push_back(coeff);
            variances.push_back(square(rating_groups[team][player].sigma));
        }
    }

    for (std::size_t p = 0; p < player_coeffs.size(); ++p) {
        for (std::size_t row = 0; row < diff_count; ++row) {
            for (std::size_t col = 0; col < diff_count; ++col) {
                const double contribution = player_coeffs[p][row] * player_coeffs[p][col];
                ata[row][col] += contribution;
                start[row][col] += variances[p] * contribution;
            }
        }
    }
    for (std::size_t row = 0; row < diff_count; ++row) {
        for (std::size_t col = 0; col < diff_count; ++col) {
            start[row][col] += square(beta_) * ata[row][col];
        }
    }

    const double det_ata = determinant(ata);
    const double det_start = determinant(start);
    if (det_ata <= 0.0 || det_start <= 0.0) {
        return 0.0;
    }
    const std::vector<double> solved = solve(start, mean);
    const double exponent = -0.5 * std::inner_product(mean.begin(), mean.end(), solved.begin(), 0.0);
    const double sqrt_part = std::sqrt(std::pow(square(beta_), static_cast<double>(diff_count)) * det_ata / det_start);
    return std::clamp(sqrt_part * std::exp(exponent), 0.0, 1.0);
}

double Environment::draw_probability(const RatingGroups& rating_groups, const Weights& weights) const {
    validate_groups(rating_groups);
    if (rating_groups.size() != 2) {
        throw std::invalid_argument("free-for-all draw probability is not supported");
    }
    const Weights group_weights = normalized_weights(rating_groups, weights);
    validate_participating_teams(rating_groups, group_weights);

    double mean = 0.0;
    double variance = 0.0;
    std::size_t player_count = 0;
    for (std::size_t team = 0; team < rating_groups.size(); ++team) {
        const double team_sign = team == 0 ? 1.0 : -1.0;
        for (std::size_t player = 0; player < rating_groups[team].size(); ++player) {
            const double weight = group_weights[team][player];
            if (weight == 0.0) {
                continue;
            }
            ++player_count;
            const double coeff = team_sign * weight;
            mean += coeff * rating_groups[team][player].mu;
            variance += square(coeff) * (square(rating_groups[team][player].sigma) + square(beta_));
        }
    }

    const double draw_margin = calc_draw_margin(draw_probability_, player_count, beta_);
    const double stddev = std::sqrt(variance);
    const double probability =
        detail::normal_cdf((draw_margin - mean) / stddev) - detail::normal_cdf((-draw_margin - mean) / stddev);
    return std::clamp(probability, 0.0, 1.0);
}

std::pair<Rating, Rating> Environment::rate_1vs1(
    const Rating& first_player,
    const Rating& second_player,
    bool drawn,
    double min_delta) const {
    const RatingGroups rated = rate({{first_player}, {second_player}}, drawn ? std::vector<int>{0, 0} : std::vector<int>{0, 1}, {}, min_delta);
    return {rated[0][0], rated[1][0]};
}

double Environment::quality_1vs1(const Rating& a, const Rating& b) const {
    return quality({{a}, {b}});
}

double Environment::draw_probability_1vs1(const Rating& a, const Rating& b) const {
    return draw_probability({{a}, {b}});
}

double calc_draw_margin(double draw_probability, std::size_t player_count, double beta) {
    require_probability(draw_probability, "draw_probability");
    require_finite_positive(beta, "beta");
    if (player_count == 0) {
        throw std::invalid_argument("player_count must be positive");
    }
    return detail::inverse_normal_cdf((draw_probability + 1.0) / 2.0) * std::sqrt(static_cast<double>(player_count)) * beta;
}

double calc_draw_probability(double draw_margin, std::size_t player_count, double beta) {
    if (!std::isfinite(draw_margin) || draw_margin < 0.0) {
        throw std::invalid_argument("draw_margin must be a finite non-negative number");
    }
    require_finite_positive(beta, "beta");
    if (player_count == 0) {
        throw std::invalid_argument("player_count must be positive");
    }
    return 2.0 * detail::normal_cdf(draw_margin / (std::sqrt(static_cast<double>(player_count)) * beta)) - 1.0;
}

} // namespace skill_rating
