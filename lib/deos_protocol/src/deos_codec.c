#include <deos/deos_codec.h>

void (deos_put_u16_le)(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

void (deos_put_i16_le)(uint8_t *dst, int16_t value)
{
    deos_put_u16_le(dst, (uint16_t)value);
}

void (deos_put_u32_le)(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

void (deos_put_i32_le)(uint8_t *dst, int32_t value)
{
    deos_put_u32_le(dst, (uint32_t)value);
}

uint16_t (deos_get_u16_le)(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

int16_t (deos_get_i16_le)(const uint8_t *src)
{
    return (int16_t)deos_get_u16_le(src);
}

uint32_t (deos_get_u32_le)(const uint8_t *src)
{
    return (uint32_t)((uint32_t)src[0] |
                     ((uint32_t)src[1] << 8) |
                     ((uint32_t)src[2] << 16) |
                     ((uint32_t)src[3] << 24));
}

int32_t (deos_get_i32_le)(const uint8_t *src)
{
    return (int32_t)deos_get_u32_le(src);
}
