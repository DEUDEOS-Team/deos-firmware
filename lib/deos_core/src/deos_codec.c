#include "deos_internal.h"
#include <zephyr/sys/byteorder.h>
#include <errno.h>

void deos_put_u16_le(uint8_t *dst, uint16_t value)
{
    sys_put_le16(value, dst);
}

void deos_put_i16_le(uint8_t *dst, int16_t value)
{
    sys_put_le16((uint16_t)value, dst);
}

void deos_put_u32_le(uint8_t *dst, uint32_t value)
{
    sys_put_le32(value, dst);
}

uint16_t deos_get_u16_le(const uint8_t *src)
{
    return sys_get_le16(src);
}

int16_t deos_get_i16_le(const uint8_t *src)
{
    return (int16_t)sys_get_le16(src);
}

uint32_t deos_get_u32_le(const uint8_t *src)
{
    return sys_get_le32(src);
}
