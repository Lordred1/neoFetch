#include <stdlib.h>
#include <cstdlib>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <array>
#include <memory>
#include <WtsApi32.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

//#include <wdm.h>
//Steps
// 0.  make ascii art
// 1.  Get the windows version 
// 2.  Get the info at the side 
// 3.  Make the ascci art in a diffrent file to be customized
// 4.

std::string exec(const char* cmd) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) return "ERROR";
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
		result += buffer.data();
	return result;
}

std::string GetMemoryInfo() {
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	DWORDLONG totalPhys = statex.ullTotalPhys;
	DWORDLONG AvailPhys = statex.ullAvailPhys;
	// Convert to MB
	std::ostringstream oss;
	oss << "RAM: " << (AvailPhys / (1024 * 1024)) << " / " << (totalPhys / (1024 * 1024)) << " MB";
	return oss.str();
}

std::string GetEnvVar(const std::string& name) {
	char* value = nullptr;
	size_t len = 0;
	errno_t err = _dupenv_s(&value, &len, name.c_str());
	std::string result;

	if (err == 0 && value != nullptr) {
		result = value;
		free(value);
	}

	return result;
}

std::string GetPowerShellVersion() {
	std::string version = exec("powershell -Command \"$PSVersionTable.PSVersion.Build\"");
	version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
	return version;
}

std::string GetCmdbuild() {
	std::string version = exec("cmd /c ver");
	version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
	return version;
}

std::string getConsoleType() {
	if (!GetEnvVar("WT_SESSION").empty())
		return GetCmdbuild();
	if (!GetEnvVar("PSModulePath").empty())
		return GetPowerShellVersion();
	return "Gang it geeked";
}

std::string WideToUTF8(const wchar_t* src)
{
	if (!src) return "";
	int size = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) return "";

	std::string result(size - 1, '\0'); 
	WideCharToMultiByte(CP_UTF8, 0, src, -1, &result[0], size - 1, nullptr, nullptr);
	return result;
}
//Thank you chatGPT for this function you are my savior
std::string CleanWMICOutput(const std::string& input) {
	std::istringstream stream(input);
	std::string line;
	std::string result;

	while (std::getline(stream, line)) {
		line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
		
		line.erase(line.begin(), std::find_if(line.begin(), line.end(),
			[](unsigned char ch) { return !std::isspace(ch); }));
		
		line.erase(std::find_if(line.rbegin(), line.rend(),
			[](unsigned char ch) { return !std::isspace(ch); }).base(), line.end());

		std::string lowerLine = line;
		std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
		if (line.empty() || lowerLine == "name") continue;

		line = std::regex_replace(line, std::regex("\\s+"), " ");
		result = line; 
	}
	return result;
}


std::string GetTimes() {
	ULONGLONG uptimeMS = GetTickCount64();
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	ULONGLONG currentTime = (((ULONGLONG)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	ULONGLONG bootTime = currentTime - (uptimeMS * 10000);

	FILETIME bootFileTime;
	bootFileTime.dwLowDateTime = (DWORD)(bootTime & 0xFFFFFFFF);
	bootFileTime.dwHighDateTime = (DWORD)(bootTime >> 32);
	SYSTEMTIME stUTC;
	FileTimeToSystemTime(&bootFileTime, &stUTC);

	std::ostringstream oss;
	oss << stUTC.wDay << "-"
		<< stUTC.wMonth << "-"
		<< stUTC.wYear
		<< " at "
		<< stUTC.wHour << ":"
		<< stUTC.wMinute;
	return oss.str();
}

std::string Components(int CPU/*1 for cpu*/, int GPU/*1 for gpu*/) {
	if (CPU == 1){
		std::string CPU = exec("wmic cpu get name");
		return CleanWMICOutput(CPU);
	}
	if (GPU == 1){
		std::string GPU = exec("wmic path win32_VideoController get name");
		return CleanWMICOutput(GPU);
	}
}

std::string getScreenResolution() {
	SetProcessDPIAware();  // needed to deal with scaling as 125% is 1536x864 on 1080p
	std::string Resolutuion = std::to_string(GetSystemMetrics(SM_CXSCREEN)) + "x" + std::to_string(GetSystemMetrics(SM_CYSCREEN));
	return Resolutuion;
}

void ColorRotate(HANDLE hConsoleOUT, int Color_Row_Print/*0-8 for the first set */, int Color_Row_End /*8-16 for the first set */) { // how many colors per row to print
	char whitespace = '\x20';
	for (int i = Color_Row_Print; i < Color_Row_End; ++i) {
		SetConsoleTextAttribute(hConsoleOUT, i << 4);
		std::cout << whitespace;
	}
	SetConsoleTextAttribute(hConsoleOUT, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	std::cout << std::endl;
}

static int AsciiArt(const HANDLE hConsoleOUT, const std::string CurrentUserName, const std::string HostName) {
	std::string Boot_Time = GetTimes();
	int rowIndex = 0;
	std::ifstream file("ascii.conf");
	if (!file.is_open()) {
		std::cerr << "[*]Failed to open ascii art file\n[*]To Fix add a file caled 'ascii.conf' in the same folder as the exe" << '\n';
		return 1;
	}

	CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
	csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

	if(!GetConsoleScreenBufferInfoEx(hConsoleOUT, &csbiex)){
		std::cerr << "[*]Failed to get the console buffer info\n";
		return 1;
	}

	csbiex.ColorTable[3] = RGB(82, 224, 244);

	std::string lines;
	while (std::getline(file, lines)) {
		
		if (rowIndex < 7) {
			SetConsoleTextAttribute(hConsoleOUT, 0x1);//Dark Blue
		}
		else if (rowIndex < 15) {
			SetConsoleTextAttribute(hConsoleOUT, 3);//Cyan
		}

		std::cout << lines;
		WORD color = 0;
		if (rowIndex == 0) {
			std::cout << "\t\t\t" << CurrentUserName << "@" << HostName << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 1) {
			std::cout << "\t\tCPU: " << Components(1, 0) << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 2) {
			std::cout << "\t\tGPU: " << Components(0, 1) << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 3) {
			std::cout << "\t\tBoot Time: " << Boot_Time << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 4) {
			std::cout << "\t\tShell: " << getConsoleType() << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 5) {
			std::cout << "\t\t" << GetMemoryInfo() << '\n';
			rowIndex += 1;
			continue;
		}
		if (rowIndex == 6) {
			std::cout << "\t\tResolution: " << getScreenResolution() << '\n';
			rowIndex += 1;
			continue;
		}
		int Row = 0;
		if (rowIndex == 13 or rowIndex == 14) {
			if (rowIndex == 13) {
				std::cout << "\t\t";
				ColorRotate(hConsoleOUT, 0, 8);
				rowIndex += 1;
				continue;
			}
			if (rowIndex == 14) {
				std::cout << "\t\t";
				ColorRotate(hConsoleOUT, 8, 16);
				rowIndex += 1;
				continue;
			}
			
			continue;
		}
		std::cout << std::endl;
		rowIndex += 1;
	}
	SetConsoleTextAttribute(hConsoleOUT, 7);//normal terminal color
	file.close();
	return 0;
}


#pragma comment(lib, "Wtsapi32.lib")

int main() {
	DWORD Active_User = WTSGetActiveConsoleSessionId();
	LPTSTR pUserName = nullptr;
	LPTSTR pDomainName = nullptr;
	DWORD bytesReturned;
	std::wstring UserName;
	// query username of the active session
	if (WTSQuerySessionInformation(
		WTS_CURRENT_SERVER_HANDLE,
		Active_User,
		WTSUserName,
		&pUserName,
		&bytesReturned))
	{
		
		//std::wcout << L"User Name: " << pUserName << std::endl;
	}
	else
	{
		std::cerr << "[*]Failed to get user name" << std::endl;
	}
	WCHAR HostName[256];
	DWORD HostNameSize = sizeof(HostName) / sizeof(HostName[0]);
	std::string HostNameUTF8;
	if (GetComputerNameW(HostName, &HostNameSize)) {
		HostNameUTF8 = WideToUTF8(HostName);
		//std::cout << "DEBUG: HostNameUTF8='" << HostNameUTF8 << "'" << std::endl;
	}
	else {
		std::cerr << "[*]Failed to get host name" << std::endl;
	}
	std::string UserNameUTF8 = WideToUTF8(pUserName);
	WTSFreeMemory(pUserName);

	const HANDLE hConsoleOUT = GetStdHandle(STD_OUTPUT_HANDLE);

	AsciiArt(hConsoleOUT, UserNameUTF8, HostNameUTF8);
	

	//std::cout << "DEBUG: UsernameUTF8='" << UserNameUTF8 << "'" << std::endl;

}