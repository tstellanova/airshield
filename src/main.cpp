/*
 * Copyright (c) 2021 Particle Industries, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Particle.h>
#include <cmath>

#include "SparkFunBME280.h"
#include "SparkFunCCS811.h" 


SYSTEM_THREAD(ENABLED);
// We're creating a BLE beacon that need not connect to the Particle Cloud
SYSTEM_MODE(MANUAL);

const uint8_t CCS811_ADDR = 0x5B ;//Default I2C Address

// Main gas sensor
CCS811 voc_sensor(CCS811_ADDR);

// Main Pressure, Humidity, Temperature, (PHT) Sensor
BME280 pht_sensor;


SerialLogHandler logHandler(115200, LOG_LEVEL_INFO,
    {
      {"app", LOG_LEVEL_INFO},
      {"gsm0710muxer", LOG_LEVEL_WARN},
    });

static SystemSleepConfiguration sleep_cfg = {};
static uint8_t custom_adv_data[BLE_MAX_ADV_DATA_LEN] = {};
// The value to report in advertisement
static uint32_t report_value = 0;
static float ewma_value = 0;
static uint32_t ewma_update_count = 0;

/// Change advertising data to match latest values
static void update_adv_data() {
  Log.trace("update_adv_data start...");
	size_t offset = 0;

	// Manufacturer-specific data
	// 16-bit: Company ID (0xffff)
	// Byte: Internal packet identifier (0x55)
	// 32-bit: Color code

	// Company ID (0xffff internal use/testing)
	custom_adv_data[offset++] = 0xff;
	custom_adv_data[offset++] = 0xff;

	// Internal packet type. This is arbitrary, but provides an extra
	// check to make sure the data is my data, since we use the 0xffff company
	// code.
	custom_adv_data[offset++] = 0x55;

	// Our specific data
	memcpy(&custom_adv_data[offset], &report_value, sizeof(report_value));
	offset += sizeof(report_value);

	BleAdvertisingData ble_ad_data;
	ble_ad_data.appendCustomData(custom_adv_data, offset);

  //clear current?
  // BLE.stopAdvertising();
	// Advertise every 500 milliseconds (in units of 0.625 millisecond).
	BLE.setAdvertisingInterval(800);

	// Continuously advertise
	BLE.advertise(&ble_ad_data);
  Log.info("adv: %lu", report_value);

}

static void configure_advertising() {
  BLE.on();
  update_adv_data();
}


// Read all of the sensors
static void poll_sensors() {
  
  float pressure = pht_sensor.readFloatPressure();
  float temperature = pht_sensor.readTempC();
  float humid  = pht_sensor.readFloatHumidity();
  Log.info("P: %f H: %f T: %f", pressure, humid, temperature);
  if (humid >= 0) {
    // update voc sensor with environmental data frequently
    voc_sensor.setEnvironmentalData(humid, temperature);
    if (voc_sensor.dataAvailable()) {
      auto read_res = voc_sensor.readAlgorithmResults();
      if(CCS811Core::SENSOR_SUCCESS == read_res){
        Log.info("CO2 ppm: %u",voc_sensor.getCO2());
        Log.info("VOC ppm: %u", voc_sensor.getTVOC());
        // we only report one gas value currently 
        float cur_val = (float)voc_sensor.getCO2();
        
        if (0 == ewma_update_count) {
          ewma_value = cur_val;
        }
        else {
          // calculate exponential weighted moving average (EWMA) using alpha of 0.01
          ewma_value += (cur_val - ewma_value)*0.01;
        }
        ewma_update_count += 1;
        report_value = (uint32_t)ceilf(ewma_value);
      }
      else {
        Log.warn("read_res: %u", read_res);
      }

    }
  }

}

// control how long we sleep based on data collection and publication config
static void sleep_control(uint32_t sleep_ms) {
  sleep_cfg.mode(SystemSleepMode::ULTRA_LOW_POWER)
    // keep BLE active and advertising during sleep
    .ble()
    .duration(sleep_ms); 
  
  uint32_t sleep_start = millis();
  Log.info("sleep %lu ms", sleep_ms);
  SystemSleepResult sleep_res = System.sleep(sleep_cfg);
  uint32_t sleep_actual = millis() - sleep_start;
  Serial.begin();
  SystemSleepWakeupReason wake_reason = sleep_res.wakeupReason();
  // allow some time for usb serial to wake from sleep
  delay(1000);

  switch (wake_reason) {
	case SystemSleepWakeupReason::BY_RTC:
		Log.info("wakeup on RTC");
		break;
    case SystemSleepWakeupReason::BY_GPIO:
      Log.info("GPIO wakeup pin: %u", sleep_res.wakeupPin());
      break;
	case SystemSleepWakeupReason::BY_NETWORK:
		Log.info("Network wakeup");
		break;

    case SystemSleepWakeupReason::BY_ADC: 
    default: {
      Log.info("wakeup: %u", (uint16_t)wake_reason);
    }
    break;
  }
  Log.info("sleep_actual: %lu", sleep_actual);

}

// setup() runs once, when the device is first turned on.
void setup() {
  Serial.begin();
  delay(3000); //wait for serial usb to init, if connected
  Log.info("=== begin ===");

  Wire.begin();

  delay(3000);

  // start VOC sensor
  auto voc_rc = voc_sensor.begin();
  if ( CCS811Core::SENSOR_SUCCESS != voc_rc) {
    Log.error("ccs811 begin failed: %u",voc_rc);
  }

  // configure and start PHT sensor
  pht_sensor.settings.commInterface = I2C_MODE;
  pht_sensor.settings.I2CAddress = 0x77;
  pht_sensor.settings.runMode = 3; //normal mode
  pht_sensor.settings.tStandby = 0; //ms
  pht_sensor.settings.filter = 0; // no FIR filter
  pht_sensor.settings.tempOverSample = 2; // times oversampling
  pht_sensor.settings.pressOverSample = 2;
  pht_sensor.settings.humidOverSample = 2;

  uint8_t rc = pht_sensor.begin();
  Log.warn("pht begin result: %u", rc);
  
  configure_advertising();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  // read the argon battery voltage (https://docs.particle.io/cards/firmware/battery-voltage/battery-voltage/)
  // custom_data_value = ((double)analogRead(BATT)) * 0.0011224;

  poll_sensors();
  update_adv_data();

  // sleep for a while between polling sensors
  sleep_control(5000);
}

