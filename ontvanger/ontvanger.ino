#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <bluefruit.h>
#include <Wire.h>
#include <MiniShell.h>

#define I2C_SLAVE_ADDR 0x42

static MiniShell shell(&Serial);
static int i2c_index = 0;
static uint8_t i2c_cmd_data[256];
static size_t i2c_cmd_len = 0;
static uint8_t i2c_rsp_data[256];
static size_t i2c_rsp_len = 0;

static void handle_manufacturer_data(uint8_t *data, uint8_t len)
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

static void i2c_on_master_write(int num)
{
    if (num < 1) {
        return;
    }

    i2c_index = Wire.read();
    if (num > 1) {
        i2c_cmd_len = 0;
        while (Wire.available()) {
            uint8_t b = Wire.read();
            if (i2c_cmd_len < sizeof(i2c_cmd_data)) {
                i2c_cmd_data[i2c_cmd_len++] = b;
            }
        }
        // TODO verify CRC
    }
}

static void i2c_on_master_read(void)
{
    Wire.write(i2c_rsp_len);
    if (i2c_rsp_len > 0) {
        Wire.write(i2c_rsp_data, i2c_rsp_len);
    }
    // TODO append CRC
    Wire.write(0);
    Wire.write(0);
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

static cmd_t commands[] = {
    { "start", do_start, "Start scanning" },
    { "stop", do_stop, "Stop scanning" },
    { NULL, NULL, NULL }
};

void setup(void)
{
    Serial.begin(115200);

    // bluetooth scan setup
    Bluefruit.begin();
    Bluefruit.setTxPower(0);
    Bluefruit.setName("nRF52840-Scanner");
    Bluefruit.Scanner.setRxCallback(scan_callback);
    Bluefruit.Scanner.setIntervalMS(160, 80);
    Bluefruit.Scanner.useActiveScan(false);     // passive scan

    // i2c slave setup
    Wire.begin(I2C_SLAVE_ADDR); // Start as I2C slave
    Wire.onReceive(i2c_on_master_write);
    Wire.onRequest(i2c_on_master_read);
    Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
}

void loop(void)
{
    shell.process("> ", commands);
}
