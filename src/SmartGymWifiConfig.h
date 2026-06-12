#pragma once

// Override these from build flags for production:
// -D SMARTGYM_WIFI_PRIMARY_SSID="\"MySSID\""
// -D SMARTGYM_WIFI_PRIMARY_PASSWORD="\"MyPass\""
// -D SMARTGYM_WIFI_SECONDARY_SSID="\"BackupSSID\""
// -D SMARTGYM_WIFI_SECONDARY_PASSWORD="\"BackupPass\""
#ifndef SMARTGYM_WIFI_PRIMARY_SSID
#define SMARTGYM_WIFI_PRIMARY_SSID ""
#endif
#ifndef SMARTGYM_WIFI_PRIMARY_PASSWORD
#define SMARTGYM_WIFI_PRIMARY_PASSWORD ""
#endif
#ifndef SMARTGYM_WIFI_SECONDARY_SSID
#define SMARTGYM_WIFI_SECONDARY_SSID ""
#endif
#ifndef SMARTGYM_WIFI_SECONDARY_PASSWORD
#define SMARTGYM_WIFI_SECONDARY_PASSWORD ""
#endif

constexpr const char* kWifiPrimarySsid = SMARTGYM_WIFI_PRIMARY_SSID;
constexpr const char* kWifiPrimaryPassword = SMARTGYM_WIFI_PRIMARY_PASSWORD;
constexpr const char* kWifiSecondarySsid = SMARTGYM_WIFI_SECONDARY_SSID;
constexpr const char* kWifiSecondaryPassword = SMARTGYM_WIFI_SECONDARY_PASSWORD;
