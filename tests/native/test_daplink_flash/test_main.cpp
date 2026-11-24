// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstring>
#include <string>

#include "DaplinkBridge.h"
#include "FakeDaplinkBridge.h"
#include "SteamiDaplinkFlash.h"
#include "Wire.h"

DaplinkBridge bridge(Wire);
SteamiDaplinkFlash flash(bridge);

static FakeDaplinkBridge& attachFirmware() {
    static FakeDaplinkBridge firmware;
    firmware = FakeDaplinkBridge();
    Wire.attachDevice(DAPLINK_BRIDGE_DEFAULT_ADDR, &firmware);
    return firmware;
}

static std::string fileOf(const FakeDaplinkBridge& firmware) {
    const std::vector<uint8_t> file = firmware.file();
    return std::string(file.begin(), file.end());
}

// Printable text of `length` bytes, never 0xFF.
static std::string pattern(size_t length) {
    std::string text;
    for (size_t i = 0; i < length; ++i) {
        text += static_cast<char>('A' + i % 26);
    }
    return text;
}

void setUp(void) {
    Wire = TwoWire();
    bridge = DaplinkBridge(Wire);
    flash = SteamiDaplinkFlash(bridge);
}

void tearDown(void) {}

// --------------------------------------------------
// Lifecycle and filename
// --------------------------------------------------

void test_begin_detects_bridge(void) {
    attachFirmware();
    TEST_ASSERT_TRUE(flash.begin());
}

void test_set_filename_stores_padded_upper_case_name(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    TEST_ASSERT_TRUE(flash.setFilename("data", "csv"));
    TEST_ASSERT_EQUAL_STRING_LEN("DATA    CSV",
                                 reinterpret_cast<const char*>(firmware.filename.data()), 11);
}

void test_get_filename_trims_padding(void) {
    attachFirmware();
    TEST_ASSERT_TRUE(flash.setFilename("LOG", "TX"));

    SteamiDaplinkFlash::Filename name;
    TEST_ASSERT_TRUE(flash.getFilename(name));
    TEST_ASSERT_EQUAL_STRING("LOG", name.name);
    TEST_ASSERT_EQUAL_STRING("TX", name.ext);
}

void test_get_filename_fails_when_never_set(void) {
    attachFirmware();
    SteamiDaplinkFlash::Filename name;
    TEST_ASSERT_FALSE(flash.getFilename(name));
    TEST_ASSERT_EQUAL_STRING("", name.name);
}

void test_set_filename_rejects_invalid_names(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    TEST_ASSERT_FALSE(flash.setFilename(nullptr, "CSV"));
    TEST_ASSERT_FALSE(flash.setFilename("DATA", nullptr));
    TEST_ASSERT_FALSE(flash.setFilename("", "CSV"));
    TEST_ASSERT_FALSE(flash.setFilename("TOOLONGNAME", "CSV"));
    TEST_ASSERT_FALSE(flash.setFilename("DATA", "CSVX"));
    TEST_ASSERT_FALSE(flash.setFilename("DA.TA", "CSV"));
    TEST_ASSERT_FALSE(flash.setFilename("DATA", "C*V"));
    TEST_ASSERT_FALSE(flash.setFilename(" DATA", "CSV"));
    // Nothing reached the bridge.
    TEST_ASSERT_EQUAL_HEX8(0xFF, firmware.filename[0]);
}

void test_set_filename_detects_flash_failure(void) {
    // A flash failure only sets the error bit that the firmware clears on
    // the next command: setFilename() must read the name back.
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.failWrites = true;
    TEST_ASSERT_FALSE(flash.setFilename("DATA", "CSV"));
}

// --------------------------------------------------
// Writing
// --------------------------------------------------

void test_write_appends_in_32_byte_frames(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    const std::string text = pattern(70);

    TEST_ASSERT_EQUAL_INT(70, flash.write(text.c_str()));

    TEST_ASSERT_EQUAL_STRING(text.c_str(), fileOf(firmware).c_str());
    // 30 + 30 + 10 bytes, each frame padded to the 31 argument bytes the
    // firmware waits for.
    TEST_ASSERT_EQUAL_INT(3, firmware.writeFrameLengths.size());
    for (size_t length : firmware.writeFrameLengths) {
        TEST_ASSERT_EQUAL_INT(32, length);
    }
    TEST_ASSERT_EQUAL_INT(0, firmware.ignoredFrames);
}

void test_write_appends_to_existing_file(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.storeFile("abc");
    TEST_ASSERT_EQUAL_INT(3, flash.write("def"));
    TEST_ASSERT_EQUAL_STRING("abcdef", fileOf(firmware).c_str());
}

void test_write_line_appends_newline(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    TEST_ASSERT_EQUAL_INT(4, flash.writeLine("1,2"));
    TEST_ASSERT_EQUAL_INT(4, flash.writeLine("3,4"));
    TEST_ASSERT_EQUAL_STRING("1,2\n3,4\n", fileOf(firmware).c_str());
}

void test_write_stops_before_0xff(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    const uint8_t data[] = {'o', 'k', 0xFF, 'x'};
    TEST_ASSERT_EQUAL_INT(2, flash.write(data, sizeof(data)));
    TEST_ASSERT_EQUAL_STRING("ok", fileOf(firmware).c_str());
}

void test_write_stops_when_file_is_full(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    const std::string almostFull = pattern(FakeDaplinkBridge::FLASH_SIZE - 10);
    firmware.storeFile(almostFull.c_str());

    // The chunk that fills the flash goes through, the next one is refused.
    const std::string text = pattern(60);
    TEST_ASSERT_EQUAL_INT(30, flash.write(text.c_str()));
    TEST_ASSERT_EQUAL_INT(FakeDaplinkBridge::FLASH_SIZE, firmware.file().size());
}

// --------------------------------------------------
// Reading
// --------------------------------------------------

void test_read_sector_returns_256_bytes(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    const std::string text = pattern(600);
    firmware.storeFile(text.c_str());

    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_TRUE(flash.readSector(1, sector));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(text.data() + 256, sector, 256);
    TEST_ASSERT_EQUAL_INT(256, firmware.largestRead);
}

void test_read_sector_rejects_bad_arguments(void) {
    attachFirmware();
    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_FALSE(flash.readSector(0, nullptr));
    TEST_ASSERT_FALSE(flash.readSector(DAPLINK_FLASH_MAX_SECTORS, sector));
}

void test_read_sector_fails_on_device_error(void) {
    // Past the end of the (shortened) fake flash, READ_SECTOR fails.
    attachFirmware();
    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_FALSE(
        flash.readSector(FakeDaplinkBridge::FLASH_SIZE / FakeDaplinkBridge::SECTOR_SIZE, sector));
}

void test_read_returns_file_across_sectors(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    const std::string text = pattern(600);
    firmware.storeFile(text.c_str());

    uint8_t buf[1024];
    TEST_ASSERT_EQUAL_INT(600, flash.read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(text.data(), buf, 600);
}

void test_read_stops_at_max_len(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.storeFile(pattern(600).c_str());

    uint8_t buf[300];
    TEST_ASSERT_EQUAL_INT(300, flash.read(buf, sizeof(buf)));
}

void test_read_empty_file(void) {
    attachFirmware();
    uint8_t buf[16];
    TEST_ASSERT_EQUAL_INT(0, flash.read(buf, sizeof(buf)));
}

void test_write_then_read_round_trip(void) {
    attachFirmware();
    const std::string text = pattern(300);
    TEST_ASSERT_EQUAL_INT(300, flash.write(text.c_str()));

    uint8_t buf[512];
    TEST_ASSERT_EQUAL_INT(300, flash.read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(text.data(), buf, 300);
}

// --------------------------------------------------
// Clearing
// --------------------------------------------------

void test_clear_flash_erases_file(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.storeFile(pattern(500).c_str());

    TEST_ASSERT_TRUE(flash.clearFlash());
    TEST_ASSERT_EQUAL_INT(1, firmware.clearFlashCommands);
    TEST_ASSERT_EQUAL_INT(0, firmware.file().size());
}

void test_clear_flash_fails_on_i2c_error(void) {
    attachFirmware();
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(flash.clearFlash());
}

// --------------------------------------------------
// Firmwares before READ_SECTOR took a 2-byte sector number
// --------------------------------------------------

void test_legacy_firmware_cannot_read(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;
    firmware.storeFile("data");

    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_FALSE(flash.readSector(0, sector));
    uint8_t buf[16];
    TEST_ASSERT_EQUAL_INT(0, flash.read(buf, sizeof(buf)));
    TEST_ASSERT_FALSE(firmware.hung);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_bridge);
    RUN_TEST(test_set_filename_stores_padded_upper_case_name);
    RUN_TEST(test_get_filename_trims_padding);
    RUN_TEST(test_get_filename_fails_when_never_set);
    RUN_TEST(test_set_filename_rejects_invalid_names);
    RUN_TEST(test_set_filename_detects_flash_failure);
    RUN_TEST(test_write_appends_in_32_byte_frames);
    RUN_TEST(test_write_appends_to_existing_file);
    RUN_TEST(test_write_line_appends_newline);
    RUN_TEST(test_write_stops_before_0xff);
    RUN_TEST(test_write_stops_when_file_is_full);
    RUN_TEST(test_read_sector_returns_256_bytes);
    RUN_TEST(test_read_sector_rejects_bad_arguments);
    RUN_TEST(test_read_sector_fails_on_device_error);
    RUN_TEST(test_read_returns_file_across_sectors);
    RUN_TEST(test_read_stops_at_max_len);
    RUN_TEST(test_read_empty_file);
    RUN_TEST(test_write_then_read_round_trip);
    RUN_TEST(test_clear_flash_erases_file);
    RUN_TEST(test_clear_flash_fails_on_i2c_error);
    RUN_TEST(test_legacy_firmware_cannot_read);
    return UNITY_END();
}
