#include "solax_x1.h"
#include "esphome/core/log.h"

namespace esphome {
namespace solax_x1 {

static const char *const TAG = "solax_x1";

// Hybrid.X1.X3-G3.ModbusTCP.RTU.V3.21-.English.pdf
// May be some modes doesn't apply to the X1 mini
static const std::string MODE_NAMES[10] = {
    "Wait",             // 0
    "Check",            // 1
    "Normal",           // 2
    "Fault",            // 3
    "Permanent Fault",  // 4

    "Update",     // 5
    "EPS check",  // 6
    "EPS",        // 7
    "Self Test",  // 8
    "Idle",       // 9
};

static const char *const ERRORS[32] = {
    "Tz Protection Fault",   // Byte 0.0
    "Mains Lost Fault",      // Byte 0.1
    "Grid Voltage Fault",    // Byte 0.2
    "Grid Frequency Fault",  // Byte 0.3
    "PLL Lost Fault",        // Byte 0.4
    "Bus Voltage Fault",     // Byte 0.5
    "Error Bit 06",          // Byte 0.6
    "Oscillator Fault",      // Byte 0.7

    "DCI OCP Fault",           // Byte 1.0
    "Residual Current Fault",  // Byte 1.1
    "PV Voltage Fault",        // Byte 1.2
    "Ac10Mins Voltage Fault",  // Byte 1.3
    "Isolation Fault",         // Byte 1.4
    "Over Temperature Fault",  // Byte 1.5
    "Ventilator Fault",        // Byte 1.6
    "Error Bit 15",            // Byte 1.7

    "SPI Communication Fault",        // Byte 2.0
    "SCI Communication Fault",        // Byte 2.1
    "Error Bit 18",                   // Byte 2.2
    "Input Configuration Fault",      // Byte 2.3
    "EEPROM Fault",                   // Byte 2.4
    "Relay Fault",                    // Byte 2.5
    "Sample Consistence Fault",       // Byte 2.6
    "Residual-Current Device Fault",  // Byte 2.7

    "Error Bit 24",        // Byte 3.0
    "Error Bit 25",        // Byte 3.1
    "Error Bit 26",        // Byte 3.2
    "Error Bit 27",        // Byte 3.3
    "Error Bit 28",        // Byte 3.4
    "DCI Device Fault",    // Byte 3.5
    "Other Device Fault",  // Byte 3.6
    "Error Bit 31",        // Byte 3.7
};

void SolaxX1::on_modbus_solax_info(const std::vector<uint8_t> &data) {
  if (data.size() != 58) {
    ESP_LOGW(TAG, "Invalid response size: %zu", data.size());
    return;
  }

  ESP_LOGW(TAG, "Info: %s", format_hex_pretty(&data.front(), data.size()).c_str());
  // @TODO: Output Solax_Info_t
  this->no_response_count_ = 0;
}

void SolaxX1::on_modbus_solax_data(const std::vector<uint8_t> &data) {
  if (data.size() != 52 && data.size() != 50 && data.size() != 56) {
    // Solax X1 mini status report:
    // AA.55.00.0A.01.00.11.82.34.00.1A.00.02.00.00.00.00.00.00.00.00.00.00.09.21.13.87.00.00.FF.FF.
    // 00.00.00.12.00.00.00.15.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.04.D6

    // Solax X1 mini g2 status report:
    // AA.55.00.0A.01.00.11.82.32.00.21.00.02.07.EC.00.00.00.1D.00.00.00.18.09.55.13.80.02.2B.FF.FF.
    // 00.00.5D.AF.00.00.10.50.00.02.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.07.A4

    // Solax X1 mini g3 status report:
    // ...........................00.17.00.00.03.DE.00.00.00.08.00.00.00.05.09.06.13.85.00.48.FF.FF.
    // 00.00.00.28.00.00.00.0C.00.02.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.01.00.8A.00.DE
    ESP_LOGW(TAG, "Invalid response size: %zu", data.size());
    ESP_LOGW(TAG, "Your device is probably not supported. Please create an issue here: "
                  "https://github.com/syssi/esphome-modbus-solax-x1/issues");
    ESP_LOGW(TAG, "Please provide the following status response data: %s",
             format_hex_pretty(&data.front(), data.size()).c_str());
    return;
  }

  auto solax_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto solax_get_32bit = [&](size_t i) -> uint32_t {
    return uint32_t((data[i] << 24) | (data[i + 1] << 16) | (data[i + 2] << 8) | data[i + 3]);
  };
  auto solax_get_error_bitmask = [&](size_t i) -> uint32_t {
    return uint32_t((data[i + 3] << 24) | (data[i + 2] << 16) | (data[i + 1] << 8) | data[i]);
  };

  float temperature = solax_get_16bit(0);

  uint16_t raw_energy_today = solax_get_16bit(2);
  float energy_today = raw_energy_today * 0.1f;

  uint16_t raw_dc1_voltage = solax_get_16bit(4);
  float dc1_voltage = raw_dc1_voltage * 0.1f;

  uint16_t raw_dc2_voltage = solax_get_16bit(6);
  float dc2_voltage = raw_dc2_voltage * 0.1f;

  uint16_t raw_dc1_current = solax_get_16bit(8);
  float dc1_current = raw_dc1_current * 0.1f;

  uint16_t raw_dc2_current = solax_get_16bit(10);
  float dc2_current = raw_dc2_current * 0.1f;

  uint16_t raw_ac_current = solax_get_16bit(12);
  float ac_current = raw_ac_current * 0.1f;

  uint16_t raw_ac_voltage = solax_get_16bit(14);
  float ac_voltage = raw_ac_voltage * 0.1f;

  uint16_t raw_ac_frequency = solax_get_16bit(16);
  float ac_frequency = raw_ac_frequency * 0.01f;

  uint16_t raw_ac_power = solax_get_16bit(18);
  float ac_power = raw_ac_power;

  // register 20 is not used

  uint32_t raw_energy_total = solax_get_32bit(22);
  // The inverter publishes a zero once per day on boot-up. This confuses the energy dashboard.
  if (raw_energy_total > 0) {
    this->publish_state_(this->energy_total_sensor_, (float) raw_energy_total * 0.1f);
  }

  uint32_t raw_runtime_total = solax_get_32bit(26);
  if (raw_runtime_total > 0) {
    this->publish_state_(this->runtime_total_sensor_, (float) raw_runtime_total);
  }

  uint8_t mode = (uint8_t) solax_get_16bit(30);

  // register 32: Grid voltage fault in 0.1V
  // register 34: Grid frequency fault in 0.01Hz
  // register 36: DC injection fault in 1mA
  // register 38: Temperature fault in °C
  // register 40: Pv1 voltage fault in 0.1V
  // register 42: Pv2 voltage fault in 0.1V
  // register 44: GFC fault

  uint32_t error_bits = solax_get_error_bitmask(46);

  this->publish_state_(this->temperature_sensor_, temperature);
  this->publish_state_(this->energy_today_sensor_, energy_today);
  this->publish_state_(this->dc1_voltage_sensor_, dc1_voltage);
  this->publish_state_(this->dc2_voltage_sensor_, dc2_voltage);
  this->publish_state_(this->dc1_current_sensor_, dc1_current);
  this->publish_state_(this->dc2_current_sensor_, dc2_current);
  this->publish_state_(this->ac_current_sensor_, ac_current);
  this->publish_state_(this->ac_voltage_sensor_, ac_voltage);
  this->publish_state_(this->ac_frequency_sensor_, ac_frequency);
  this->publish_state_(this->ac_power_sensor_, ac_power);
  this->publish_state_(this->mode_sensor_, mode);
  this->publish_state_(this->error_bits_sensor_, error_bits);
  this->publish_state_(this->errors_text_sensor_, this->error_bits_to_string_(error_bits));
  this->publish_state_(this->mode_name_text_sensor_, (mode <= 9) ? MODE_NAMES[mode] : "Unknown");

  this->no_response_count_ = 0;
}

void SolaxX1::publish_device_offline_() {
  this->publish_state_(this->mode_sensor_, -1);
  this->publish_state_(this->mode_name_text_sensor_, "Offline");

  this->publish_state_(this->temperature_sensor_, NAN);
  this->publish_state_(this->dc1_voltage_sensor_, 0);
  this->publish_state_(this->dc2_voltage_sensor_, 0);
  this->publish_state_(this->dc1_current_sensor_, 0);
  this->publish_state_(this->dc2_current_sensor_, 0);
  this->publish_state_(this->ac_current_sensor_, 0);
  this->publish_state_(this->ac_voltage_sensor_, NAN);
  this->publish_state_(this->ac_frequency_sensor_, NAN);
  this->publish_state_(this->ac_power_sensor_, 0);
}

void SolaxX1::update() {
  if (this->no_response_count_ >= REDISCOVERY_THRESHOLD) {
    this->publish_device_offline_();
    ESP_LOGD(TAG, "The device is or was offline. Broadcasting discovery for address configuration...");
    this->discover_devices();
    //    this->query_info(this->address_);
    // Try to query live data on next update again. The device doesn't
    // respond to the discovery broadcast if it's already configured.
    this->no_response_count_ = 0;
  } else {
    no_response_count_++;
    this->query_live_data(this->address_);
  }
}

void SolaxX1::publish_state_(sensor::Sensor *sensor, float value) {
  if (sensor == nullptr)
    return;

  sensor->publish_state(value);
}

void SolaxX1::publish_state_(text_sensor::TextSensor *text_sensor, const std::string &state) {
  if (text_sensor == nullptr)
    return;

  text_sensor->publish_state(state);
}

void SolaxX1::dump_config() {
  ESP_LOGCONFIG(TAG, "SolaxX1:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("", "Energy Today", this->energy_today_sensor_);
  LOG_SENSOR("", "DC1 Voltage", this->dc1_voltage_sensor_);
  LOG_SENSOR("", "DC2 Voltage", this->dc2_voltage_sensor_);
  LOG_SENSOR("", "DC1 Current", this->dc1_current_sensor_);
  LOG_SENSOR("", "DC2 Current", this->dc2_current_sensor_);
  LOG_SENSOR("", "AC Current", this->ac_current_sensor_);
  LOG_SENSOR("", "AC Voltage", this->ac_voltage_sensor_);
  LOG_SENSOR("", "AC Frequency", this->ac_frequency_sensor_);
  LOG_SENSOR("", "AC Power", this->ac_power_sensor_);
  LOG_SENSOR("", "Energy Total", this->energy_total_sensor_);
  LOG_SENSOR("", "Runtime Total", this->runtime_total_sensor_);
  LOG_SENSOR("", "Mode", this->mode_sensor_);
  LOG_SENSOR("", "Error Bits", this->error_bits_sensor_);
  LOG_TEXT_SENSOR("  ", "Mode Name", this->mode_name_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Errors", this->errors_text_sensor_);
}

std::string SolaxX1::error_bits_to_string_(const uint32_t mask) {
  bool first = true;
  std::string errors_list = "";

  if (mask) {
    for (int i = 0; i < 32; i++) {
      if (mask & (1 << i)) {
        if (first) {
          first = false;
        } else {
          errors_list.append(";");
        }
        errors_list.append(ERRORS[i]);
      }
    }
  }

  return errors_list;
}

}  // namespace solax_x1
}  // namespace esphome
