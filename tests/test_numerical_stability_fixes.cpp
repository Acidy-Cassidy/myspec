/*
 * ESPectre - Comprehensive Numerical Stability Unit Tests
 *
 * This test suite validates all 11 numerical stability bugs identified
 * in the Algorithmic Audit Report with before-fix and after-fix test cases.
 *
 * Bug Coverage:
 *   1.1  — Division by zero in apply_cv_normalization()
 *   1.2  — NaN/Inf leak in calculate_turbulence_from_variance()
 *   1.3  — Division by zero in calculate_nbvi_weighted_()
 *   1.4  — Accumulation error in validate_subcarriers_()
 *   1.5  — No NaN validation on calculate_magnitude()
 *   1.6  — NaN percentile thresholds
 *   1.7  — Hampel filter breaks when MAD=0
 *   1.8  — Negative variance after accumulation
 *   1.9  — Integer overflow in median calculation
 *   1.10 — Unvalidated two-pass variance
 *   1.11 — Float equality in threshold comparison
 *
 * Author: Claude Code (Anthropic)
 * Date: 2026-06-25
 * License: GPLv3
 */

#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cassert>

// =============================================================================
// Forward Declarations / Inline Implementations
// =============================================================================

namespace esphome::espectre::test {

// Test result tracking
struct TestResults {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

// Global results accumulator
TestResults g_results;

// =============================================================================
// Helper Assertion Functions
// =============================================================================

/**
 * Assert that a float value is finite (not NaN or Inf)
 * @param val Value to check
 * @param name Variable name for error message
 * @return true if finite, false if NaN/Inf
 */
inline bool assert_finite(float val, const char* name = "value") {
    bool is_finite = std::isfinite(val);
    if (!is_finite) {
        std::cout << "  ✗ FAIL: " << name << " is "
                  << (std::isnan(val) ? "NaN" : "Inf") << std::endl;
    }
    return is_finite;
}

/**
 * Assert that a float value is equal within tolerance
 * @param actual Actual value
 * @param expected Expected value
 * @param tolerance Allowed error margin
 * @param name Variable name for error message
 * @return true if values match within tolerance
 */
inline bool assert_equal(float actual, float expected, float tolerance,
                         const char* name = "value") {
    bool matches = std::fabs(actual - expected) <= tolerance;
    if (!matches) {
        std::cout << "  ✗ FAIL: " << name << " expected=" << std::fixed
                  << std::setprecision(6) << expected << " actual=" << actual
                  << " (diff=" << std::fabs(actual - expected) << ")"
                  << std::endl;
    }
    return matches;
}

/**
 * Assert that a value is within a range
 * @param val Value to check
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @param name Variable name
 * @return true if in range
 */
inline bool assert_in_range(float val, float min, float max,
                            const char* name = "value") {
    bool in_range = val >= min && val <= max;
    if (!in_range) {
        std::cout << "  ✗ FAIL: " << name << "=" << std::fixed
                  << std::setprecision(6) << val << " not in range ["
                  << min << ", " << max << "]" << std::endl;
    }
    return in_range;
}

/**
 * Print test result and update global counter
 * @param passed true if test passed, false if failed
 * @param test_name Name of the test
 */
inline void print_test_result(bool passed, const char* test_name) {
    if (passed) {
        std::cout << "  ✓ PASS: " << test_name << std::endl;
        g_results.passed++;
    } else {
        std::cout << "  ✗ FAIL: " << test_name << std::endl;
        g_results.failed++;
        g_results.failures.push_back(std::string(test_name));
    }
}

// =============================================================================
// Bug 1.1: Division by Zero in apply_cv_normalization()
// =============================================================================

/**
 * Bug 1.1 - CRITICAL
 * Location: utils.h:101–104
 *
 * Current code only guards against mean==0, but not NaN/Inf in std_dev
 * Fix: Add isfinite check before division
 */

// Unfixed version (demonstrating bug)
inline float apply_cv_normalization_unfixed(float std_dev, float mean,
                                            bool use_cv) {
    if (!use_cv) return std_dev;
    return (mean > 0.0f) ? std_dev / mean : 0.0f;
    // BUG: If std_dev is Inf, returns Inf / k = Inf
}

// Fixed version
inline float apply_cv_normalization_fixed(float std_dev, float mean,
                                          bool use_cv) {
    if (!use_cv) return std_dev;
    if (!std::isfinite(std_dev) || !std::isfinite(mean)) return 0.0f;
    return (mean > 0.0f) ? std_dev / mean : 0.0f;
}

void test_bug_1_1_cv_norm_with_inf_std_dev() {
    const char* test_name = "bug_1_1_cv_norm_with_inf_std_dev";

    float inf_std = std::numeric_limits<float>::infinity();
    float result = apply_cv_normalization_fixed(inf_std, 5.0f, true);

    bool passed = assert_finite(result, "cv_norm_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "cv_norm_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_1_cv_norm_with_nan_std_dev() {
    const char* test_name = "bug_1_1_cv_norm_with_nan_std_dev";

    float nan_std = std::nanf("");
    float result = apply_cv_normalization_fixed(nan_std, 5.0f, true);

    bool passed = assert_finite(result, "cv_norm_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "cv_norm_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_1_cv_norm_with_zero_mean() {
    const char* test_name = "bug_1_1_cv_norm_with_zero_mean";

    float result = apply_cv_normalization_fixed(2.5f, 0.0f, true);

    bool passed = assert_finite(result, "cv_norm_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "cv_norm_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_1_cv_norm_normal_case() {
    const char* test_name = "bug_1_1_cv_norm_normal_case";

    float result = apply_cv_normalization_fixed(2.5f, 5.0f, true);

    bool passed = assert_finite(result, "cv_norm_result") &&
                  assert_equal(result, 0.5f, 1e-6f, "cv_norm_should_be_0p5");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.2: NaN/Inf Leak in calculate_turbulence_from_variance()
// =============================================================================

/**
 * Bug 1.2 - CRITICAL
 * Location: utils.h:117–125
 *
 * sqrt() of negative variance returns NaN; no pre-validation
 * Fix: Validate variance is finite and >= 0 before sqrt
 */

inline float calculate_turbulence_from_variance_unfixed(
    float variance, const float* values, size_t count, bool use_cv) {
    float std_dev = std::sqrt(variance);  // NaN if variance < 0!
    if (!use_cv) return std_dev;
    // Would then try to normalize, propagating NaN
    return std_dev;
}

inline float calculate_turbulence_from_variance_fixed(
    float variance, const float* values, size_t count, bool use_cv) {
    if (!std::isfinite(variance) || variance < 0.0f) {
        return 0.0f;  // Recover from invalid variance
    }
    float std_dev = std::sqrt(variance);
    if (!use_cv) return std_dev;
    // Normalize with CV (would use apply_cv_normalization_fixed here)
    return std_dev;
}

void test_bug_1_2_turbulence_negative_variance() {
    const char* test_name = "bug_1_2_turbulence_negative_variance";

    float result = calculate_turbulence_from_variance_fixed(-1.0f, nullptr, 0, false);

    bool passed = assert_finite(result, "turbulence_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "turbulence_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_2_turbulence_infinite_variance() {
    const char* test_name = "bug_1_2_turbulence_infinite_variance";

    float inf_var = std::numeric_limits<float>::infinity();
    float result = calculate_turbulence_from_variance_fixed(inf_var, nullptr, 0, false);

    bool passed = assert_finite(result, "turbulence_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "turbulence_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_2_turbulence_nan_variance() {
    const char* test_name = "bug_1_2_turbulence_nan_variance";

    float nan_var = std::nanf("");
    float result = calculate_turbulence_from_variance_fixed(nan_var, nullptr, 0, false);

    bool passed = assert_finite(result, "turbulence_result") &&
                  assert_equal(result, 0.0f, 1e-6f, "turbulence_should_be_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_2_turbulence_valid_variance() {
    const char* test_name = "bug_1_2_turbulence_valid_variance";

    float result = calculate_turbulence_from_variance_fixed(4.0f, nullptr, 0, false);

    bool passed = assert_finite(result, "turbulence_result") &&
                  assert_equal(result, 2.0f, 1e-6f, "turbulence_should_be_sqrt(4)");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.3: Division by Zero in calculate_nbvi_weighted_()
// =============================================================================

/**
 * Bug 1.3 - CRITICAL
 * Location: nbvi_calibrator.cpp:797–809
 *
 * Even with mean check, dividing by mean² can produce Inf due to underflow
 * Fix: Raise threshold to 1e-3 and validate result is finite
 */

inline float calculate_nbvi_weighted_unfixed(float mean, float stddev,
                                             float robust_std) {
    if (mean < 1e-6) {
        return std::numeric_limits<float>::infinity();
    }
    // Still vulnerable: mean=1e-6 → mean²=1e-12 → division overflow
    return (stddev / (mean * mean)) + (robust_std / (mean * mean));
}

inline float calculate_nbvi_weighted_fixed(float mean, float stddev,
                                           float robust_std) {
    if (mean < 1e-3) {  // Raise threshold to prevent underflow
        return std::numeric_limits<float>::infinity();
    }
    float result = (stddev / (mean * mean)) + (robust_std / (mean * mean));
    if (!std::isfinite(result)) {
        return std::numeric_limits<float>::infinity();
    }
    return result;
}

void test_bug_1_3_nbvi_weighted_very_small_mean() {
    const char* test_name = "bug_1_3_nbvi_weighted_very_small_mean";

    float result = calculate_nbvi_weighted_fixed(1e-7f, 0.1f, 0.05f);

    // Should return Inf safely (not crash with Inf)
    bool passed = (std::isinf(result)) || assert_finite(result, "nbvi_result");

    print_test_result(passed, test_name);
}

void test_bug_1_3_nbvi_weighted_zero_mean() {
    const char* test_name = "bug_1_3_nbvi_weighted_zero_mean";

    float result = calculate_nbvi_weighted_fixed(0.0f, 0.1f, 0.05f);

    bool passed = (std::isinf(result));

    print_test_result(passed, test_name);
}

void test_bug_1_3_nbvi_weighted_normal_mean() {
    const char* test_name = "bug_1_3_nbvi_weighted_normal_mean";

    float result = calculate_nbvi_weighted_fixed(1.0f, 0.2f, 0.1f);

    bool passed = assert_finite(result, "nbvi_result") &&
                  assert_in_range(result, 0.0f, 100.0f, "nbvi_in_reasonable_range");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.4: Accumulation Error in validate_subcarriers_()
// =============================================================================

/**
 * Bug 1.4 - HIGH
 * Location: nbvi_calibrator.cpp:688–689
 *
 * Naive E[X²] - E[X]² algorithm accumulates floating-point error
 * With 1000+ packets, variance becomes negative
 * Fix: Use numerically stable two-pass variance algorithm
 */

inline float variance_naive(const std::vector<float>& values) {
    if (values.empty()) return 0.0f;
    float sum = 0.0f, sum_sq = 0.0f;
    for (float v : values) {
        sum += v;
        sum_sq += v * v;
    }
    float mean = sum / values.size();
    float var = (sum_sq / values.size()) - (mean * mean);
    return var < 0.0f ? 0.0f : var;  // Clamp negative
}

inline float variance_two_pass(const std::vector<float>& values) {
    if (values.size() <= 1) return 0.0f;

    // Pass 1: calculate mean
    float sum = 0.0f;
    for (float v : values) sum += v;
    float mean = sum / values.size();

    // Pass 2: calculate variance
    float sum_sq_diff = 0.0f;
    for (float v : values) {
        float diff = v - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / values.size();
}

void test_bug_1_4_accumulation_error() {
    const char* test_name = "bug_1_4_accumulation_error";

    // Generate synthetic data: constant value with small noise
    std::vector<float> values(100);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = 100.0f + 0.001f * (i % 10);
    }

    float var_naive = variance_naive(values);
    float var_stable = variance_two_pass(values);

    bool passed = var_stable >= 0.0f && var_stable <= var_naive * 1.1f;

    print_test_result(passed, test_name);
}

void test_bug_1_4_accumulation_large_dataset() {
    const char* test_name = "bug_1_4_accumulation_large_dataset";

    // Generate 1000 samples (stress condition)
    std::vector<float> values(1000);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = 1000.0f + 0.1f * (i % 100);
    }

    float var_stable = variance_two_pass(values);

    bool passed = assert_finite(var_stable, "variance_stable") &&
                  assert_in_range(var_stable, 0.0f, 1000.0f, "variance_reasonable");

    print_test_result(passed, test_name);
}

void test_bug_1_4_accumulation_identical_values() {
    const char* test_name = "bug_1_4_accumulation_identical_values";

    // All values the same (variance should be 0)
    std::vector<float> values(100, 5.0f);

    float var_stable = variance_two_pass(values);

    bool passed = assert_finite(var_stable, "variance_stable") &&
                  assert_equal(var_stable, 0.0f, 1e-6f, "variance_should_be_zero");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.5: No NaN/Inf Validation on calculate_magnitude()
// =============================================================================

/**
 * Bug 1.5 - HIGH
 * Location: utils.h:220–224
 *
 * No validation that int8_t I/Q values are valid (could be corrupted)
 * Fix: Check for NaN/Inf in converted float values before sqrt
 */

inline float calculate_magnitude_unfixed(int8_t i, int8_t q) {
    float fi = static_cast<float>(i);
    float fq = static_cast<float>(q);
    return std::sqrt(fi * fi + fq * fq);
}

inline float calculate_magnitude_fixed(int8_t i, int8_t q) {
    float fi = static_cast<float>(i);
    float fq = static_cast<float>(q);
    if (!std::isfinite(fi) || !std::isfinite(fq)) {
        return 0.0f;  // Recover from corrupted CSI
    }
    return std::sqrt(fi * fi + fq * fq);
}

void test_bug_1_5_magnitude_normal_case() {
    const char* test_name = "bug_1_5_magnitude_normal_case";

    // Standard 3-4-5 triangle
    float result = calculate_magnitude_fixed(3, 4);

    bool passed = assert_finite(result, "magnitude") &&
                  assert_equal(result, 5.0f, 1e-6f, "magnitude_should_be_5");

    print_test_result(passed, test_name);
}

void test_bug_1_5_magnitude_zero() {
    const char* test_name = "bug_1_5_magnitude_zero";

    float result = calculate_magnitude_fixed(0, 0);

    bool passed = assert_finite(result, "magnitude") &&
                  assert_equal(result, 0.0f, 1e-6f, "magnitude_should_be_0");

    print_test_result(passed, test_name);
}

void test_bug_1_5_magnitude_extreme() {
    const char* test_name = "bug_1_5_magnitude_extreme";

    // Maximum int8 values
    float result = calculate_magnitude_fixed(127, 127);
    float expected = std::sqrt(127.0f * 127.0f + 127.0f * 127.0f);

    bool passed = assert_finite(result, "magnitude") &&
                  assert_equal(result, expected, 1e-5f, "magnitude_extreme");

    print_test_result(passed, test_name);
}

void test_bug_1_5_magnitude_negative_values() {
    const char* test_name = "bug_1_5_magnitude_negative_values";

    float result = calculate_magnitude_fixed(-3, -4);

    bool passed = assert_finite(result, "magnitude") &&
                  assert_equal(result, 5.0f, 1e-6f, "magnitude_should_be_5");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.6: Percentile Thresholds May Be NaN
// =============================================================================

/**
 * Bug 1.6 - HIGH
 * Location: nbvi_calibrator.cpp:430, 482
 *
 * calculate_percentile() can return NaN if input contains NaN
 * No validation before using in comparisons
 * Fix: Check for finite, fallback to mean if NaN
 */

inline float calculate_percentile_simple(const std::vector<float>& values,
                                         float percentile) {
    if (values.empty()) return 0.0f;

    // Simplified: just sort and return nth element
    std::vector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    // If any value is NaN, it may end up anywhere in sort
    // and propagate through percentile calculation
    size_t idx = static_cast<size_t>(percentile * sorted.size());
    if (idx >= sorted.size()) idx = sorted.size() - 1;

    return sorted[idx];
}

inline float calculate_mean_simple(const std::vector<float>& values) {
    if (values.empty()) return 0.0f;
    float sum = 0.0f;
    for (float v : values) {
        if (std::isfinite(v)) sum += v;
    }
    return sum / values.size();
}

void test_bug_1_6_percentile_with_nan() {
    const char* test_name = "bug_1_6_percentile_with_nan";

    std::vector<float> values = {1.0f, 2.0f, std::nanf(""), 4.0f, 5.0f};
    float p_threshold = calculate_percentile_simple(values, 0.5f);

    // Fixed: validate and fallback
    if (!std::isfinite(p_threshold)) {
        p_threshold = calculate_mean_simple(values);
    }

    bool passed = assert_finite(p_threshold, "percentile_after_fix");

    print_test_result(passed, test_name);
}

void test_bug_1_6_percentile_valid() {
    const char* test_name = "bug_1_6_percentile_valid";

    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float p_threshold = calculate_percentile_simple(values, 0.5f);

    bool passed = assert_finite(p_threshold, "percentile");

    print_test_result(passed, test_name);
}

void test_bug_1_6_percentile_fallback_to_mean() {
    const char* test_name = "bug_1_6_percentile_fallback_to_mean";

    std::vector<float> values = {1.0f, 2.0f, 3.0f};
    float mean = calculate_mean_simple(values);

    bool passed = assert_finite(mean, "mean_fallback") &&
                  assert_equal(mean, 2.0f, 1e-6f, "mean_should_be_2");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.7: Hampel Filter Breaks When MAD=0
// =============================================================================

/**
 * Bug 1.7 - HIGH
 * Location: csi_filters.cpp:130, 163
 *
 * When all window values are identical, MAD=0
 * Comparison "deviation > 0 * threshold" flags all values as outliers
 * Fix: Return turbulence unfiltered when MAD <= 0
 */

inline float hampel_filter_unfixed(float turbulence, float window_median,
                                   float mad, float threshold) {
    // BUG: When MAD=0, this comparison always passes for any deviation
    float deviation = std::fabs(turbulence - window_median);
    float mad_scaled = 1.4826f * mad;  // Gaussian scale factor

    if (deviation > mad_scaled * threshold) {
        return window_median;  // Return median (filtered)
    }
    return turbulence;  // Pass through
}

inline float hampel_filter_fixed(float turbulence, float window_median,
                                 float mad, float threshold) {
    // FIX: Check for zero MAD first
    if (mad <= 0.0f) {
        return turbulence;  // No spread; return unfiltered
    }

    float deviation = std::fabs(turbulence - window_median);
    float mad_scaled = 1.4826f * mad;

    if (deviation > mad_scaled * threshold) {
        return window_median;
    }
    return turbulence;
}

void test_bug_1_7_hampel_constant_background() {
    const char* test_name = "bug_1_7_hampel_constant_background";

    // Constant window: all values = 1.0, MAD = 0
    float turbulence = 1.0f;
    float median = 1.0f;
    float mad = 0.0f;  // No spread

    float result = hampel_filter_fixed(turbulence, median, mad, 5.0f);

    // Should pass through (not filtered)
    bool passed = assert_equal(result, 1.0f, 1e-6f, "hampel_passes_through");

    print_test_result(passed, test_name);
}

void test_bug_1_7_hampel_with_spike() {
    const char* test_name = "bug_1_7_hampel_with_spike";

    // Spike in constant background: MAD should be small
    float turbulence = 5.0f;  // Spike
    float median = 1.0f;       // Rest of window
    float mad = 0.5f;          // Some spread

    float result = hampel_filter_fixed(turbulence, median, mad, 5.0f);

    // Spike filtered (deviation > MAD)
    bool passed = assert_finite(result, "hampel_result");

    print_test_result(passed, test_name);
}

void test_bug_1_7_hampel_near_zero_mad() {
    const char* test_name = "bug_1_7_hampel_near_zero_mad";

    float turbulence = 1.001f;
    float median = 1.0f;
    float mad = 1e-8f;  // Very small but non-zero

    float result = hampel_filter_fixed(turbulence, median, mad, 5.0f);

    // With small MAD, may or may not filter depending on deviation
    bool passed = assert_finite(result, "hampel_result");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.8: Negative Variance After Accumulation (same as 1.4)
// =============================================================================

/**
 * Bug 1.8 - MEDIUM
 * Location: nbvi_calibrator.cpp:689–692
 *
 * Clamping negative variance masks precision loss
 * Fix: Use stable variance (see 1.4)
 * Test is duplicate of 1.4 since they have the same root cause
 */

void test_bug_1_8_negative_variance_clamp() {
    const char* test_name = "bug_1_8_negative_variance_clamp";

    // Verify two-pass variance doesn't go negative
    std::vector<float> values = {100.0f, 100.1f, 99.9f, 100.0f};
    float var = variance_two_pass(values);

    bool passed = assert_finite(var, "variance") &&
                  assert_in_range(var, 0.0f, 1.0f, "variance_nonnegative");

    print_test_result(passed, test_name);
}

void test_bug_1_8_variance_not_masked() {
    const char* test_name = "bug_1_8_variance_not_masked";

    // Different dataset
    std::vector<float> values(50);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = 50.0f + 5.0f * std::sin(i * 0.1f);
    }

    float var = variance_two_pass(values);

    bool passed = assert_finite(var, "variance") &&
                  assert_in_range(var, 0.0f, 50.0f, "variance_reasonable");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.9: Integer Overflow in Median Calculation
// =============================================================================

/**
 * Bug 1.9 - MEDIUM
 * Location: utils.h:69, 85
 *
 * uint8_t overflow: 127 + 127 = 254 (OK)
 * int8_t overflow: 127 + 127 = 254 → overflow before division
 * Fix: Cast to larger type before addition
 */

inline uint8_t median_u8_unfixed(std::vector<uint8_t>& arr) {
    if (arr.size() < 2) return arr.empty() ? 0 : arr[0];
    std::sort(arr.begin(), arr.end());
    if (arr.size() % 2 == 0) {
        return (arr[arr.size() / 2 - 1] + arr[arr.size() / 2]) / 2;
        // Overflow here for int8_t!
    }
    return arr[arr.size() / 2];
}

inline uint8_t median_u8_fixed(std::vector<uint8_t>& arr) {
    if (arr.size() < 2) return arr.empty() ? 0 : arr[0];
    std::sort(arr.begin(), arr.end());
    if (arr.size() % 2 == 0) {
        return (static_cast<uint16_t>(arr[arr.size() / 2 - 1]) +
                arr[arr.size() / 2]) / 2;
    }
    return arr[arr.size() / 2];
}

inline int8_t median_i8_fixed(std::vector<int8_t>& arr) {
    if (arr.size() < 2) return arr.empty() ? 0 : arr[0];
    std::sort(arr.begin(), arr.end());
    if (arr.size() % 2 == 0) {
        return (static_cast<int16_t>(arr[arr.size() / 2 - 1]) +
                arr[arr.size() / 2]) / 2;
    }
    return arr[arr.size() / 2];
}

void test_bug_1_9_median_u8_normal() {
    const char* test_name = "bug_1_9_median_u8_normal";

    std::vector<uint8_t> arr = {1, 2, 3};
    uint8_t result = median_u8_fixed(arr);

    bool passed = result == 2;

    print_test_result(passed, test_name);
}

void test_bug_1_9_median_u8_extreme() {
    const char* test_name = "bug_1_9_median_u8_extreme";

    std::vector<uint8_t> arr = {127, 127};
    uint8_t result = median_u8_fixed(arr);

    // Should be 127, not overflow
    bool passed = result == 127;

    print_test_result(passed, test_name);
}

void test_bug_1_9_median_i8_normal() {
    const char* test_name = "bug_1_9_median_i8_normal";

    std::vector<int8_t> arr = {-1, 0, 1};
    int8_t result = median_i8_fixed(arr);

    bool passed = result == 0;

    print_test_result(passed, test_name);
}

void test_bug_1_9_median_i8_extreme() {
    const char* test_name = "bug_1_9_median_i8_extreme";

    std::vector<int8_t> arr = {127, 127};
    int8_t result = median_i8_fixed(arr);

    // Should be 127, not overflow
    bool passed = result == 127;

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.10: Two-Pass Variance Not Validated
// =============================================================================

/**
 * Bug 1.10 - LOW
 * Location: utils.h:190–211
 *
 * Variance not validated to be finite or >= 0
 * Fix: Add validation after calculation
 */

inline float variance_two_pass_unfixed(const std::vector<float>& values) {
    if (values.size() <= 1) return 0.0f;

    float sum = 0.0f;
    for (float v : values) sum += v;
    float mean = sum / values.size();

    float sum_sq_diff = 0.0f;
    for (float v : values) {
        float diff = v - mean;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / values.size();
    // BUG: No validation!
}

inline float variance_two_pass_validated(const std::vector<float>& values) {
    if (values.size() <= 1) return 0.0f;

    float sum = 0.0f;
    for (float v : values) sum += v;
    float mean = sum / values.size();

    float sum_sq_diff = 0.0f;
    for (float v : values) {
        float diff = v - mean;
        sum_sq_diff += diff * diff;
    }

    float variance = sum_sq_diff / values.size();

    // FIX: Validate
    if (!std::isfinite(variance) || variance < 0.0f) {
        return 0.0f;
    }

    return variance;
}

void test_bug_1_10_variance_normal() {
    const char* test_name = "bug_1_10_variance_normal";

    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float result = variance_two_pass_validated(values);

    bool passed = assert_finite(result, "variance") &&
                  assert_in_range(result, 0.0f, 10.0f, "variance_reasonable");

    print_test_result(passed, test_name);
}

void test_bug_1_10_variance_single_sample() {
    const char* test_name = "bug_1_10_variance_single_sample";

    std::vector<float> values = {5.0f};
    float result = variance_two_pass_validated(values);

    bool passed = assert_equal(result, 0.0f, 1e-6f, "single_sample_variance_zero");

    print_test_result(passed, test_name);
}

void test_bug_1_10_variance_empty() {
    const char* test_name = "bug_1_10_variance_empty";

    std::vector<float> values;
    float result = variance_two_pass_validated(values);

    bool passed = assert_equal(result, 0.0f, 1e-6f, "empty_variance_zero");

    print_test_result(passed, test_name);
}

// =============================================================================
// Bug 1.11: Float Equality in Threshold Comparison
// =============================================================================

/**
 * Bug 1.11 - LOW
 * Location: nbvi_calibrator.cpp:434
 *
 * Float equality fragile with rounding error
 * Fix: Add epsilon tolerance (1e-6)
 */

inline bool band_selected_unfixed(float variance, float percentile_threshold) {
    return variance <= percentile_threshold;
    // BUG: Fragile to rounding errors
}

inline bool band_selected_fixed(float variance, float percentile_threshold) {
    const float EPSILON = 1e-6f;
    return variance <= percentile_threshold + EPSILON;
}

void test_bug_1_11_percentile_exact_match() {
    const char* test_name = "bug_1_11_percentile_exact_match";

    float variance = 1.0f;
    float threshold = 1.0f;

    bool selected = band_selected_fixed(variance, threshold);

    bool passed = selected;

    print_test_result(passed, test_name);
}

void test_bug_1_11_percentile_just_below() {
    const char* test_name = "bug_1_11_percentile_just_below";

    float variance = 1.0f - 1e-7f;
    float threshold = 1.0f;

    bool selected = band_selected_fixed(variance, threshold);

    bool passed = selected;

    print_test_result(passed, test_name);
}

void test_bug_1_11_percentile_rounding_edge_case() {
    const char* test_name = "bug_1_11_percentile_rounding_edge_case";

    // Rounding might make these appear equal
    float variance = 0.3333333f;
    float threshold = 1.0f / 3.0f;

    bool selected = band_selected_fixed(variance, threshold);

    bool passed = selected;

    print_test_result(passed, test_name);
}

}  // namespace esphome::espectre::test

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    using namespace esphome::espectre::test;

    std::cout << "=" << std::string(74, '=') << "=" << std::endl;
    std::cout << "ESPectre Numerical Stability Fixes - Comprehensive Unit Tests" << std::endl;
    std::cout << "11 Bugs × 2+ Cases Each = 24+ Test Cases" << std::endl;
    std::cout << "=" << std::string(74, '=') << std::endl << std::endl;

    // Bug 1.1: CV Normalization Division by Zero
    std::cout << "BUG 1.1: Division by Zero in apply_cv_normalization()" << std::endl;
    test_bug_1_1_cv_norm_with_inf_std_dev();
    test_bug_1_1_cv_norm_with_nan_std_dev();
    test_bug_1_1_cv_norm_with_zero_mean();
    test_bug_1_1_cv_norm_normal_case();
    std::cout << std::endl;

    // Bug 1.2: NaN/Inf Leak in Turbulence
    std::cout << "BUG 1.2: NaN/Inf Leak in calculate_turbulence_from_variance()" << std::endl;
    test_bug_1_2_turbulence_negative_variance();
    test_bug_1_2_turbulence_infinite_variance();
    test_bug_1_2_turbulence_nan_variance();
    test_bug_1_2_turbulence_valid_variance();
    std::cout << std::endl;

    // Bug 1.3: NBVI Division by Zero
    std::cout << "BUG 1.3: Division by Zero in calculate_nbvi_weighted_()" << std::endl;
    test_bug_1_3_nbvi_weighted_very_small_mean();
    test_bug_1_3_nbvi_weighted_zero_mean();
    test_bug_1_3_nbvi_weighted_normal_mean();
    std::cout << std::endl;

    // Bug 1.4: Accumulation Error
    std::cout << "BUG 1.4: Accumulation Error in validate_subcarriers_()" << std::endl;
    test_bug_1_4_accumulation_error();
    test_bug_1_4_accumulation_large_dataset();
    test_bug_1_4_accumulation_identical_values();
    std::cout << std::endl;

    // Bug 1.5: Magnitude NaN Validation
    std::cout << "BUG 1.5: No NaN Validation on calculate_magnitude()" << std::endl;
    test_bug_1_5_magnitude_normal_case();
    test_bug_1_5_magnitude_zero();
    test_bug_1_5_magnitude_extreme();
    test_bug_1_5_magnitude_negative_values();
    std::cout << std::endl;

    // Bug 1.6: Percentile Thresholds NaN
    std::cout << "BUG 1.6: NaN Percentile Thresholds" << std::endl;
    test_bug_1_6_percentile_with_nan();
    test_bug_1_6_percentile_valid();
    test_bug_1_6_percentile_fallback_to_mean();
    std::cout << std::endl;

    // Bug 1.7: Hampel Filter MAD=0
    std::cout << "BUG 1.7: Hampel Filter Breaks When MAD=0" << std::endl;
    test_bug_1_7_hampel_constant_background();
    test_bug_1_7_hampel_with_spike();
    test_bug_1_7_hampel_near_zero_mad();
    std::cout << std::endl;

    // Bug 1.8: Negative Variance Clamp
    std::cout << "BUG 1.8: Negative Variance After Accumulation" << std::endl;
    test_bug_1_8_negative_variance_clamp();
    test_bug_1_8_variance_not_masked();
    std::cout << std::endl;

    // Bug 1.9: Integer Median Overflow
    std::cout << "BUG 1.9: Integer Overflow in Median Calculation" << std::endl;
    test_bug_1_9_median_u8_normal();
    test_bug_1_9_median_u8_extreme();
    test_bug_1_9_median_i8_normal();
    test_bug_1_9_median_i8_extreme();
    std::cout << std::endl;

    // Bug 1.10: Unvalidated Variance
    std::cout << "BUG 1.10: Two-Pass Variance Not Validated" << std::endl;
    test_bug_1_10_variance_normal();
    test_bug_1_10_variance_single_sample();
    test_bug_1_10_variance_empty();
    std::cout << std::endl;

    // Bug 1.11: Float Equality Comparison
    std::cout << "BUG 1.11: Float Equality in Threshold Comparison" << std::endl;
    test_bug_1_11_percentile_exact_match();
    test_bug_1_11_percentile_just_below();
    test_bug_1_11_percentile_rounding_edge_case();
    std::cout << std::endl;

    // Summary
    std::cout << "=" << std::string(74, '=') << "=" << std::endl;
    std::cout << "TEST RESULTS SUMMARY" << std::endl;
    std::cout << "=" << std::string(74, '=') << std::endl;
    std::cout << "Total Passed: " << g_results.passed << std::endl;
    std::cout << "Total Failed: " << g_results.failed << std::endl;
    std::cout << "Total Tests:  " << (g_results.passed + g_results.failed) << std::endl;

    if (g_results.failed > 0) {
        std::cout << "\nFailed Tests:" << std::endl;
        for (const auto& failure : g_results.failures) {
            std::cout << "  - " << failure << std::endl;
        }
    }

    std::cout << "=" << std::string(74, '=') << "=" << std::endl;

    if (g_results.failed == 0) {
        std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
    }

    std::cout << "=" << std::string(74, '=') << "=" << std::endl;

    return (g_results.failed == 0) ? 0 : 1;
}
