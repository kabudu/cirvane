#include <assert.h>
#include <string.h>

#include "cirvane_wifi_policy.h"

int main(void)
{
    const char ssid_32[] = "12345678901234567890123456789012";
    const char password_63[] =
        "123456789012345678901234567890123456789012345678901234567890123";
    assert(!cirvane_wifi_ssid_valid("", 0));
    assert(cirvane_wifi_ssid_valid("Cirvane Lab", strlen("Cirvane Lab")));
    assert(cirvane_wifi_ssid_valid(ssid_32, strlen(ssid_32)));
    assert(!cirvane_wifi_ssid_valid("123456789012345678901234567890123", 33));
    assert(!cirvane_wifi_ssid_valid("bad\x1bssid", 8));
    assert(cirvane_wifi_password_valid("", 0));
    assert(!cirvane_wifi_password_valid("1234567", 7));
    assert(cirvane_wifi_password_valid("12345678", 8));
    assert(cirvane_wifi_password_valid(password_63, strlen(password_63)));
    assert(!cirvane_wifi_password_valid(password_63, 64));
    assert(!cirvane_wifi_password_valid("bad\npass", 8));
    return 0;
}
