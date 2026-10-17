/**
 * @file deos_codec.h
 * @brief Portable Endian and Byte-Packing Utilities for DEOS Protocol
 *
 * Provides pure ANSI C (C99) Little-Endian encode and decode routines.
 * Zero external dependencies (No RTOS, No POSIX required).
 */

#ifndef DEOS_CODEC_H
#define DEOS_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Little-Endian Serialization (Encoding)
 */
void deos_put_u16_le(uint8_t *dst, uint16_t value);
void deos_put_i16_le(uint8_t *dst, int16_t value);
void deos_put_u32_le(uint8_t *dst, uint32_t value);
void deos_put_i32_le(uint8_t *dst, int32_t value);

/*
 * Little-Endian Deserialization (Decoding)
 */
uint16_t deos_get_u16_le(const uint8_t *src);
int16_t  deos_get_i16_le(const uint8_t *src);
uint32_t deos_get_u32_le(const uint8_t *src);
int32_t  deos_get_i32_le(const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_CODEC_H */
