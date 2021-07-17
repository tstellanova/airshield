

#include <Particle.h>

SYSTEM_THREAD(ENABLED);
// We're creating a BLE peripheral that need not connect to the Particle Cloud
SYSTEM_MODE(MANUAL);

// SerialLogHandler logHandler(LOG_LEVEL_INFO);
SerialLogHandler logHandler(115200, LOG_LEVEL_INFO,
    {
      {"app", LOG_LEVEL_INFO},
      {"gsm0710muxer", LOG_LEVEL_WARN},
    });

static SystemSleepConfiguration sleep_cfg = {};
static uint8_t custom_adv_data[BLE_MAX_ADV_DATA_LEN] = {};
static double custom_data_value = 0;

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
	memcpy(&custom_adv_data[offset], &custom_data_value, sizeof(custom_data_value));
	offset += sizeof(custom_data_value);

	BleAdvertisingData ble_ad_data;
	ble_ad_data.appendCustomData(custom_adv_data, offset);

  //clear current?
  // BLE.stopAdvertising();
	// Advertise every 250 milliseconds (in units of 0.625 millisecond).
	BLE.setAdvertisingInterval(400);

	// Continuously advertise
	BLE.advertise(&ble_ad_data);
  Log.info("adv: %f", custom_data_value);

}

static void configure_advertising() {
  //enable BLE
  BLE.on();
  update_adv_data();
}



// control how long we sleep based on data collection and publication config
static void sleep_control(uint32_t sleep_ms) {
  sleep_cfg.mode(SystemSleepMode::ULTRA_LOW_POWER)
  // keep BLE active and advertising
    .ble()
	// Wake on battery fuel gauge event, eg battery unplugged 
    // .gpio(LOW_BAT_UC, FALLING) 
    .duration(sleep_ms); //ms
  
  uint32_t sleep_start = millis();
  Log.info("sleep %lu ms", sleep_ms);
  SystemSleepResult sleep_res = System.sleep(sleep_cfg);
  SystemSleepWakeupReason wake_reason = sleep_res.wakeupReason();
  uint32_t sleep_actual = millis() - sleep_start;
  // allow some time for usb serial to wake from sleep
  Serial.begin();
  delay(3000);
  Log.info("sleep_actual: %lu", sleep_actual);

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
}

// setup() runs once, when the device is first turned on.
void setup() {
  Serial.begin();
  delay(3000); //wait for serial usb to init, if connected
  Log.info("=== begin ===");
  configure_advertising();

}

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  // read the argon battery voltage (https://docs.particle.io/cards/firmware/battery-voltage/battery-voltage/)
  custom_data_value = ((double)analogRead(BATT)) * 0.0011224;
  update_adv_data();

  sleep_control(15000);
}

