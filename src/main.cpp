

#include <Particle.h>
#include <cmath>
#include "Air_Quality_Sensor.h"

SYSTEM_THREAD(ENABLED);
// We're creating a BLE beacon that need not connect to the Particle Cloud
SYSTEM_MODE(MANUAL);

// Pin for the gas sensor
const uint8_t AQS_PIN = (A2);

// Main gas sensor
AirQualitySensor aq_sensor(AQS_PIN);

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
	// Advertise every 250 milliseconds (in units of 0.625 millisecond).
	BLE.setAdvertisingInterval(400);

	// Continuously advertise
	BLE.advertise(&ble_ad_data);
  Log.info("adv: %lu", report_value);

}

static void configure_advertising() {
  BLE.on();
  update_adv_data();
}

static void poll_aq_sensor() {
  // ask the sensor to update
  int aq_level = aq_sensor.slope();
  if (aq_level >= 0) {
    // these values generally fall into the range 0..1024
    int32_t raw_aq = aq_sensor.getValue();
    ewma_value += ((float)raw_aq - ewma_value)*0.01; // EWMA using alpha of 0.01
    report_value = (uint32_t)ceilf(ewma_value);
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
  aq_sensor.init();
  configure_advertising();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  // read the argon battery voltage (https://docs.particle.io/cards/firmware/battery-voltage/battery-voltage/)
  // custom_data_value = ((double)analogRead(BATT)) * 0.0011224;

  poll_aq_sensor();
  update_adv_data();

  // sleep for a while between polling sensor
  sleep_control(15000);
}

