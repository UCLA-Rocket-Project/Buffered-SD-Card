#include "FS.h"
#include <SD.h>
#include <buffered_sd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/types.h>

BufferedSD::BufferedSD(
    SPIClass &spi_bus, uint8_t CS, const char *base_filepath, const char *extension,
    size_t buffer_size
)
    : _spi(&spi_bus), _CS_pin(CS), _buffer_size(buffer_size), _base_path(base_filepath),
      _extension(extension), _file_number(-1) {
    // add the () behind to zero-initialize the buffer
    _write_buffer = new uint8_t[buffer_size]();
    configASSERT(_write_buffer && "Failed to initialize write buffer");
    _buffer_idx = 0;
}

BufferedSD::~BufferedSD() {
    flush_buffer();
    delete[] _write_buffer;
}

bool BufferedSD::begin() {
    for (int i = 0; i < NUM_TRIES_TO_OPEN; ++i) {
        if (SD.begin(_CS_pin, *_spi, 1200000)) {
            break;
        } else if (i == NUM_TRIES_TO_OPEN - 1) {
            return false;
        }
        delay(100);
    }

    create_new_file();

    return true;
}

int BufferedSD::write(const char *data) { return write(data, strlen(data)); }

int BufferedSD::write(const char *data, size_t length) {
    // Handle data larger than buffer
    if (length > _buffer_size) {
        flush_buffer();
        // Write directly to file for oversized data
        return write_immediate(data, length);
    }

    if (_buffer_idx + length > _buffer_size) {
        flush_buffer();
    }

    memcpy(_write_buffer + _buffer_idx, data, length);
    _buffer_idx += length;
    return length;
}

int BufferedSD::write_immediate(const char *data) { return write_immediate(data, strlen(data)); }

int BufferedSD::write_immediate(const char *data, size_t length) {
    flush_buffer();

    File f = SD.open(_filepath, FILE_APPEND);
    if (!f)
        return -1;
    size_t written_length = 0;
    written_length += f.write(reinterpret_cast<const uint8_t *>(data), length);
    f.close();

    return written_length;
}

void BufferedSD::print_contents() {
    File f = SD.open(_filepath, FILE_READ);
    if (!f) {
        Serial.print("The file cannot be opened");
        return;
    }

    while (f.available()) {
        Serial.print((char)f.read());
    }

    f.close();
}

void BufferedSD::find_first_available_file(const char *planned_filepath, const char *extension) {
    char temp_filepath[FILEPATH_NAME_MAX_LENGTH];

    while (true) {
        // FAT32 (file format used by XTSD has a limit of 256 files per directory)
        for (int i = 0; i < 256; ++i) {
            snprintf(
                temp_filepath, FILEPATH_NAME_MAX_LENGTH, "/%s-%d%s", planned_filepath, i, extension
            );

            if (!SD.exists(temp_filepath)) {
                strncpy(_filepath, temp_filepath, FILEPATH_NAME_MAX_LENGTH);
                _file_number = i;
                // null terminate the resulting string just in case
                _filepath[FILEPATH_NAME_MAX_LENGTH - 1] = '\0';
                return;
            }
        }
    }
}

sd_card_update BufferedSD::get_file_update() {
    File file = SD.open(_filepath, "r");
    if (!file) {
        return {0, 0};
    }

    uint32_t file_size = file.size();

    // skip past the final \n character
    uint32_t offset = file_size - 3;
    file.seek(offset, SeekSet);
    char curr = file.peek();

    // find the second last \n character
    while (offset > 0 && curr != '\n') {
        file.seek(offset, SeekSet);
        offset--;
        curr = file.peek();
    }

    // biggest value is 4,294,967,295
    // go back and read the last timestamp present on the file
    char number[11];
    int i = 0;
    while (file.available()) {
        number[i] = file.read();
        if (number[i] == ',') {
            break;
        }
        i++;
    }

    number[i++] = '\0';

    char *endptr;

    return {file_size, strtoul(number, &endptr, 10)};
}

sd_card_update BufferedSD::get_file_update_bin(
    sd_card_header_footer_info hf_info[], uint8_t hf_into_count, uint8_t struct_buffer[]
) {
    File file = SD.open(_filepath, "r");
    if (!file) {
        return {0, 0};
    }

    uint32_t file_size = file.size();

    // other odd behavior
    // if the chunk size is anything but 1, I have to little endian it
    //
    // have not entirely figured out why a chunk size of < 4 does not work though
    constexpr uint32_t backtrack_chunk_size = 512;

    uint32_t offset = file_size;
    uint8_t backtrack_buffer[backtrack_chunk_size]{};

    // track the matching struct
    int found_idx = -1;
    // track the position within the buffer that the file was found at
    int64_t header_found_position = -1;

    // search for the file header
    uint8_t candidate_header[sizeof(uint32_t)]{};
    while (true) {
        while (found_idx == -1 && offset != 0 && header_found_position == -1) {
            uint32_t backtrack_amt = min(offset, backtrack_chunk_size);
            offset -= backtrack_amt;
            file.seek(offset, SeekSet);

            for (uint32_t i = 0; i < backtrack_amt; i++) {
                backtrack_buffer[i] = file.read();
            }

            // try to seek in the buffer for the header
            int buffer_idx = 0;
            for (int i = 0; i < backtrack_amt; i++) {
                // we write to the back of the array, even though we are seeking forward
                // in the file, because everything is in little endian
                candidate_header[3] = backtrack_buffer[i];

                uint32_t potential_header = 0;
                memcpy(&potential_header, candidate_header, sizeof(uint32_t));

                if (potential_header != 0) {
                    for (int k = 0; k < hf_into_count; k++) {
                        if (potential_header == hf_info[k].header) {
                            // we subtract 3 here, since we are little-endianing the number
                            // when we've found the correct number, we would be 3 past the
                            // start of the struct
                            header_found_position = offset + i - 3;
                            found_idx = k;
                            break;
                        }
                    }
                }

                // if not found, slide the window one byte back
                // this byte is kept thorughout the parent while loop
                if (found_idx != -1 && header_found_position != -1) {
                    break;
                }

                candidate_header[0] = candidate_header[1];
                candidate_header[1] = candidate_header[2];
                candidate_header[2] = candidate_header[3];
            }
        }

        if (found_idx == -1 || header_found_position == -1) {
            return {0, 0};
        }

        file.seek(header_found_position, SeekSet);
        for (int i = 0; i < hf_info[found_idx].struct_size; i++) {
            struct_buffer[i] = file.read();
        }

        int64_t timestamp{};
        memcpy(&timestamp, (struct_buffer + hf_info[found_idx].timestamp_offset), sizeof(int64_t));

        // the timestamp is an int64_t but most of our code was previously written assuming it was
        // uint32_t this is something that could definitely be improved in the future though
        return {file_size, timestamp};
    }
}

void BufferedSD::get_file_name(char *buf) { strncpy(buf, _filepath, strlen(_filepath) + 1); }

bool BufferedSD::delete_all_files() { return delete_all_files_with_exception(nullptr); }

bool BufferedSD::delete_all_files_with_exception(const char *exception) {
    File root = SD.open("/");
    if (!root) {
        return false;
    }

    File file = root.openNextFile();

    while (file) {
        char name[FILEPATH_NAME_MAX_LENGTH];
        snprintf(name, FILEPATH_NAME_MAX_LENGTH, "/%s", file.name());

        if (exception && strcmp(name, exception) == 0) {
            continue;
        }

        SD.remove(name);

        file = root.openNextFile();

        // add timeout so there is no watchdog reset
        delay(1);
    }

    return true;
}

int BufferedSD::count_top_level_files() {
    int counter = 0;

    File root = SD.open("/");
    if (!root) {
        return -1;
    }
    if (!root.isDirectory()) {
        return -2;
    }

    File file = root.openNextFile();
    while (file) {
        counter++;
        file = root.openNextFile();
    }

    return counter;
}

bool BufferedSD::create_new_file() {
    find_first_available_file(_base_path, _extension);

    File f = SD.open(_filepath, FILE_WRITE);
    if (!f) {
        return false;
    }
    f.close();

    return true;
}

unsigned long BufferedSD::get_free_space() {
    return (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024);
}

int BufferedSD::update_config(const char tx_buf[], size_t config_length) {
    File file_handle = {};

    if (!SD.exists(SD_CONFIG_FILEPATH)) {
        file_handle = SD.open(SD_CONFIG_FILEPATH, FILE_WRITE);
    } else {
        file_handle = SD.open(SD_CONFIG_FILEPATH, FILE_APPEND);
    }

    if (!file_handle) {
        return -1;
    }

    int wrote = file_handle.write(reinterpret_cast<const uint8_t *>(tx_buf), config_length);
    file_handle.close();
    return wrote;
}

int BufferedSD::read_config(char rx_buf[], size_t buf_len) {
    File file_handle = {};

    if (!SD.exists(SD_CONFIG_FILEPATH)) {
        return -1;
    }

    file_handle = SD.open(SD_CONFIG_FILEPATH, FILE_READ);

    if (!file_handle)
        return -1;

    int i = 0;
    for (; i < buf_len; i++) {
        if (file_handle.available()) {
            rx_buf[i] = file_handle.read();
        } else {
            break;
        }
    }

    rx_buf[i] = '\0';
    return i;
}

void BufferedSD::clear_config_file() {
    if (!SD.exists(SD_CONFIG_FILEPATH)) {
        return;
    } else {
        SD.remove(SD_CONFIG_FILEPATH);
    }

    delay(10);
    SD.open(SD_CONFIG_FILEPATH, FILE_WRITE);
}