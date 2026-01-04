#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <bluefruit.h>
#include <Wire.h>

#include <FRAM.h>
#include <MiniShell.h>

#define PIN_POWER           PIN_013
#define PIN_I2C_SLAVE_SDA   PIN_104
#define PIN_I2C_SLAVE_SCL   PIN_106
#define PIN_I2C_MASTER_SDA  PIN_010
#define PIN_I2C_MASTER_SCL  PIN_009

#define I2C_SLAVE_ADDR 0x42

static MiniShell shell(&Serial);
static FRAM fram(&Wire1);
static size_t i2c_rsp_len = 0;
static uint8_t i2c_rsp_buf[256];
static uint32_t lower = 0;
static uint32_t upper = 0;
static bool have_fram = false;

static void handle_manufacturer_data(const uint8_t *data, uint8_t len)
{
    printf("Manufacturer Data (%2d bytes):", len);
    for (uint8_t i = 0; i < len; i++) {
        printf(" %02X", data[i]);
    }
    printf("\n");
}

static void scan_callback(ble_gap_evt_adv_report_t *report)
{
    uint16_t report_len = report->data.len;
    uint8_t *report_data = report->data.p_data;
#if 0
    printf("Advertisement data (%2d bytes)\n", report_len);
    for (int i = 0; i < report_len; i++) {
        printf(" %02X", report_data[i]);
    }
    printf("\n");
#endif
    int i = 0;
    while (i < report_len) {
        uint8_t len = report_data[i++];
        if ((i + len) > report_len || len == 0) {
            break;
        }
        uint8_t ad_type = report_data[i];
        uint8_t *ad_data = &report_data[i + 1];
        uint8_t ad_len = len - 1;
        i += len;

        if (ad_type == BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA) {
            handle_manufacturer_data(ad_data, ad_len);
        }
    }
    Bluefruit.Scanner.resume();
}

static size_t handle_smbus_command(uint8_t cmd, uint8_t *rsp_buf)
{
    size_t idx = 0;

    printf("Got SMBUS cmd 0x%02X", cmd);

    switch (cmd) {
    case 0x10:                 // "perform"
        // nothing to do
        break;
    case 0x11:                 // "read", prepare response buffer
        rsp_buf[idx++] = (lower >> 24) & 0xFF;
        rsp_buf[idx++] = (lower >> 16) & 0xFF;
        rsp_buf[idx++] = (lower >> 8) & 0xFF;
        rsp_buf[idx++] = (lower >> 0) & 0xFF;
        rsp_buf[idx++] = (upper >> 24) & 0xFF;
        rsp_buf[idx++] = (upper >> 16) & 0xFF;
        rsp_buf[idx++] = (upper >> 8) & 0xFF;
        rsp_buf[idx++] = (upper >> 0) & 0xFF;
        break;
    default:
        break;
    }
    return idx;
}

static void i2c_on_master_write(int num)
{
    // we understand only one kind of i2c message, an smbus byte write
    if (num == 1) {
        uint8_t cmd = Wire.read();
        i2c_rsp_len = handle_smbus_command(cmd, i2c_rsp_buf);
    }
}

static void i2c_on_master_read(void)
{
    // just send prepared i2c buffer
    Wire.write(i2c_rsp_len);
    if (i2c_rsp_len > 0) {
        Wire.write(i2c_rsp_buf, i2c_rsp_len);
    }
}

static int do_start(int argc, char *argv[])
{
    Bluefruit.Scanner.start(0);
    printf("Started scanning");
    return 0;
}

static int do_stop(int argc, char *argv[])
{
    Bluefruit.Scanner.stop();
    printf("Stopped scanning\n");
    return 0;
}

// returns 0 if device detected
static int i2c_detect(TwoWire *wire, int address)
{
    wire->beginTransmission(address);
    return wire->endTransmission();
}

static int do_i2c_scan(int argc, char *argv[])
{
    int result;
    for (int i = 0; i < 128; i++) {
        char c;
        if (i < 0x08) {
            c = 'S';
        } else {
            result = i2c_detect(&Wire1, i);
            switch (result) {
                case 0: c = '!'; break;
                case 2:
                case 3: c = 'N'; break;
                case 4: c = 'E'; break;
                default:
                    c = '?';
                    break;
            }
        }
        if ((i % 16) == 0) {
            printf("\n%02X: ", i);
        }
        printf("%c", c);
    }
    printf("\n");
    return 0;
}

static int do_fram(int argc, char *argv[])
{
    if (argc < 2) {
        return -1;
    }

    char *cmd = argv[1];
    if (strcmp(cmd, "begin") == 0) {
        fram.begin();
    }
    if ((argc > 2) && strcmp(cmd, "r") == 0) {
        int addr = atoi(argv[2]);
        uint8_t b = fram.read8(addr);
        printf("READ fram[0x%04X]=0x%02X\n", addr, b);
    }
    if ((argc > 3) && strcmp(cmd, "w") == 0) {
        int addr = atoi(argv[2]);
        uint8_t b = strtoul(argv[3], NULL, 0);
        printf("WRITE fram[0x%04X]=0x%02X\n", addr, b);
        fram.write8(addr, b);
    }
    return 0;
}

static cmd_t commands[] = {
    { "start", do_start, "Start scanning" },
    { "stop", do_stop, "Stop scanning" },
    { "i2cscan", do_i2c_scan, "Perform an I2C scan" },
    { "fram", do_fram, "<begin|r|w> [addr] [data]" },
    { NULL, NULL, NULL }
};

void setup(void)
{
    Serial.begin(115200);

    // power pin setup
    pinMode(PIN_POWER, OUTPUT);
    digitalWrite(PIN_POWER, 1);

    // bluetooth scan setup
    Bluefruit.begin();
    Bluefruit.setTxPower(0);
    Bluefruit.setName("draaiwerk-ontvanger");
    Bluefruit.Scanner.setRxCallback(scan_callback);
    Bluefruit.Scanner.setIntervalMS(160, 80);
    Bluefruit.Scanner.useActiveScan(false);     // passive scan

    // i2c slave setup
    Wire.begin(I2C_SLAVE_ADDR); // Start as I2C slave
    Wire.setPins(PIN_I2C_SLAVE_SDA, PIN_I2C_SLAVE_SCL);
    Wire.onReceive(i2c_on_master_write);
    Wire.onRequest(i2c_on_master_read);

    // i2c master setup
    Wire1.begin();
    Wire1.setPins(PIN_I2C_MASTER_SDA, PIN_I2C_MASTER_SCL);

    // fram setup
    have_fram = (i2c_detect(&Wire1, 0x50) == 0);
    if (have_fram) {
        fram.begin();
    }
}

void loop(void)
{
    shell.process("> ", commands);
}
