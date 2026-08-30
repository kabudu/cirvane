#include <assert.h>
#include <stdint.h>

#include "cirvane_serial.h"

int main(void)
{
    assert(!cirvane_serial_u32_newer(7, 7));
    assert(cirvane_serial_u32_newer(8, 7));
    assert(!cirvane_serial_u32_newer(7, 8));
    assert(cirvane_serial_u32_newer(0, UINT32_MAX));
    assert(!cirvane_serial_u32_newer(UINT32_MAX, 0));
    assert(!cirvane_serial_u32_newer(0x80000000u, 0));
    assert(!cirvane_serial_u32_newer(0, 0x80000000u));
    assert(cirvane_serial_u32_newer(0x7fffffffu, 0));
    return 0;
}
