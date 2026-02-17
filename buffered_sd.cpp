#include <SD.h>
#include <buffered_sd.h>
#include <cstring>

BufferedSD::BufferedSD(
    SPIClass &spi_bus, uint8_t CS, const char *base_filepath, const char *extension,
    size_t buffer_size
)
    : _spi(&spi_bus), _CS_pin(CS), _buffer_size(buffer_size), _base_path(base_filepath),
      _extension(extension) {
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
        if (SD.begin(_CS_pin, *_spi)) {
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

int BufferedSD::update_config(uint8_t tx_buf[], size_t config_length) {
    File file_handle = {};

    if (!SD.exists(SD_CONFIG_FILEPATH)) {
        file_handle = SD.open(SD_CONFIG_FILEPATH, FILE_WRITE);
    } else {
        file_handle = SD.open(SD_CONFIG_FILEPATH, FILE_APPEND);
    }

    if (!file_handle)
        return -1;

    int wrote = file_handle.write(tx_buf, config_length);
    file_handle.close();
    return wrote;
}

int BufferedSD::read_config(uint8_t rx_buf[], size_t buf_len) {
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