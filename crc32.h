#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <sys/types.h>

uint32_t update_crc32(uint32_t crc, const uint8_t* data, size_t len);
uint32_t crc32(const uint8_t* buf, size_t len);

#endif // CRC32_H
