#include <Arduino.h>
#include <bluefruit.h>

#include <RotaryEncoder.h>
#include <FRAM.h>
#include <MiniShell.h>

#define PIN_A     PIN_006
#define PIN_B     PIN_008
#define PIN_BATT  PIN_004

// #define HAVE_FRAM

typedef struct {
    uint32_t sequence;
    uint32_t count;
} nvdata_t;

static MiniShell shell(&Serial);
static FRAM fram;
static volatile int32_t counter = 0x12345678;
static nvdata_t nvdata;

static void update_advertisement(uint32_t sequence, uint32_t count)
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
    data[index++] = (count >> 24) & 0xFF;
    data[index++] = (count >> 16) & 0xFF;
    data[index++] = (count >> 8) & 0xFF;
    data[index++] = (count >> 0) & 0xFF;

    // battery voltage (mV)
    uint16_t vbat = analogRead(PIN_BATT) * 1.4 * 3600 / 1024;
    data[index++] = (vbat >> 8) & 0xFF;
    data[index++] = (vbat >> 0) & 0xFF;

    // placeholder for message authentication code
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;

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
    nvdata.count += step;
#ifdef HAVE_FRAM
    fram.writeObject(0, nvdata);
#endif
    update_advertisement(nvdata.sequence++, nvdata.count);
}

static int do_clear(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "yes") == 0) {
            printf("Clearing non-volatile data\n");
            memset(&nvdata, 0, sizeof(nvdata));
            fram.writeObject(0, nvdata);
            return 0;
        }
    }
    printf("Add 'yes' if you are sure to clear the non-volatile data\n");
    return -1;
}

static int do_report(int argc, char *argv[])
{
    uint32_t seqnr = nvdata.sequence++;
    uint32_t count = (argc > 1) ? atoi(argv[1]) : nvdata.count;
    printf("Sending report: seqnr=%u, count=%u\n", seqnr, count);
    update_advertisement(seqnr, count);
    return 0;
}

static cmd_t commands[] = {
    { "clear", do_clear, "<yes> Clear non-volatile data" },
    { "report", do_report, "[count] Send a report" },
    { NULL, NULL, NULL }
};

void setup(void)
{
    Serial.begin(115200);

    // bluetooth init
    Bluefruit.begin();
    Bluefruit.setTxPower(0);
    Bluefruit.autoConnLed(false);
    Bluefruit.setName("TelWerk");
    Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED);
    Bluefruit.Advertising.setIntervalMS(1000, 10000);

    // rotary encoder init
    RotaryEncoder.begin(PIN_A, PIN_B);
    RotaryEncoder.setReporter(1);
    RotaryEncoder.setDebounce(true);
    RotaryEncoder.setSampler(QDEC_SAMPLEPER_SAMPLEPER_1024us);
    RotaryEncoder.setCallback(encoder_callback);
    RotaryEncoder.start();

    // FRAM init
#ifdef HAVE_FRAM
    if (fram.begin() == 0) {
        fram.readObject(0, nvdata);
    }
#endif

    // send initial advertisement
    update_advertisement(nvdata.sequence, nvdata.count);
}

void loop(void)
{
    shell.process(">", commands);
}
