#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"
#include "Helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

// Utils //

namespace GameProcessUtils
{
	std::vector<processEntry> GetRunningProcessList()
	{
		HANDLE hProcessSnap;
		PROCESSENTRY32 pe32{ 0 };
		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		std::vector<processEntry> processList;

		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			pe32.dwSize = sizeof(PROCESSENTRY32);
			if (Process32First(hProcessSnap, &pe32)) {
				processList.push_back({
					.name = pe32.szExeFile,
					.pid = pe32.th32ProcessID
					});

				while (Process32Next(hProcessSnap, &pe32)) {
					processList.push_back({
						.name = pe32.szExeFile,
						.pid = pe32.th32ProcessID
						});
				}
			}

			CloseHandle(hProcessSnap);
		}
		return processList;
	};
};


// -- Private methods  -- //

GameProcessErrcode_ GameProcess::AttachToNamedProcess(const char* processName, DWORD processExtraFlags)
{
	DWORD pid = GetGamePID(processName);

	if (pid == (DWORD)-1) {
		return GameProcessErrcode_PROC_NOT_FOUND;
	}
	else {
		m_processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | processExtraFlags, FALSE, pid);
		if (m_processHandle != nullptr)
		{
			if (LoadGameMainModule(processName))
			{
				processId = pid;
				return GameProcessErrcode_PROC_ATTACHED;
			}
			CloseHandle(m_processHandle);
		}
		return GameProcessErrcode_PROC_ATTACH_ERR;
	}
}


DWORD GameProcess::GetGamePID(const char* processName)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32{ 0 };
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hProcessSnap, &pe32)) {
			if (strcmp(pe32.szExeFile, processName) == 0) {
				m_pid = pe32.th32ProcessID;
				CloseHandle(hProcessSnap);
				return m_pid;
			}

			while (Process32Next(hProcessSnap, &pe32)) {
				if (strcmp(pe32.szExeFile, processName) == 0) {
					m_pid = pe32.th32ProcessID;
					CloseHandle(hProcessSnap);
					return m_pid;
				}
			}
		}
		CloseHandle(hProcessSnap);
	}

	return (DWORD)-1;
}

std::vector<moduleEntry> GameProcess::GetModuleList()
{
	HANDLE moduleSnap;
	MODULEENTRY32 me32{ 0 };

	std::vector<moduleEntry> modules;

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {
			modules.push_back({
				.name = std::string(me32.szModule),
				.address = (gameAddr)me32.modBaseAddr,
				.size = (uint64_t)me32.modBaseSize
			});

			while (Module32Next(moduleSnap, &me32)) {
				modules.push_back({
					.name = std::string(me32.szModule),
					.address = (gameAddr)me32.modBaseAddr,
					.size = (uint64_t)me32.modBaseSize
				});
			}
		}
	}

	CloseHandle(moduleSnap);
	return modules;
}

bool GameProcess::LoadGameMainModule(const char* processName)
{
	std::vector<moduleEntry> modules = GetModuleList();

	for (auto& module : modules)
	{
		if (module.name == processName)
		{
			moduleAddr = module.address;
			moduleSize = module.size;
		}
	}

	return moduleAddr != -1;
}

// -- Public methods -- //

bool GameProcess::Attach(const char* processName, DWORD processExtraFlags)
{
	if (status == GameProcessErrcode_PROC_ATTACHED) {
		return false;
	}

	allocatedMemory.clear();
	m_toFree.clear();
	status = AttachToNamedProcess(processName, processExtraFlags);
	return status == GameProcessErrcode_PROC_ATTACHED;
}

void GameProcess::Detach()
{
	if (m_processHandle != nullptr) {
		CloseHandle(m_processHandle);
		m_processHandle = nullptr;
	}
	status = GameProcessErrcode_PROC_NOT_ATTACHED;
}

bool GameProcess::IsAttached()
{
	return status == GameProcessErrcode_PROC_ATTACHED;
}

bool GameProcess::CheckRunning()
{
	if (m_processHandle != nullptr)
	{
		int32_t value = 0;
		if (ReadProcessMemory(m_processHandle, (LPCVOID)moduleAddr, (LPVOID)&value, 4, nullptr) == 0)
		{
			DEBUG_LOG("! CheckRunning() failed: Process not running anymore !\n");
			Detach();
			status = GameProcessErrcode_PROC_EXITED;
			return false;
		}
		return true;
	}
	return false;
}

void GameProcess::FreeOldGameMemory(bool instant)
{
	uint64_t currentDate = Helpers::getCurrentTimestamp();
	for (int i = 0; i < m_toFree.size();)
	{
		auto& [date, targetAddr] = m_toFree[i];
		if (instant || ((currentDate - date) >= GAME_FREEING_DELAY_SEC))
		{
			VirtualFreeEx(m_processHandle, (LPVOID)targetAddr, 0, MEM_RELEASE);
			m_toFree.erase(m_toFree.begin() + i, m_toFree.begin() + i + 1);
			DEBUG_LOG("Freeing game memory %llx after delay\n", targetAddr);
		}
		else {
			++i;
		}
	}
}

int8_t GameProcess::readInt8(gameAddr addr)
{
	int8_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

int16_t GameProcess::readInt16(gameAddr addr)
{
	int16_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

int32_t GameProcess::readInt32(gameAddr addr)
{
	int32_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

int64_t GameProcess::readInt64(gameAddr addr)
{
	int64_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

uint8_t GameProcess::readUInt8(gameAddr addr)
{
	uint8_t value{ (uint8_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

uint16_t GameProcess::readUInt16(gameAddr addr)
{
	uint16_t value{ (uint16_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

uint32_t GameProcess::readUInt32(gameAddr addr)
{
	uint32_t value{ (uint32_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

uint64_t GameProcess::readUInt64(gameAddr addr)
{
	uint64_t value{ (uint64_t)- 1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

float GameProcess::readFloat(gameAddr addr)
{
	float value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

void GameProcess::readBytes(gameAddr addr, void* buf, size_t readSize)
{
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)buf, readSize, nullptr);
}


void GameProcess::writeInt8(gameAddr addr, int8_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 1, nullptr);
}

void  GameProcess::writeInt16(gameAddr addr, int16_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 2, nullptr);
}

void  GameProcess::writeInt32(gameAddr addr, int32_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeInt64(gameAddr addr, int64_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 8, nullptr);
}

void GameProcess::writeUInt8(gameAddr addr, uint8_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 1, nullptr);
}

void  GameProcess::writeUInt16(gameAddr addr, uint16_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 2, nullptr);
}

void  GameProcess::writeUInt32(gameAddr addr, uint32_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeUInt64(gameAddr addr, uint64_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 8, nullptr);
}

void  GameProcess::writeFloat(gameAddr addr, float value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeBytes(gameAddr addr, void* buf, size_t bufSize)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)buf, bufSize, nullptr);
}


gameAddr GameProcess::allocateMem(size_t amount)
{
	gameAddr allocatedBlock = (gameAddr)VirtualAllocEx(m_processHandle, nullptr, amount, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (allocatedBlock != 0) {
		allocatedMemory.push_back(std::pair<gameAddr, uint64_t>(allocatedBlock, amount));
		DEBUG_LOG("Allocated to game memory from : %llx to %llx\n", allocatedBlock, allocatedBlock + amount);
	}
	else {
		DEBUG_LOG("Allocation failure: attempt to allocate %lld (%llx) bytes\n", amount, amount);
	}

	return allocatedBlock;
}

void GameProcess::freeMem(gameAddr targetAddr)
{
	for (std::pair<gameAddr, uint64_t>& block : allocatedMemory) {
		if (block.first == (gameAddr)targetAddr)
		{
			allocatedMemory.erase(std::find(allocatedMemory.begin(), allocatedMemory.end(), block));
			DEBUG_LOG("Queuing freeing of allocated block %llx (size is %lld)\n", block.first, block.second);
			uint64_t currentDate = Helpers::getCurrentTimestamp();
			m_toFree.push_back({ currentDate, targetAddr });
			return;
		}
	}

	DEBUG_LOG("Freeing of allocated block %llx (not size found)\n", targetAddr);
	VirtualFreeEx(m_processHandle, (LPVOID)targetAddr, 0, MEM_RELEASE);
}

GameProcessThreadCreation_ GameProcess::createRemoteThread(gameAddr startAddress, uint64_t argument, bool waitEnd, int32_t* exitCodeThread)
{
	auto hThread = CreateRemoteThread(m_processHandle, nullptr, 0, (LPTHREAD_START_ROUTINE)startAddress, (PVOID)argument, 0, nullptr);

	bool result = hThread != nullptr;
	DEBUG_LOG("CreateRemoteThread at %llx, arg is %llx ; success = %d\n", startAddress, argument, result);
	if (hThread != nullptr && waitEnd) {
		switch (WaitForSingleObject(hThread, 10000))
		{
		case WAIT_TIMEOUT:
			DEBUG_LOG("Thread: not waiting for more than 10secs for it to end.\n");
			return GameProcessThreadCreation_WaitTimeout;
		default:
			DEBUG_LOG("Thread finished.\n");
			if (exitCodeThread != nullptr) {
				GetExitCodeThread(hThread, (DWORD*)exitCodeThread);
			}
			return GameProcessThreadCreation_FinishedCleanly;
		}
	}
	return GameProcessThreadCreation_Error;
}

bool GameProcess::InjectDll(const wchar_t* fullpath)
{
	{
		// Get .dll name only (including extension)
		std::string dllName;
		{
			std::wstring w_dllName = fullpath;
			if (w_dllName.find_last_of(L"/\\") != std::wstring::npos) {
				w_dllName.erase(0, w_dllName.find_last_of(L"/\\") + 1);
			}
			dllName = std::string(w_dllName.begin(), w_dllName.end());
		}

		// Check if the DLL is not already loaded
		for (auto& module : GetModuleList())
		{
			if (module.name == dllName) {
				DEBUG_LOG("-- DLL '%s' already injected, not re-injecting. --\n", dllName.c_str());
				return true;
			}
		}
	}

	DEBUG_LOG("-- Injecting DLL %S --\n", fullpath);
	// Allocate space for dll path in process memory
	int fullpathSize = (wcslen(fullpath) + 1) * (int)sizeof(WCHAR);
	gameAddr bufferAddr = allocateMem(fullpathSize);

	if (bufferAddr == 0) {
		DEBUG_LOG("DLL injection failure: tried to allocated %d, got nullptr from alloc.\n", fullpathSize);
		return false;
	}
	// Write path in process memory
	writeBytes(bufferAddr, (void*)fullpath, fullpathSize);

	// Get address of LoadLibraryW
	auto moduleHandle = GetModuleHandleA(TEXT("Kernel32"));
	if (moduleHandle == 0) {
		DEBUG_LOG("Failed getting the module handle of Kernel32.\n");
		return false;
	}
	PTHREAD_START_ROUTINE threatStartRoutineAddress = (PTHREAD_START_ROUTINE)GetProcAddress(moduleHandle, "LoadLibraryW");

	// Start thread in remote process
	auto errcode = createRemoteThread((gameAddr)threatStartRoutineAddress, bufferAddr, true);
	if (errcode != GameProcessThreadCreation_WaitTimeout) {
		// Only free if the thread is actually finished
		// Yes, this means a potential memory leak if the thread takes more than 10sec
		// Todo: if WaitTimeout, remember the thread handle and regularly check if it is still ongoing
		// Not really important here though
		freeMem(bufferAddr);
	}

	return errcode != GameProcessThreadCreation_Error;
}