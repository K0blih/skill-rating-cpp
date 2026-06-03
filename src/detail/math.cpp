#include "detail/math.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace skill_rating::detail {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr int kInverseNormalRefinementIterations = 2;

} // namespace

void require_finite_positive(double value, const char* name) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be a finite positive number");
    }
}

void require_probability(double value, const char* name) {
    if (!std::isfinite(value) || value <= 0.0 || value >= 1.0) {
        throw std::invalid_argument(std::string(name) + " must be in (0, 1)");
    }
}

double square(double value) noexcept {
    return value * value;
}

double normal_pdf(double x) {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * kPi);
}

double normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double inverse_normal_cdf(double p) {
    require_probability(p, "p");

    constexpr double a[] = {
        -3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
        1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    constexpr double b[] = {
        -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
        6.680131188771972e+01, -1.328068155288572e+01};
    constexpr double c[] = {
        -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
        -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    constexpr double d[] = {
        7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
        3.754408661907416e+00};

    constexpr double low = 0.02425;
    constexpr double high = 1.0 - low;
    double x = 0.0;
    if (p < low) {
        const double q = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p <= high) {
        const double q = p - 0.5;
        const double r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }

    for (int i = 0; i < kInverseNormalRefinementIterations; ++i) {
        const double error = normal_cdf(x) - p;
        x -= error / normal_pdf(x);
    }
    return x;
}

double determinant(std::vector<std::vector<double>> matrix) {
    const std::size_t n = matrix.size();
    double det = 1.0;
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < n; ++row) {
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(matrix[pivot][col]) < kEpsilon) {
            return 0.0;
        }
        if (pivot != col) {
            std::swap(matrix[pivot], matrix[col]);
            det = -det;
        }
        det *= matrix[col][col];
        for (std::size_t row = col + 1; row < n; ++row) {
            const double factor = matrix[row][col] / matrix[col][col];
            for (std::size_t j = col; j < n; ++j) {
                matrix[row][j] -= factor * matrix[col][j];
            }
        }
    }
    return det;
}

std::vector<double> solve(std::vector<std::vector<double>> matrix, std::vector<double> rhs) {
    const std::size_t n = matrix.size();
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < n; ++row) {
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(matrix[pivot][col]) < kEpsilon) {
            throw std::runtime_error("singular quality matrix");
        }
        std::swap(matrix[pivot], matrix[col]);
        std::swap(rhs[pivot], rhs[col]);
        const double div = matrix[col][col];
        for (std::size_t j = col; j < n; ++j) {
            matrix[col][j] /= div;
        }
        rhs[col] /= div;
        for (std::size_t row = 0; row < n; ++row) {
            if (row == col) {
                continue;
            }
            const double factor = matrix[row][col];
            for (std::size_t j = col; j < n; ++j) {
                matrix[row][j] -= factor * matrix[col][j];
            }
            rhs[row] -= factor * rhs[col];
        }
    }
    return rhs;
}

} // namespace skill_rating::detail
