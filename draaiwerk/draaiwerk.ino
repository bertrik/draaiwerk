#include <Arduino.h>
#include <bluefruit.h>

#include <RotaryEncoder.h>

#define PIN_A     PIN_006
#define PIN_B     PIN_008
#define PIN_BATT  PIN_004

static volatile int32_t counter = 0x12345678;
static uint32_t _sequence = 0;

static void update_advertisement(uint32_t sequence, uint32_t value)
{
    // prepare data
    uint8_t data[16];
    int index = 0;

    // company identifier
    data[index++] = 0xFF;
    data[index++] = 0xFF;

    // sequence
    data[index++] = (sequence >> 24) & 0xFF;
    data[index++] = (sequence >> 16) & 0xFF;
    data[index++] = (sequence >> 8) & 0xFF;
    data[index++] = (sequence >> 0) & 0xFF;

    // counter value (big endian)
    data[index++] = (value >> 24) & 0xFF;
    data[index++] = (value >> 16) & 0xFF;
    data[index++] = (value >> 8) & 0xFF;
    data[index++] = (value >> 0) & 0xFF;

    // battery voltage (mV)
    uint16_t vbat = analogRead(PIN_BATT) * 1.4 * 3600 / 1024;
    data[index++] = (vbat >> 8) & 0xFF;
    data[index++] = (vbat >> 0) & 0xFF;

    // update advertising data
    Bluefruit.Advertising.stop();
    Bluefruit.Advertising.clearData();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addManufacturerData(data, index);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.start(0);
}

static void encoder_callback(int step)
{
    counter += step;
    update_advertisement(_sequence++, counter);
}

void setup(void)
{
    Serial.begin(115200);

    // bluetooth init
    Bluefruit.begin();
    Bluefruit.setTxPower(0);
    Bluefruit.autoConnLed(false);
    Bluefruit.setName("DraaiWerk");
    Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED);
    Bluefruit.Advertising.setIntervalMS(1000, 10000);

    // rotary encoder init
    RotaryEncoder.begin(PIN_A, PIN_B);
    RotaryEncoder.setReporter(1);
    RotaryEncoder.setDebounce(true);
    RotaryEncoder.setSampler(QDEC_SAMPLEPER_SAMPLEPER_1024us);
    RotaryEncoder.setCallback(encoder_callback);
    RotaryEncoder.start();
}

void loop(void)
{
    static unsigned long last_second = -1;

    unsigned long seconds = millis() / 1000;
    if (seconds != last_second) {
        last_second = seconds;
        if (seconds % 10 == 0) {
            counter++;
            printf("Counter: 0x%08X\n", (unsigned int) counter);
            update_advertisement(_sequence++, counter);
        }
    }

}
