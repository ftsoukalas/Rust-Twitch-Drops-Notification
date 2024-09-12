#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>

#include <sstream>  // For std::istringstream and std::ostringstream
#include <iomanip>  // For std::get_time and std::put_time
#include <ctime>    // For std::tm, std::localtime, std::gmtime, etc.

#include <chrono>
#include <thread> // For std::this_thread::sleep_for

#define timegm _mkgmtime

#pragma comment(lib, "wininet.lib")

// Log dates to file
static void LogToFile(const std::string& message) {
    std::ofstream logfile("output.log", std::ios::app);
    if (logfile.is_open()) {
        logfile << message << std::endl;
        logfile.close();
    }
}

// Convert UTC time to local time
static std::string ConvertUTCToLocal(const std::string& utcTimeStr) {
    std::tm tm = {};
    std::istringstream ss(utcTimeStr);
    ss >> std::get_time(&tm, "%B %d, %Y at %I:%M %p UTC");

    // Convert to time_t assuming UTC
    std::time_t utcTime = timegm(&tm);

    // Convert to local time
    std::tm localTime{};
    std::tm* localTimePtr = &localTime;
    localtime_s(localTimePtr, &utcTime);

    // Check if it's Daylight Saving Time (DST)
    std::tm localNow{};
    std::tm* localNowPtr = &localNow;
    std::time_t now = std::time(nullptr);
    localtime_s(localNowPtr, &now);
    bool isDST = (localNowPtr->tm_isdst > 0);

    std::ostringstream os;
    os << std::put_time(localTimePtr, "%B %d, %Y at %I:%M %p ");

    // Add timezone abbreviation
    if (isDST) {
        os << "EEST"; // Eastern European Summer Time
    }
    else {
        os << "EET";  // Eastern European Time
    }

    return os.str();
}

// Separate function to show MessageBox in a new thread
static void ShowMessageBox(const std::wstring& wideMessage) {
    MessageBox(NULL, wideMessage.c_str(), L"Twitch Notification: Rust Drops Event!", MB_OK);
}

// Parse times and display them in popup window
static void ReadWebPageHTML(const wchar_t* url) {
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    DWORD bytesRead = 0;
    char buffer[4096]{};
    std::string htmlContent; // Accumulate HTML content

    // Initialize WinINet
    hInternet = InternetOpen(L"WinINet Sample", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        std::cerr << "InternetOpen failed" << std::endl;
        return;
    }

    // Connect to the website
    hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        std::cerr << "InternetOpenUrl failed" << std::endl;
        InternetCloseHandle(hInternet);
        return;
    }

    // Read and accumulate the HTML content
    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the buffer
        htmlContent.append(buffer); // Append to the accumulated content
    }

    // Find the trigger text
    if (htmlContent.find("Event Live") == std::string::npos) {
        // Clean up
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }

    // Find and extract the dates
    std::string startTag = "<span class=\"date\"";
    std::string endTag = "</span>";
    size_t pos = 0;
    std::string extractedDate;
    std::string date1, date2;

    while ((pos = htmlContent.find(startTag, pos)) != std::string::npos) {
        pos = htmlContent.find('>', pos) + 1; // Move past the opening tag
        size_t endPos = htmlContent.find(endTag, pos);
        if (endPos != std::string::npos) {
            extractedDate = htmlContent.substr(pos, endPos - pos);
            // Store dates
            if (date1.empty()) {
                date1 = extractedDate;
            }
            else if (date2.empty()) {
                date2 = extractedDate;
                break; // Assume only two dates are needed
            }
        }
        pos = endPos + endTag.length(); // Move past the end tag
    }

    std::string localDate1 = ConvertUTCToLocal(date1);
    std::string localDate2 = ConvertUTCToLocal(date2);

    // Log Dates
    LogToFile("Start: " + localDate1);
    LogToFile("End: " + localDate2);

    // Construct the message to display
    std::string message = "Starting: " + localDate1 + "\nEnding: " + localDate2;

    // Convert the message to wide string for MessageBox
    std::wstring wideMessage(message.begin(), message.end());

    // Run the MessageBox in a new thread so it doesn't block the loop
    std::thread msgThread(ShowMessageBox, wideMessage);
    msgThread.detach();  // Detach the thread to avoid blocking

    // Clean up
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t* url = L"https://twitch.facepunch.com/"; // URL

    // Call periodic task on app start
    ReadWebPageHTML(url);

    while (true) {

        // Wait for 3 hours
        std::this_thread::sleep_for(std::chrono::hours(3));

        // Call periodic task if window does not exist
        HWND hwnd = FindWindow(NULL, L"Twitch Notification: Rust Drops Event!");
        if (hwnd == NULL) {
            ReadWebPageHTML(url);
        }

    }

    return 0;
}