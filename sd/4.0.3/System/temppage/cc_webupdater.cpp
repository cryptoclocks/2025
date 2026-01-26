#include "cc_webupdater.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SD.h>
#include <MD5Builder.h>
#include <time.h>
#include "cc_download.h"


const char* baseRawURL = "https://raw.githubusercontent.com/cryptoclocks/2025/main";  // **อย่ามี / ต่อท้าย**
const char* sdPath = "/sd/v3.7.0";  // **อย่ามี / ต่อท้าย**


// ===== 🔒 ฟังก์ชันภายใน =====
bool checkLastError();
bool checkOldSettingsFile();
bool attemptDownloadSettings();
bool downloadFile(const char* url, const char* savePath);

bool checkMD5(const char* path);
bool checkSize(const char* path, size_t minSize);
bool checkAvailableSpace(size_t requiredSpace);
void backupSettingsFile();
void createSettingsOk();
void safeRollback();
void logError(const char* errorCode);
void cleanupLogs();

// ===== 🚀 Implementations =====

void setupWebUpdater() {
  Serial.println("setupWebUpdater");

  if (checkLastError()) {
    Serial.println("⚠️ มี Error จากการรีบูตก่อนหน้า");
  }
  else
  {
    Serial.println("ไม่ มี Error จากการรีบูตก่อนหน้า");
  }

  if (SD.exists("/settings.ok")) {
    Serial.println("✅ settings.ok พบ ใช้งานไฟล์ได้ทันที");
    return;
  }
  else
  {
    Serial.println("✅ settings.ok ไม่พบ");
  }

  if (checkOldSettingsFile()) {
    createSettingsOk();
    return;
  }
  else
  {
    Serial.println("เวปเดิมไม่ผ่าน");
  }

  if (!attemptDownloadSettings()) {
    
    logError("ERR_DOWNLOAD_FAIL");
    Serial.println("rollback");
    safeRollback();
  }
}

// ===== 🔒 ฟังก์ชันภายใน =====




bool checkLastError() {
  if (!SD.exists("/last_error.log")) return false;
  File file = SD.open("/last_error.log", FILE_READ);
  if (!file) return false;
  Serial.println("📜 Last Error Detected:");
  while (file.available()) Serial.write(file.read());
  file.close();
  SD.remove("/last_error.log");
  return true;
}

bool checkOldSettingsFile() {
  if (!SD.exists("/System/website/settings.html")) return false;
  if (!checkMD5("/System/website/settings.html")) return false;
  if (!checkSize("/System/website/settings.html", MIN_SETTINGS_SIZE)) return false;
  Serial.println("✅ settings.html เก่าผ่านทุกเงื่อนไข");
  return true;
}

bool attemptDownloadSettings() {
  Serial.println("🚀 [เริ่ม] attemptDownloadSettings");

  if (!checkAvailableSpace(MIN_SETTINGS_SIZE)) {
    Serial.println("❌ [ERROR] พื้นที่ไม่พอสำหรับโหลด settings.html");
    logError("ERR_NO_SPACE");
    safeRollback();
    return false;
  }

  Serial.println("✅ [OK] พื้นที่ว่างเพียงพอ");

  backupSettingsFile();

  for (int i = 0; i < MAX_RETRY_DOWNLOAD; i++) {
    Serial.printf("➡️ [รอบ %d] กำลังดาวน์โหลด settings.html\n", i + 1);

    if (downloadFile("/System/website/settings.html")) {
      Serial.println("✅ [OK] ดาวน์โหลด settings.html สำเร็จ");

      bool md5Ok = checkMD5("/System/website/settings.html");
      bool sizeOk = checkSize("/System/website/settings.html", MIN_SETTINGS_SIZE);

      if (!md5Ok) {
        Serial.println("❌ [ERROR] MD5 ของ settings.html ไม่ตรงกับที่ต้องการ");
      }
      if (!sizeOk) {
        Serial.println("❌ [ERROR] ขนาดไฟล์ settings.html น้อยเกินไป");
      }

      if (md5Ok && sizeOk) {
        Serial.println("✅ [OK] settings.html ผ่านทุกเงื่อนไข (MD5 + Size)");
        createSettingsOk();
        ESP.restart();
      } else {
        Serial.println("⚠️ [FAIL] settings.html โหลดได้ แต่ไม่ผ่าน MD5 หรือ Size ตรวจสอบ");
      }
    } else {
      Serial.println("❌ [ERROR] โหลด settings.html ไม่สำเร็จจาก URL");
    }

    Serial.printf("⚠️ [RETRY] โหลดไฟล์ไม่สำเร็จ ครั้งที่ %d\n", i + 1);
    delay(3000);
  }

  Serial.println("❌ [END] attemptDownloadSettings - ล้มเหลวหลังจากพยายามครบทุกครั้ง");
  return false;
}




bool checkMD5(const char* path) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.println("❌ เปิดไฟล์เพื่อเช็ก MD5 ไม่ได้");
    return false;
  }
  MD5Builder md5;
  md5.begin();
  while (file.available()) {
    uint8_t buf[512];
    size_t len = file.read(buf, sizeof(buf));
    md5.add(buf, len);
  }
  file.close();
  md5.calculate();

  String calculatedMD5 = md5.toString();  // <<== ดึงค่าที่ได้
  Serial.printf("📄 ไฟล์: %s\n", path);
  Serial.printf("🔍 MD5 ที่ได้จริง: %s\n", calculatedMD5.c_str());
  Serial.printf("🔍 เทียบกับ REQUIRED_SETTINGS_MD5: %s\n", REQUIRED_SETTINGS_MD5);

  bool result = (calculatedMD5 == REQUIRED_SETTINGS_MD5);
  Serial.printf("🔎 ผลการตรวจสอบ: %s\n", result ? "ผ่าน" : "ไม่ผ่าน");
  return result;
}


bool checkSize(const char* path, size_t minSize) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  size_t size = file.size();
  file.close();
  if (size < minSize) {
    Serial.printf("❌ ขนาดไฟล์ %d bytes น้อยกว่า %d bytes\n", size, minSize);
    return false;
  }
  return true;
}

bool checkAvailableSpace(size_t requiredSpace) {
  uint64_t freeSpace = SD.totalBytes() - SD.usedBytes();
  Serial.printf("🗂️ พื้นที่ว่าง: %llu bytes\n", freeSpace);
  return freeSpace > requiredSpace;
}

void backupSettingsFile() {
  if (SD.exists("/System/website/settings.bak")) SD.remove("/System/website/settings.bak");
  if (SD.exists("/System/website/settings.html")) {
    if (SD.rename("/System/website/settings.html", "/System/website/settings.bak")) {
      Serial.println("✅ backup settings.html -> settings.bak สำเร็จ");
    } else {
      Serial.println("❌ backup settings.html ล้มเหลว");
    }
  }
}

void createSettingsOk() {
  File file = SD.open("/settings.ok", FILE_WRITE);
  if (!file) return;
  file.println("{\"firmwareVersion\":\"3.7.0\",\"settingsVersion\":\"3.7.0\",\"updateDate\":\"2025-05-01\",\"settingsMD5\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}");
  file.close();
  Serial.println("✅ สร้าง settings.ok เรียบร้อย");
}

void safeRollback() {
  Serial.println("🔴 กำลัง Rollback Firmware...");
  Update.rollBack();
  ESP.restart();
}

void logError(const char* errorCode) {
  char filename[64];
  time_t now = time(nullptr);
  struct tm* tm_info = localtime(&now);
  snprintf(filename, sizeof(filename), "/log/log_%04d%02d%02d_%02d%02d.txt",
           tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
           tm_info->tm_hour, tm_info->tm_min);
  File file = SD.open(filename, FILE_WRITE);
  if (!file) return;
  file.printf("[%04d-%02d-%02d %02d:%02d:%02d] ERROR: %s\n",
              tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
              tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
              errorCode);
  file.close();
  cleanupLogs();
}

void cleanupLogs() {
  File root = SD.open("/log");
  if (!root || !root.isDirectory()) return;
  File file = root.openNextFile();
  int fileCount = 0;
  while (file) {
    fileCount++;
    file = root.openNextFile();
  }
  if (fileCount > MAX_LOG_FILES) {
    File oldest = root.openNextFile();
    if (oldest) {
      Serial.printf("🧹 ลบ log เก่า: %s\n", oldest.name());
      SD.remove(oldest.name());
    }
  }
}