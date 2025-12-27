#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <bluefruit.h>
#include <Wire.h>
#include <MiniShell.h>

#define I2C_SLAVE_ADDR 0x42

static MiniShell shell(&Serial);
static size_t i2c_rsp_len = 0;
static uint8_t i2c_rsp_buf[256];
static uint32_t lower = 0;
static uint32_t upper = 0;

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
    Bluefruit.setName("draaiwerk-ontvanger");
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
