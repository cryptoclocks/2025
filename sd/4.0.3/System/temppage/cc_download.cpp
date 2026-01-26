#include "Arduino.h"
#include "cc_download.h"
#include <Arduino.h>     // เพราะใช้ String, size_t, delay()
#include <WiFi.h>        // เพราะใช้ WiFiClient ผ่าน HTTPClient
#include <HTTPClient.h>  // เพราะใช้ HTTPClient (http3)
#include <SD.h>          // เพราะใช้ SD.open, SD.exists, SD.mkdir

#include "GlobalModule.h"
#include "GlobalParameter.h"


//sd card
// ---------- รายการไฟล์ ----------
const char* files[] = {
  "/bg001.jpg",
  "/System/website/logo.ico",
  "/System/website/index.html",
  "/System/website/gallery.html",
  "/System/website/logo.png",
  "/System/website/api_config.json",
  "/System/website/api.html",
  "/System/website/settings.html",
  "/System/image/ads.jpg",
  "/System/image/logo.ico",
  "/System/image/logo200.jpg",
  "/System/image/cct.jpg",
  "/System/image/ccp.jpg",
  "/System/image/huaybg2.jpg",
  "/System/image/usdt.jpg",
  "/System/image/pagecm.jpg",
  "/System/image/logo.png",
  "/System/image/Manual.jpg",
  "/System/image/profile.jpg",
  "/System/image/oilpttbg.jpg",
  "/System/image/promptpay.jpg",
  "/System/image/ln.jpg",
  "/System/image/goldbg.jpg",
  "/System/image/page2.jpg",
  "/System/data/topScore.json",
  "/logo.png"
};

const size_t FILE_COUNT = sizeof(files) / sizeof(files[0]);

/******************** 1) รายชื่อโฟลเดอร์หลัก ********************/
const char* rootDirs[] = {
  "/mp3",
  "/System/data",
  "/System/website",
  "/System/image"
};
const size_t DIR_COUNT = sizeof(rootDirs) / sizeof(rootDirs[0]);


/******************** 2) ฟังก์ชันสร้างโฟลเดอร์หลัก ********************/
void createRootDirs() {
  for (size_t i = 0; i < DIR_COUNT; ++i) {
    const char* dir = rootDirs[i];
    if (ensurePath(dir)) {
      Serial.printf("✓ โฟลเดอร์พร้อม: %s\n", dir);
    } else {
      Serial.printf("✗ สร้างโฟลเดอร์ไม่สำเร็จ: %s\n", dir);
    }
  }
}

// ---------- สร้างโฟลเดอร์ย่อยตามพาธ ----------
bool ensurePath(String dir) {
  if (dir == "/" || dir.length() == 0) return true;
  if (dir[0] != '/') dir = "/" + dir;

  String current = "";
  int start = 1;

  while (true) {
    int slash = dir.indexOf('/', start);
    String token = (slash == -1) ? dir.substring(start) : dir.substring(start, slash);
    if (token.length()) {
      current += "/" + token;

      // ---------- สร้างโฟลเดอร์ด้วย retry ----------
      uint8_t retry = 0;
      while (!SD.exists(current) && retry < 3) {
        size_t freeHeap = ESP.getFreeHeap();
        Serial.printf("mkdir %s (heap %u)  try %u\n",
                      current.c_str(), freeHeap, retry + 1);

        if (SD.mkdir(current)) break;          // สำเร็จ
        retry++;
        delay(100);                            // หน่วงเล็กน้อยก่อนลองใหม่
      }

      if (!SD.exists(current)) {               // ยังไม่สำเร็จ
        Serial.printf("❌ mkdir failed: %s\n", current.c_str());
        return false;
      }
    }

    if (slash == -1) break;
    start = slash + 1;
  }
  return true;
}




// ---------- ตรวจสอบ & ดาวน์โหลด ----------
void verifyFiles() {
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    const char* path = files[i];
    if (SD.exists(path)) {
      Serial.printf("พบแล้ว: %s\n", path);
    } else {
      Serial.printf("ไม่พบ: %s → เริ่มดาวน์โหลด\n", path);
      if (!downloadFile(path)) {
        Serial.printf("ดาวน์โหลดล้มเหลว: %s\n", path);
      }
    }
  }
}

// ---------- ดาวน์โหลดและบันทึกไฟล์ ----------
bool downloadFile(const char* filepath)
{
  //file path ex
  // "/sd/v3.7.0"
  // "/System/website/settings.html"
  
  // สร้าง URL ปลายทางบน GitHub (raw)
  String url = String(baseRawURL) + String(sdPath) + String(filepath);

  // ตำแหน่งเก็บบน SD การ์ด
  String localPath = filepath;

  // สร้างโฟลเดอร์ย่อย (ถ้ายังไม่มี)
  int slashPos = localPath.lastIndexOf('/');
  if (slashPos > 0) {
    String dir = localPath.substring(0, slashPos);
    if (!ensurePath(dir)) return false;
  }

  Serial.printf("ดาวน์โหลด: %s → %s\n", url.c_str(), localPath.c_str());

  // เริ่ม HTTP GET
  http3.begin(url);
  int httpCode = http3.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP Error %d\n", httpCode);
    http3.end();
    return false;
  }

  int contentLength = http3.getSize();
  WiFiClient* stream = http3.getStreamPtr();

  // เปิดไฟล์ปลายทาง
  File file = SD.open(localPath, FILE_WRITE);
  if (!file) {
    Serial.println("เปิดไฟล์ไม่สำเร็จ");
    http3.end();
    return false;
  }

  // ถ่ายข้อมูลทีละก้อน 512 ไบต์
  uint8_t buff[512];
  int written = 0;
  while (http3.connected() && (contentLength > 0 || contentLength == -1)) {
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(buff, min(size, sizeof(buff)));
      file.write(buff, c);
      written += c;
      if (contentLength > 0) contentLength -= c;
    }
    delay(1); // ป้องกัน watchdog
  }
  file.close();
  http3.end();

  Serial.printf("บันทึกแล้ว %d ไบต์\n", written);
  return (written > 0);
}