# Buffered SD

<hr />

## SD Library Performance Comparison
- Cut write times by 80%

## Test Setup
- **Device**: ESP32-S3-Wroom-1U
- **SD Card**: XTSD

## Performance Results

| SD interface | Time Taken (*) |
|----------------------------|----------------|
| Regular                    | 16.02160       |
| Buffered SD                | 3.05133        |

*Test of writing 200 bytes of data 1000 times

<hr />