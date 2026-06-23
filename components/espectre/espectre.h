/*
 * ESPectre - Main Component
 *
 * WiFi CSI-based motion detection using Moving Variance Score (MVS).
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_event.h"

#include "utils.h"
#include "threshold.h"
#include "base_detector.h"
#include "mvs_detector.h"
#include "sensor_publisher.h"
#include "csi_manager.h"
#include "wifi_lifecycle.h"
#include "nbvi_calibrator.h"

namespace esphome {
namespace espectre {

static const char *const TAG = "espectre";

class ESpectreComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  ~ESpectreComponent();
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Setters for YAML configuration
  void set_segmentation_threshold(float threshold) {
    this->segmentation_threshold_ = threshold;
    this->threshold_mode_ = ThresholdMode::MANUAL;
  }
  void set_threshold_mode(const std::string &mode) {
    if (mode == "min") {
      this->threshold_mode_ = ThresholdMode::MIN;
    } else {
      this->threshold_mode_ = ThresholdMode::AUTO;
    }
  }
  void set_segmentation_window_size(uint16_t size) { this->segmentation_window_size_ = size; }
  void set_gain_lock_mode(const std::string &mode) {
    if (mode == "enabled") {
      this->gain_lock_mode_ = GainLockMode::ENABLED;
    } else if (mode == "disabled") {
      this->gain_lock_mode_ = GainLockMode::DISABLED;
    } else {
      this->gain_lock_mode_ = GainLockMode::AUTO;
    }
  }
  void set_publish_interval(uint32_t interval) { this->publish_interval_ = interval; }
  void set_evaluation_interval(uint32_t interval) { this->evaluation_interval_ = interval; }
  void set_motion_on_hits(uint8_t hits) { this->motion_on_hits_ = hits; }
  void set_motion_off_hits(uint8_t hits) { this->motion_off_hits_ = hits; }
  void set_lowpass_enabled(bool enabled) { this->lowpass_enabled_ = enabled; }
  void set_lowpass_cutoff(float cutoff) { this->lowpass_cutoff_ = cutoff; }
  void set_hampel_enabled(bool enabled) { this->hampel_enabled_ = enabled; }
  void set_hampel_window(uint8_t window) { this->hampel_window_ = window; }
  void set_hampel_threshold(float threshold) { this->hampel_threshold_ = threshold; }

  void set_selected_subcarriers(const std::vector<uint8_t> &subcarriers) {
    size_t count = std::min(subcarriers.size(), (size_t)12);
    for (size_t i = 0; i < count; i++) {
      this->selected_subcarriers_[i] = subcarriers[i];
    }
    this->user_specified_subcarriers_ = true;
  }

  void set_movement_sensor(sensor::Sensor *sensor) { this->sensor_publisher_.set_movement_sensor(sensor); }
  void set_motion_binary_sensor(binary_sensor::BinarySensor *sensor) { this->sensor_publisher_.set_motion_binary_sensor(sensor); }
  void set_threshold_number(number::Number *num) { this->threshold_number_ = num; }

  void set_threshold_runtime(float threshold);
  float get_threshold() const { return this->segmentation_threshold_; }

  void trigger_recalibration();
  bool is_calibrating() const { return this->nbvi_calibrator_.is_calibrating(); }

  void set_calibrate_switch(switch_::Switch *sw) { this->calibrate_switch_ = sw; }

 protected:
  void start_calibration_();
  void on_wifi_connected_();
  void on_wifi_disconnected_();

  BaseDetector* detector_{nullptr};
  MVSDetector mvs_detector_;
  MotionState motion_state_{MotionState::IDLE};

  float segmentation_threshold_{1.0f};
  uint16_t segmentation_window_size_{100};
  GainLockMode gain_lock_mode_{GainLockMode::AUTO};
  uint32_t publish_interval_{100};
  uint32_t evaluation_interval_{25};
  uint8_t motion_on_hits_{3};
  uint8_t motion_off_hits_{3};
  bool lowpass_enabled_{false};
  float lowpass_cutoff_{11.0f};
  bool hampel_enabled_{true};
  uint8_t hampel_window_{7};
  float hampel_threshold_{5.0f};
  uint8_t selected_subcarriers_[12] = {
    DEFAULT_SUBCARRIERS[0], DEFAULT_SUBCARRIERS[1], DEFAULT_SUBCARRIERS[2], DEFAULT_SUBCARRIERS[3],
    DEFAULT_SUBCARRIERS[4], DEFAULT_SUBCARRIERS[5], DEFAULT_SUBCARRIERS[6], DEFAULT_SUBCARRIERS[7],
    DEFAULT_SUBCARRIERS[8], DEFAULT_SUBCARRIERS[9], DEFAULT_SUBCARRIERS[10], DEFAULT_SUBCARRIERS[11]
  };

  bool user_specified_subcarriers_{false};
  ThresholdMode threshold_mode_{ThresholdMode::AUTO};

  SensorPublisher sensor_publisher_;
  CSIManager csi_manager_;
  WiFiLifecycleManager wifi_lifecycle_;
  NBVICalibrator nbvi_calibrator_;

  number::Number *threshold_number_{nullptr};
  switch_::Switch *calibrate_switch_{nullptr};

  float best_pxx_{0.0f};
  bool ready_to_publish_{false};
  bool threshold_republished_{false};
};

}  // namespace espectre
}  // namespace esphome
