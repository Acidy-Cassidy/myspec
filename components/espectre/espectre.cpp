/*
 * ESPectre - Main Component Implementation
 *
 * WiFi CSI-based motion detection using Moving Variance Score (MVS).
 */

#include "espectre.h"
#include "threshold_number.h"
#include "calibrate_switch.h"
#include "utils.h"
#include "threshold.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <vector>
#include <string>
#include <span>

#include "sdkconfig.h"

namespace esphome {
namespace espectre {

void ESpectreComponent::setup() {
  ESP_LOGI(TAG, "Initializing ESPectre component...");

  esp_err_t wifi_init_err = this->wifi_lifecycle_.init();
  if (wifi_init_err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi lifecycle init failed: %s. ESPectre setup aborted.",
             esp_err_to_name(wifi_init_err));
    this->mark_failed();
    return;
  }

  this->mvs_detector_ = MVSDetector(this->segmentation_window_size_, this->segmentation_threshold_);
  this->mvs_detector_.configure_lowpass(this->lowpass_enabled_, this->lowpass_cutoff_);
  this->mvs_detector_.configure_hampel(this->hampel_enabled_, this->hampel_window_, this->hampel_threshold_);
  this->detector_ = &this->mvs_detector_;
  ESP_LOGI(TAG, "Using MVS detector (window=%d, threshold=%.2f)",
           this->segmentation_window_size_, this->segmentation_threshold_);

  this->nbvi_calibrator_.init(&this->csi_manager_);
  this->nbvi_calibrator_.set_mvs_window_size(this->segmentation_window_size_);
  this->nbvi_calibrator_.configure_lowpass(this->lowpass_enabled_, this->lowpass_cutoff_);
  this->nbvi_calibrator_.configure_hampel(this->hampel_enabled_, this->hampel_window_, this->hampel_threshold_);
  this->nbvi_calibrator_.set_buffer_size(this->segmentation_window_size_ * CALIBRATION_NUM_WINDOWS);

  this->csi_manager_.init(
    this->detector_,
    this->selected_subcarriers_,
    this->publish_interval_,
    this->gain_lock_mode_
  );
  this->csi_manager_.set_evaluation_interval(this->evaluation_interval_);
  this->csi_manager_.set_motion_on_hits(this->motion_on_hits_);
  this->csi_manager_.set_motion_off_hits(this->motion_off_hits_);
  this->csi_manager_.set_game_mode_callback([](float, float) {});

  esp_err_t handlers_err = this->wifi_lifecycle_.register_handlers(
      [this]() { this->on_wifi_connected_(); },
      [this]() { this->on_wifi_disconnected_(); }
  );
  if (handlers_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi handlers: %s. ESPectre setup aborted.",
             esp_err_to_name(handlers_err));
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "ESPectre initialized successfully");
  ESP_LOGD(TAG, "[resources] Free heap: %lu bytes, largest block: %lu bytes",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

ESpectreComponent::~ESpectreComponent() {}

void ESpectreComponent::on_wifi_connected_() {
  this->motion_state_ = MotionState::IDLE;
  this->csi_manager_.set_motion_state_callback([this](MotionState state) {
    this->motion_state_ = state;
    if (!this->ready_to_publish_) return;
    this->sensor_publisher_.publish_motion_binary(state);
  });

  if (!this->csi_manager_.is_enabled()) {
    ESP_ERROR_CHECK(this->csi_manager_.enable(
      [this](MotionState state, uint32_t packets_received) {
        this->motion_state_ = state;
        if (!this->ready_to_publish_) return;

        if (!this->threshold_republished_ && this->threshold_number_ != nullptr) {
          auto *threshold_num = static_cast<ESpectreThresholdNumber *>(this->threshold_number_);
          threshold_num->republish_state();
          this->threshold_republished_ = true;
        }

        this->sensor_publisher_.log_status(TAG, this->detector_, state, packets_received);
        this->sensor_publisher_.publish_movement_metric(this->detector_);
      }
    ));
  }

  this->csi_manager_.set_gain_lock_callback([this]() {
    auto& gc = this->csi_manager_.get_gain_controller();
    bool need_cv = gc.needs_cv_normalization();
    this->detector_->set_cv_normalization(need_cv);
    this->nbvi_calibrator_.set_cv_normalization(need_cv);
    this->start_calibration_();
  });

  this->ready_to_publish_ = true;
  this->threshold_republished_ = false;
}

void ESpectreComponent::on_wifi_disconnected_() {
  this->csi_manager_.disable();
  this->motion_state_ = MotionState::IDLE;
  this->ready_to_publish_ = false;
}

void ESpectreComponent::loop() {}

void ESpectreComponent::set_threshold_runtime(float threshold) {
  this->segmentation_threshold_ = threshold;
  this->csi_manager_.set_threshold(threshold);
  if (this->threshold_number_ != nullptr) {
    this->threshold_number_->publish_state(threshold);
  }
  ESP_LOGD(TAG, "Threshold updated to %.2f", threshold);
}

void ESpectreComponent::start_calibration_() {
  if (this->calibrate_switch_ != nullptr) {
    static_cast<ESpectreCalibrateSwitch *>(this->calibrate_switch_)->set_calibrating(true);
  }

  auto calibration_callback = [this](const uint8_t* band, uint8_t size,
                                     const std::vector<float>& cal_values, bool success) {
    if (success && !this->user_specified_subcarriers_) {
      memcpy(this->selected_subcarriers_, band, size);
      this->csi_manager_.update_subcarrier_selection(band);
    }

    if (band != nullptr && !cal_values.empty()) {
      float adaptive_threshold;
      uint8_t percentile;
      calculate_adaptive_threshold(cal_values, this->threshold_mode_, adaptive_threshold, percentile);
      this->best_pxx_ = adaptive_threshold;

      if (this->threshold_mode_ != ThresholdMode::MANUAL) {
        this->set_threshold_runtime(adaptive_threshold);
        ESP_LOGD(TAG, "Adaptive threshold: %.4f (P%d)", adaptive_threshold, percentile);
      }

      this->csi_manager_.clear_detector_buffer();
      this->sensor_publisher_.reset_rate_counter();
    }

    if (this->calibrate_switch_ != nullptr) {
      static_cast<ESpectreCalibrateSwitch *>(this->calibrate_switch_)->set_calibrating(false);
    }

    ESP_LOGD(TAG, "Calibration %s", success ? "completed successfully" : "failed");
  };

  esp_err_t cal_start_err = this->nbvi_calibrator_.start_calibration(
    this->selected_subcarriers_,
    12,
    calibration_callback
  );
  if (cal_start_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start calibration: %s", esp_err_to_name(cal_start_err));
    if (this->calibrate_switch_ != nullptr) {
      static_cast<ESpectreCalibrateSwitch *>(this->calibrate_switch_)->set_calibrating(false);
    }
  }
}

void ESpectreComponent::trigger_recalibration() {
  if (this->nbvi_calibrator_.is_calibrating()) {
    ESP_LOGW(TAG, "Calibration already in progress");
    return;
  }
  if (!this->csi_manager_.is_gain_locked()) {
    ESP_LOGW(TAG, "Cannot recalibrate: gain not yet locked");
    return;
  }
  ESP_LOGI(TAG, "Manual recalibration triggered");
  this->start_calibration_();
}

void ESpectreComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "");
  ESP_LOGCONFIG(TAG, "  _____ ____  ____           __            ");
  ESP_LOGCONFIG(TAG, " | ____/ ___||  _ \\ ___  ___| |_ _ __ ___ ");
  ESP_LOGCONFIG(TAG, " |  _| \\___ \\| |_) / _ \\/ __| __| '__/ _ \\");
  ESP_LOGCONFIG(TAG, " | |___ ___) |  __/  __/ (__| |_| | |  __/");
  ESP_LOGCONFIG(TAG, " |_____|____/|_|   \\___|\\___|\\__|_|  \\___|");
  ESP_LOGCONFIG(TAG, "");
  ESP_LOGCONFIG(TAG, "      Wi-Fi CSI Motion Detection System");
  ESP_LOGCONFIG(TAG, "");
  const char* thr_mode_str = (this->threshold_mode_ == ThresholdMode::MANUAL) ? "Manual" :
                             (this->threshold_mode_ == ThresholdMode::MIN) ? "Min (P100)" : "Auto (P95x1.1)";
  ESP_LOGCONFIG(TAG, " MOTION DETECTION");
  ESP_LOGCONFIG(TAG, " ├─ Detector ........... MVS");
  ESP_LOGCONFIG(TAG, " ├─ Threshold .......... %.2f (%s)", this->segmentation_threshold_, thr_mode_str);
  ESP_LOGCONFIG(TAG, " ├─ Window ............. %d pkts", this->segmentation_window_size_);
  ESP_LOGCONFIG(TAG, " └─ Baseline Pxx ....... %.4f", this->best_pxx_);
  ESP_LOGCONFIG(TAG, "");
  ESP_LOGCONFIG(TAG, " SUBCARRIERS [%02d,%02d,%02d,%02d,%02d,%02d,%02d,%02d,%02d,%02d,%02d,%02d]",
                this->selected_subcarriers_[0], this->selected_subcarriers_[1],
                this->selected_subcarriers_[2], this->selected_subcarriers_[3],
                this->selected_subcarriers_[4], this->selected_subcarriers_[5],
                this->selected_subcarriers_[6], this->selected_subcarriers_[7],
                this->selected_subcarriers_[8], this->selected_subcarriers_[9],
                this->selected_subcarriers_[10], this->selected_subcarriers_[11]);
  ESP_LOGCONFIG(TAG, " └─ Source ............. %s", this->user_specified_subcarriers_ ? "YAML" : "NBVI");
  ESP_LOGCONFIG(TAG, "");
  ESP_LOGCONFIG(TAG, " PUBLISH INTERVAL  %u pkts", this->publish_interval_);
  ESP_LOGCONFIG(TAG, " EVALUATION        interval=%u hits=%u/%u",
                this->evaluation_interval_, this->motion_on_hits_, this->motion_off_hits_);
  ESP_LOGCONFIG(TAG, " LOW-PASS          %s", this->lowpass_enabled_ ? "[ENABLED]" : "[DISABLED]");
  ESP_LOGCONFIG(TAG, " HAMPEL            %s", this->hampel_enabled_ ? "[ENABLED]" : "[DISABLED]");
  ESP_LOGCONFIG(TAG, "");
}

}  // namespace espectre
}  // namespace esphome
