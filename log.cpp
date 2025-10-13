#include <LittleFS.h>
#include <WebSerial.h>
#include <time.h>

#include "log.h"

const char* logFilePath = "/log.txt";
const size_t maxLogFileSize = 4096 * 10; // 40KB max log size, adjust as needed
bool isWebSerialStarted = false;

void initiateLog() {
    if (!LittleFS.begin()) {
        WebSerial.println("[LOG] LittleFS mount failed");
        return;
    }
    if (!LittleFS.exists(logFilePath)) {
        File logFile = LittleFS.open(logFilePath, "w");
        if (logFile) {
            logFile.println("[LOG] log initiated!");
            logFile.close();
        }
    } 
    else {
        File logFile = LittleFS.open(logFilePath, "a");
        if (logFile) {
            logFile.println("[LOG] new session started!");
            logFile.close();
        }
    }
}

String getTimestamp() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    unsigned long millisecond = millis() % 1000;
    char buffer[30];

    snprintf(
        buffer,
        sizeof(buffer), 
        "%02d.%02d.%04d %02d:%02d:%02d.%03lu",
        timeinfo.tm_mday,
        timeinfo.tm_mon + 1,
        timeinfo.tm_year + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        millisecond
    );

    return String(buffer);
}

void truncateLogIfNeeded() {
    File logFile = LittleFS.open(logFilePath, "r");
    if (!logFile) {
        WebSerial.println("[LOG] Failed to open log file for truncation check");
        return;
    }

    size_t fileSize = logFile.size();
    if (fileSize <= maxLogFileSize) {
        logFile.close();
        return;
    }

    // Read entire log file into memory
    String logContent = logFile.readString();
    logFile.close();

    // Remove oldest lines until size is below half of maxLogFileSize
    int removeUntil = logContent.length() - (maxLogFileSize / 2);
    int firstNewlinePos = logContent.indexOf('\n', removeUntil);
    if (firstNewlinePos != -1) {
        logContent = logContent.substring(firstNewlinePos + 1);
    } else {
        // If no newline found, clear log
        logContent = "";
    }

    // Rewrite truncated log
    logFile = LittleFS.open(logFilePath, "w");
    if (!logFile) {
        WebSerial.println("[LOG] Failed to open log file for truncation");
        return;
    }
    logFile.println("[LOG] Log truncated due to size limit");
    logFile.print(logContent);
    logFile.close();
}

void announceWebSerialReady() {
    isWebSerialStarted = true;
}

void logMessage(const String& message) {
    String timestampedMessage = "[" + getTimestamp() + "] " + message;
    if (isWebSerialStarted) {
        WebSerial.println(timestampedMessage); // Print to WebSerial for real-time monitoring
    }

    if (Serial) {
        Serial.println(timestampedMessage.c_str());
    }

    truncateLogIfNeeded();

    File logFile = LittleFS.open(logFilePath, "a");
    if (!logFile && isWebSerialStarted) {
        WebSerial.println("[LOG] Failed to open log file");
        return;
    }
    logFile.println(timestampedMessage);
    logFile.close();
}

String readLog() {
    File logFile = LittleFS.open(logFilePath, "r");
    if (!logFile) {
        return "[LOG] Failed to open log file";
    }

    String logContent = logFile.readString();
    logFile.close();
    return logFile ? logFile.readString() : "[LOG] Log file empty or unreadable";
}