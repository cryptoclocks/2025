#ifndef CC_DOWNLOAD_H
#define CC_DOWNLOAD_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>

// ประกาศตัวแปร extern ถ้ามีการใช้ข้ามไฟล์
extern const char* baseRawURL; // URL ต้นทาง github raw
extern const char* sdPath;     // path เริ่มต้นที่ต้องดาวน์โหลดจาก github
extern HTTPClient http3;       // ใช้ HTTPClient ชุดที่ 3 สำหรับโหลดไฟล์

// ฟังก์ชันที่ใช้จาก cc_download.cpp
void createRootDirs();
void verifyFiles();
bool ensurePath(String dir);
bool downloadFile(const char* filepath);

// ค่าคงที่สำหรับไฟล์และโฟลเดอร์
extern const char* files[];
extern const size_t FILE_COUNT;
extern const char* rootDirs[];
extern const size_t DIR_COUNT;

#endif // CC_DOWNLOAD_H