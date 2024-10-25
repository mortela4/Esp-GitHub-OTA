#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#include <ArduinoJson.h>
#include "common.h"
#include "semver.h"
#include "semver_extensions.h"

String get_updated_base_url_via_api(WiFiClientSecure& wifi_client, String release_url)
{
  const char *TAG = "get_updated_base_url_via_api";
  ESP_LOGI(TAG, "Release_url: %s\n", release_url.c_str());

  HTTPClient https;
  String base_url = "";

#ifdef ESP8266
  bool mfln = wifi_client.probeMaxFragmentLength("github.com", 443, 1024);
  ESP_LOGI(TAG, "MFLN supported: %s\n", mfln ? "yes" : "no");
  if (mfln) { wifi_client.setBufferSizes(1024, 1024); }
#endif

  if (!https.begin(wifi_client, release_url))
  {
    ESP_LOGI(TAG, "[HTTPS] Unable to connect\n");
    return base_url;
  }

  int httpCode = https.GET();
  if (httpCode < 0 || httpCode >= 400)
  {
    ESP_LOGI(TAG, "[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    char errorText[128];
#ifdef ESP8266
    int errCode = wifi_client.getLastSSLError(errorText, sizeof(errorText));
#elif defined(ESP32)
    int errCode = wifi_client.lastError(errorText, sizeof(errorText));
#endif
    ESP_LOGV(TAG, "httpCode: %d, errorCode %d: %s\n", httpCode, errCode, errorText);
  }
  else if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
  {
    StaticJsonDocument<64> filter;
    filter["html_url"] = true;

    StaticJsonDocument<256> doc;
    auto result = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
    if (result != DeserializationError::Ok) {
      ESP_LOGI(TAG, "deserializeJson error %s\n", result.c_str());
    }

    base_url = String((const char *)doc["html_url"]);
    base_url.replace("tag", "download");
    base_url += "/";
  }

  https.end();

  return base_url;
}

String get_updated_base_url_via_redirect(WiFiClientSecure& wifi_client, String release_url)
{
  const char *TAG = "get_updated_base_url_via_redirect";

  String location = get_redirect_location(wifi_client, release_url);
  ESP_LOGV(TAG, "location: %s\n", location.c_str());

  if (location.length() <= 0)
  {
    ESP_LOGE(TAG, "[HTTPS] No redirect url\n");

    return "";
  }

  String base_url = "";
  base_url = location + "/";
  base_url.replace("tag", "download");

  ESP_LOGV(TAG, "returns: %s\n", base_url.c_str());

  return base_url;
}

String get_redirect_location(WiFiClientSecure& wifi_client, String initial_url)
{
  const char *TAG = "get_redirect_location";
  ESP_LOGV(TAG, "initial_url: %s\n", initial_url.c_str());

  HTTPClient https;
  https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

#ifdef ESP8266
  bool mfln = wifi_client.probeMaxFragmentLength("github.com", 443, 1024);
  ESP_LOGI(TAG, "MFLN supported: %s\n", mfln ? "yes" : "no");
  if (mfln) { wifi_client.setBufferSizes(1024, 1024); }
#endif

  if (!https.begin(wifi_client, initial_url))
  {
    ESP_LOGE(TAG, "[HTTPS] Unable to connect\n");
    return "";
  }

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_FOUND)
  {
    ESP_LOGE(TAG, "[HTTPS] GET... failed, No redirect\n");
    char errorText[128];
#ifdef ESP8266
    int errCode = wifi_client.getLastSSLError(errorText, sizeof(errorText));
#elif defined(ESP32)
    int errCode = wifi_client.lastError(errorText, sizeof(errorText));
#endif
    ESP_LOGV(TAG, "httpCode: %d, errorCode %d: %s\n", httpCode, errCode, errorText);
  }

  String redirect_url = https.getLocation();
  https.end();

  ESP_LOGV(TAG, "returns: %s\n", redirect_url.c_str());
  return redirect_url;
}

void print_update_result(const Updater& updater, const HTTPUpdateResult& result, const char *TAG)
{
  switch (result){
    case HTTP_UPDATE_FAILED:
      ESP_LOGI(TAG, "HTTP_UPDATE_FAILED Error (%d): %s\n", updater.getLastError(), updater.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      ESP_LOGI(TAG, "HTTP_UPDATE_NO_UPDATES\n");
      break;
    case HTTP_UPDATE_OK:
      ESP_LOGI(TAG, "HTTP_UPDATE_OK\n");
      break;
  }
}

bool update_required(semver_t _new_version, semver_t _current_version){
  return _new_version > _current_version;
}

void update_started()
{
  ESP_LOGI("update_started", "HTTP update process started\n");
}

void update_finished()
{
  ESP_LOGI("update_finished", "HTTP update process finished\n");
}

void update_progress(int currentlyReceiced, int totalBytes)
{
  ESP_LOGI("update_progress", "Data received, Progress: %.2f %%\r", 100.0 * currentlyReceiced / totalBytes);
}

void update_error(int err)
{
  ESP_LOGI("update_error", "HTTP update fatal error code %d\n", err);
}

// Set time via NTP, as required for x.509 validation
void synchronize_system_time()
{
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(100);
    now = time(nullptr);
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
}


#if 0   // CERT specified in library code (outdated??):
const char *github_certificate PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)CERT";
#endif

#if 0       /* NOTE: generated by 'https://projects.petrucci.ch/esp32' tool!  */
const char* github_certificate PROGMEM = R"CERT(   
-----BEGIN CERTIFICATE-----
MIIEozCCBEmgAwIBAgIQTij3hrZsGjuULNLEDrdCpTAKBggqhkjOPQQDAjCBjzEL
MAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE
BxMHU2FsZm9yZDEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5T
ZWN0aWdvIEVDQyBEb21haW4gVmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMB4X
DTI0MDMwNzAwMDAwMFoXDTI1MDMwNzIzNTk1OVowFTETMBEGA1UEAxMKZ2l0aHVi
LmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABARO/Ho9XdkY1qh9mAgjOUkW
mXTb05jgRulKciMVBuKB3ZHexvCdyoiCRHEMBfFXoZhWkQVMogNLo/lW215X3pGj
ggL+MIIC+jAfBgNVHSMEGDAWgBT2hQo7EYbhBH0Oqgss0u7MZHt7rjAdBgNVHQ4E
FgQUO2g/NDr1RzTK76ZOPZq9Xm56zJ8wDgYDVR0PAQH/BAQDAgeAMAwGA1UdEwEB
/wQCMAAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMEkGA1UdIARCMEAw
NAYLKwYBBAGyMQECAgcwJTAjBggrBgEFBQcCARYXaHR0cHM6Ly9zZWN0aWdvLmNv
bS9DUFMwCAYGZ4EMAQIBMIGEBggrBgEFBQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0
dHA6Ly9jcnQuc2VjdGlnby5jb20vU2VjdGlnb0VDQ0RvbWFpblZhbGlkYXRpb25T
ZWN1cmVTZXJ2ZXJDQS5jcnQwIwYIKwYBBQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3Rp
Z28uY29tMIIBgAYKKwYBBAHWeQIEAgSCAXAEggFsAWoAdwDPEVbu1S58r/OHW9lp
LpvpGnFnSrAX7KwB0lt3zsw7CAAAAY4WOvAZAAAEAwBIMEYCIQD7oNz/2oO8VGaW
WrqrsBQBzQH0hRhMLm11oeMpg1fNawIhAKWc0q7Z+mxDVYV/6ov7f/i0H/aAcHSC
Ii/QJcECraOpAHYAouMK5EXvva2bfjjtR2d3U9eCW4SU1yteGyzEuVCkR+cAAAGO
Fjrv+AAABAMARzBFAiEAyupEIVAMk0c8BVVpF0QbisfoEwy5xJQKQOe8EvMU4W8C
IGAIIuzjxBFlHpkqcsa7UZy24y/B6xZnktUw/Ne5q5hCAHcATnWjJ1yaEMM4W2zU
3z9S6x3w4I4bjWnAsfpksWKaOd8AAAGOFjrv9wAABAMASDBGAiEA+8OvQzpgRf31
uLBsCE8ktCUfvsiRT7zWSqeXliA09TUCIQDcB7Xn97aEDMBKXIbdm5KZ9GjvRyoF
9skD5/4GneoMWzAlBgNVHREEHjAcggpnaXRodWIuY29tgg53d3cuZ2l0aHViLmNv
bTAKBggqhkjOPQQDAgNIADBFAiEAru2McPr0eNwcWNuDEY0a/rGzXRfRrm+6XfZe
SzhYZewCIBq4TUEBCgapv7xvAtRKdVdi/b4m36Uyej1ggyJsiesA
-----END CERTIFICATE-----
)CERT";
#endif

