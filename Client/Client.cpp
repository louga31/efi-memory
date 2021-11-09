#include <stdio.h>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include <tlhelp32.h>
#include "Driver.h"

bool CheckDriverStatus() {
	int icheck = 82;
	NTSTATUS status = 0;

	int checked = Driver::read<int>(GetCurrentProcessId(), (uintptr_t)&icheck, &status);
	if (checked != icheck) {
		return false;
	}

	uintptr_t BaseAddr = Driver::GetBaseAddress(GetCurrentProcessId());
	if (BaseAddr == 0) {
		return false;
	}

	return true;
}

DWORD GetProcessIdByName(wchar_t* name) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE) {
		while (Process32Next(snapshot, &entry) == TRUE) {
			if (_wcsicmp(entry.szExeFile, name) == 0) {
				return entry.th32ProcessID;
			}
		}
	}

	CloseHandle(snapshot);
	return 0;
}


int main()
{
	if (!Driver::initialize() || !CheckDriverStatus()) {
		UNICODE_STRING VariableName = RTL_CONSTANT_STRING(VARIABLE_NAME);
		myNtSetSystemEnvironmentValueEx(
			&VariableName,
			&DummyGuid,
			0,
			0,
			ATTRIBUTES);//delete var

		std::cout << "Couldn't connect to EFI Driver\n";
		system("pause");
		exit(1);
		return 1;
	}

	wchar_t name[] = { 'd', 'i', 's', 'c', 'o', 'r', 'd', '.', 'e', 'x', 'e', 0 };
	DWORD pid = GetProcessIdByName(name);
	uintptr_t BaseAddr = Driver::GetBaseAddress(pid);

	while (true) {
		system("cls");
		std::cout << "Hi Welcome to EFI Client\n";
		std::cout << "What do you want to do?\n";
		std::cout << "1 - Get process base address by PID\n";
		std::cout << "2 - Read process memory by PID\n";
		std::cout << "3 - Read process memory by Offset\n";
		std::cout << "4 - Read process memory by Name and Offset\n";
		std::cout << "5 - Write process memory by PID\n";
		std::cout << "6 - Write process memory by Name and Offset\n";
		std::cout << "7 - Exit\n";
		int action;
		std::cin >> action;
		std::cin.clear();
		std::cin.ignore();
		if (action == 7) {
			std::cout << "Exiting Byee!\n";
			return 0;
		}

		if (action == 1 || action == 2 || action == 5) {
			std::cout << "Process ID:\n";
			std::cin >> pid;
			std::cin.clear();
			std::cin.ignore();
		}

		if (action == 4 || action == 6) {
			std::cout << "Process Name:\n";
			std::string name;
			std::cin >> name;
			pid = GetProcessIdByName((wchar_t*)std::wstring(name.begin(), name.end()).c_str());
			BaseAddr = Driver::GetBaseAddress(pid);
			std::cin.clear();
			std::cin.ignore();
		}

		if (action == 1) {
			uintptr_t BaseAddr = Driver::GetBaseAddress(pid);
			std::cout << "Base Address:\n" << std::hex << BaseAddr << "\n";
			system("pause");
		}
		else if (action == 2) {
			std::cout << "Address(Hex):\n";
			uintptr_t addr = 0;
			std::string addrData;
			std::cin >> addrData;
			std::cin.clear();
			std::cin.ignore();
			addr = std::stoull(addrData, nullptr, 16);
			std::cout << "Number of bytes:\n";
			size_t bytes;
			std::cin >> bytes;
			std::cin.clear();
			std::cin.ignore();

			BYTE* buffer = new BYTE[bytes];
			memset(buffer, 0, bytes);
			Driver::read_memory(pid, addr, (uintptr_t)&buffer[0], bytes);

			std::cout << "Readed:\n";
			for (size_t i = 0; i < bytes; i++) {
				printf("%02X ", buffer[i]);
			}
			printf("\n\n");

			delete[] buffer;

			system("pause");
		}
		else if (action == 5) {
			std::cout << "Address(Hex):\n";
			uintptr_t addr = 0;
			std::string addrData;
			std::cin >> addrData;
			std::cin.clear();
			std::cin.ignore();
			addr = std::stoull(addrData, nullptr, 16);
			std::cout << "Enter data:\n";
			int data;
			std::cin >> data;
			std::cin.clear();
			std::cin.ignore();

			Driver::write<int>(pid, addr, data, NULL);

			BYTE* buffer = new BYTE[sizeof(int)];
			memset(buffer, 0, sizeof(int));
			Driver::read_memory(pid, addr, (uintptr_t)&buffer[0], sizeof(int));
			std::cout << "Readed:\n";
			for (size_t i = 0; i < sizeof(int); i++) {
				printf("%02X ", buffer[i]);
			}
			printf("\n\n");

			delete[] buffer;

			system("pause");
		}
		else if (action == 3 || action == 4) {
			std::cout << "Offset(Hex):\n";
			uintptr_t offset = 0;
			std::string offsetData;
			std::cin >> offsetData;
			std::cin.clear();
			std::cin.ignore();
			offset = std::stoull(offsetData, nullptr, 16);
			std::cout << "Number of bytes:\n";
			size_t bytes;
			std::cin >> bytes;
			std::cin.clear();
			std::cin.ignore();

			BYTE* buffer = new BYTE[bytes];
			memset(buffer, 0, bytes);
			Driver::read_memory(pid, BaseAddr + offset, (uintptr_t)&buffer[0], bytes);

			std::cout << "Readed:\n";
			for (size_t i = 0; i < bytes; i++) {
				printf("%02X ", buffer[i]);
			}
			printf("\n\n");

			delete[] buffer;

			system("pause");
		}
	}
}