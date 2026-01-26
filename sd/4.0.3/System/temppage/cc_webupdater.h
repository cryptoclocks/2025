#ifndef CC_WEBUPDATER_H
#define CC_WEBUPDATER_H

#include <WebServer.h>

#define REQUIRED_SETTINGS_MD5 "6996bb3e0a867f117faedc1f91745267"
//"2345c4929ef268fe0cce472d6f3ab246"
//"d41d8cd98f00b204e9800998ecf8427e"
// 6996bb3e0a867f117faedc1f91745267
#define MIN_SETTINGS_SIZE   1024
#define MAX_RETRY_DOWNLOAD  3
#define MAX_LOG_FILES       20

// ===== 🚀 ฟังก์ชัน public =====
void setupWebUpdater();
bool downloadFile(const char* filepath);
#endif // CC_WEBUPDATER_H