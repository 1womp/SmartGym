#pragma once

// Copy this file to SmartGymWifiConfig.h for local private builds, or prefer
// PlatformIO build flags so credentials never enter Git.
//
// Example build flags:
// -D SMARTGYM_WIFI_PRIMARY_SSID="\"MySSID\""
// -D SMARTGYM_WIFI_PRIMARY_PASSWORD="\"MyPass\""
// -D SMARTGYM_FIREBASE_DATABASE_URL="\"https://your-project-default-rtdb.firebaseio.com\""
// -D SMARTGYM_FIREBASE_AUTH_TOKEN="\"your-token-if-used\""

#define SMARTGYM_WIFI_PRIMARY_SSID "YourSSID"
#define SMARTGYM_WIFI_PRIMARY_PASSWORD "YourPassword"
#define SMARTGYM_WIFI_SECONDARY_SSID ""
#define SMARTGYM_WIFI_SECONDARY_PASSWORD ""
