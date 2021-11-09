#include <efi.h>
#include <efilib.h>
#include "dummy.h"
#include "protocol.h"

// Title
#define BOOTKIT_TITLE1		L"\n ██╗      ██████╗ ██╗   ██╗ ██████╗  █████╗ ██████╗  ██╗ " \
							L"\n ██║     ██╔═══██╗██║   ██║██╔════╝ ██╔══██╗╚════██╗███║ " \
							L"\n ██║     ██║   ██║██║   ██║██║  ███╗███████║ █████╔╝╚██║ "
#define BOOTKIT_TITLE2		L"\n ██║     ██║   ██║██║   ██║██║   ██║██╔══██║ ╚═══██╗ ██║ " \
							L"\n ███████╗╚██████╔╝╚██████╔╝╚██████╔╝██║  ██║██████╔╝ ██║ " \
							L"\n ╚══════╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═════╝  ╚═╝ \n"

#define GNU_EFI_USE_MS_ABI 1
#define MicrosoftCallingType __attribute__((ms_abi))

// Functions (Windows only)
typedef int (MicrosoftCallingType* PsLookupProcessByProcessId)(
	void* ProcessId,
	void* OutPEProcess
	);
typedef void* (MicrosoftCallingType* PsGetProcessSectionBaseAddress)(
	void* PEProcess
	);
typedef int (MicrosoftCallingType* MmCopyVirtualMemory)(
	void* SourceProcess,
	void* SourceAddress,
	void* TargetProcess,
	void* TargetAddress,
	ptr64 BufferSize,
	char PreviousMode,
	void* ReturnSize
	);

// Our protocol GUID (should be different for every driver)
static const EFI_GUID ProtocolGuid
= { 0x2f84893e, 0xfd5e, 0x2038, {0x8d, 0x9e, 0x20, 0xa7, 0xaf, 0x9c, 0x32, 0xf1} };

// VirtualAddressMap GUID (gEfiEventVirtualAddressChangeGuid)
static const EFI_GUID VirtualGuid
= { 0x13FA7698, 0xC831, 0x49C7, { 0x87, 0xEA, 0x8F, 0x43, 0xFC, 0xC2, 0x51, 0x96 } }; //we will remove later shouldn't be important

// ExitBootServices GUID (gEfiEventExitBootServicesGuid)
static const EFI_GUID ExitGuid
= { 0x27ABF055, 0xB1B8, 0x4C26, { 0x80, 0x48, 0x74, 0x8F, 0x37, 0xBA, 0xA2, 0xDF } }; //we will remove later shouldn't be important

// Dummy protocol struct
typedef struct _DummyProtocalData {
	UINTN blank;
} DummyProtocalData;


// Pointers to original functions
static EFI_SET_VARIABLE oSetVariable = NULL;

// Global declarations
static EFI_EVENT NotifyEvent = NULL;
static EFI_EVENT ExitEvent = NULL;
static BOOLEAN Virtual = FALSE;
static BOOLEAN Runtime = FALSE;

static PsLookupProcessByProcessId GetProcessByPid = (PsLookupProcessByProcessId)0;
static PsGetProcessSectionBaseAddress GetBaseAddress = (PsGetProcessSectionBaseAddress)0;
static MmCopyVirtualMemory MCopyVirtualMemory = (MmCopyVirtualMemory)0;

// Function that actually performs the r/w
EFI_STATUS
RunCommand(MemoryCommand* cmd)
{
	// Check if the command has right magic
	// (just to be sure again)
	if (cmd->magic != COMMAND_MAGIC)
	{
		return EFI_ACCESS_DENIED;
	}

	// Copy operation
	if (cmd->operation == COPY_OPERATION)
	{
		void* src_process_id = (void*)cmd->data[0];
		void* src_address = (void*)cmd->data[1];
		void* dest_process_id = (void*)cmd->data[2];
		void* dest_address = (void*)cmd->data[3];
		ptr64 size = cmd->data[4];
		void* resultAddr = (void*)cmd->data[5];

		if (src_process_id == DIRECT_COPY) {
			// Same as memcpy function
			CopyMem(dest_address, src_address, size);
		}
		else {
			void* SrcProc = 0;
			void* DstProc = 0;
			ptr64 size_out = 0;
			int status = 0;

			status = GetProcessByPid(src_process_id, &SrcProc);
			if (status < 0) {
				*(ptr64*)resultAddr = status;
				return EFI_SUCCESS;
			}

			status = GetProcessByPid(dest_process_id, &DstProc);
			if (status < 0) {
				*(ptr64*)resultAddr = status;
				return EFI_SUCCESS;
			}


			*(ptr64*)resultAddr = MCopyVirtualMemory(SrcProc, src_address, DstProc, dest_address, size, 1, &size_out);
		}
		return EFI_SUCCESS;
	}

	if (cmd->operation == SETUP_OPERATION)
	{
		GetProcessByPid = (PsLookupProcessByProcessId)cmd->data[0];
		GetBaseAddress = (PsGetProcessSectionBaseAddress)cmd->data[1];
		MCopyVirtualMemory = (MmCopyVirtualMemory)cmd->data[2];
		ptr64 resultAddr = cmd->data[3];
		*(ptr64*)resultAddr = 1;
		return EFI_SUCCESS;
	}

	//Get Process Base Address
	if (cmd->operation == GET_PROCESS_BASE_ADDRESS_OPERATION)
	{
		void* pid = (void*)cmd->data[0];
		void* resultAddr = (void*)cmd->data[1];
		void* ProcessPtr = 0;

		//Find process by ID
		if (GetProcessByPid(pid, &ProcessPtr) < 0 || ProcessPtr == 0) {
			*(ptr64*)resultAddr = 0; // Process not found
			return EFI_SUCCESS;
		}

		//Find process Base Address
		*(ptr64*)resultAddr = (ptr64)GetBaseAddress(ProcessPtr); //Return Base Address
		return EFI_SUCCESS;
	}

	// Invalid command
	return EFI_UNSUPPORTED;
}

// Hooked EFI function SetVariable()
// Can be called from Windows with NtSetSystemEnvironmentValueEx
EFI_STATUS
EFIAPI
HookedSetVariable(
	IN CHAR16* VariableName,
	IN EFI_GUID* VendorGuid,
	IN UINT32 Attributes,
	IN UINTN DataSize,
	IN VOID* Data
)
{
	// Use our hook only after we are in virtual address-space
	if (Virtual && Runtime)
	{
		// Check of input is not null
		if (VariableName != NULL && VariableName[0] != CHAR_NULL && VendorGuid != NULL)
		{
			// Check if variable name is same as our declared one
			// this is used to check if call is really from our program
			// running in the OS (client)
			if (StrnCmp(VariableName, VARIABLE_NAME,
				(sizeof(VARIABLE_NAME) / sizeof(CHAR16)) - 1) == 0)
			{
				if (DataSize == 0 && Data == NULL)
				{
					// Skip no data
					return EFI_SUCCESS;
				}

				// Check if the data size is correct
				if (DataSize == sizeof(MemoryCommand))
				{
					// We did it!
					// Now we can call the magic function
					return RunCommand((MemoryCommand*)Data);
				}
			}
		}
	}

	// Call the original SetVariable() function
	return oSetVariable(VariableName, VendorGuid, Attributes, DataSize, Data);
}

// Event callback when SetVitualAddressMap() is called by OS
VOID
EFIAPI
SetVirtualAddressMapEvent(
	IN EFI_EVENT Event,
	IN VOID* Context
)
{
	// Convert orignal SetVariable address
	RT->ConvertPointer(0, (void**)&oSetVariable);

	// Convert all other addresses
	RT->ConvertPointer(0, (void**)&oGetTime);
	RT->ConvertPointer(0, (void**)&oSetTime);
	RT->ConvertPointer(0, (void**)&oGetWakeupTime);
	RT->ConvertPointer(0, (void**)&oSetWakeupTime);
	RT->ConvertPointer(0, (void**)&oSetVirtualAddressMap);
	RT->ConvertPointer(0, (void**)&oConvertPointer);
	//RT->ConvertPointer(0, (void**)&oGetVariable);
	//RT->ConvertPointer(0, (void**)&oGetNextVariableName);
	//RT->ConvertPointer(0, (void**)&oSetVariable);
	RT->ConvertPointer(0, (void**)&oGetNextHighMonotonicCount);
	RT->ConvertPointer(0, (void**)&oResetSystem);
	RT->ConvertPointer(0, (void**)&oUpdateCapsule);
	RT->ConvertPointer(0, (void**)&oQueryCapsuleCapabilities);
	RT->ConvertPointer(0, (void**)&oQueryVariableInfo);

	// Convert runtime services pointer
	RtLibEnableVirtualMappings();

	// Null and close the event so it does not get called again
	NotifyEvent = NULL;

	// We are now working in virtual address-space
	Virtual = TRUE;
}

// Event callback after boot process is started
VOID
EFIAPI
ExitBootServicesEvent(
	IN EFI_EVENT Event,
	IN VOID* Context
)
{
	// This event is called only once so close it
	BS->CloseEvent(ExitEvent);
	ExitEvent = NULL;

	// Boot services are now not avaible
	BS = NULL;

	// We are booting the OS now
	Runtime = TRUE;

	// Print some text so we know it works (300iq)
	ST->ConOut->SetAttribute(ST->ConOut, EFI_WHITE | EFI_BACKGROUND_GREEN);
	ST->ConOut->ClearScreen(ST->ConOut);
	Print(L"Driver seems to be working as expected! Windows is booting now...\n");
}

// Replaces service table pointer with desired one
// returns original
VOID*
SetServicePointer(
	IN OUT EFI_TABLE_HEADER* ServiceTableHeader,
	IN OUT VOID** ServiceTableFunction,
	IN VOID* NewFunction
)
{
	// We don't want to fuck up the system
	if (ServiceTableFunction == NULL || NewFunction == NULL)
		return NULL;

	// Make sure boot services pointers are not null
	ASSERT(BS != NULL);
	ASSERT(BS->CalculateCrc32 != NULL);

	// Raise task priority level
	CONST EFI_TPL Tpl = BS->RaiseTPL(TPL_HIGH_LEVEL);

	// Swap the pointers
	// GNU-EFI and InterlockedCompareExchangePointer 
	// are not friends
	VOID* OriginalFunction = *ServiceTableFunction;
	*ServiceTableFunction = NewFunction;

	// Change the table CRC32 signature
	ServiceTableHeader->CRC32 = 0;
	BS->CalculateCrc32((UINT8*)ServiceTableHeader, ServiceTableHeader->HeaderSize, &ServiceTableHeader->CRC32);

	// Restore task priority level
	BS->RestoreTPL(Tpl);

	return OriginalFunction;
}

// EFI driver unload routine
static
EFI_STATUS
EFI_FUNCTION
efi_unload(IN EFI_HANDLE ImageHandle)
{
	// We don't want our driver to be unloaded 
	// until complete reboot
	return EFI_ACCESS_DENIED;
}

// EFI entry point
EFI_STATUS
efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable)
{
	// Initialize internal GNU-EFI functions
	InitializeLib(ImageHandle, SystemTable);

	//
	// Clear screen
	//
	gST->ConOut->ClearScreen(gST->ConOut);

	//
	// Print title
	//
	gST->ConOut->SetAttribute(gST->ConOut, EFI_YELLOW | EFI_BACKGROUND_BLACK);
	Print(L"\n\n");
	Print(L"%s", BOOTKIT_TITLE1);
	Print(L"%s", BOOTKIT_TITLE2);

	gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);

	// Get handle to this image
	EFI_LOADED_IMAGE* LoadedImage = NULL;
	EFI_STATUS status = BS->OpenProtocol(ImageHandle, &LoadedImageProtocol,
		(void**)&LoadedImage, ImageHandle,
		NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

	// Return if protocol failed to open
	if (EFI_ERROR(status))
	{
		gST->ConOut->SetAttribute(gST->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
		Print(L"Can't open protocol: %d\n", status);
		return status;
	}

	//Randomize protocol GUID
	EFI_TIME time = { 0 };
	SetMem((void*)&time, sizeof(EFI_TIME), 0);
	gRT->GetTime(&time, NULL);
	ptr64 num = time.Nanosecond + time.Second;
	if (num == 0) {
		num = (ptr64)&ProtocolGuid;
	}
	unsigned char* gdata = (unsigned char*)&ProtocolGuid;
	for (int i = 0; i < 16; i++) {
		gdata[i] = (unsigned char)(num * gdata[i]);
	}

	gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
	Print(L"GUID: %g\n", ProtocolGuid);

	// Install our protocol interface
	// This is needed to keep our driver loaded
	DummyProtocalData dummy = { 0 };
	status = LibInstallProtocolInterfaces(
		&ImageHandle, &ProtocolGuid,
		&dummy, NULL);

	// Return if interface failed to register
	if (EFI_ERROR(status))
	{
		gST->ConOut->SetAttribute(gST->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
		Print(L"Can't register interface: %d\n", status);
		return status;
	}

	// Set our image unload routine
	LoadedImage->Unload = (EFI_IMAGE_UNLOAD)efi_unload;

	// Create global event for VirtualAddressMap
	status = BS->CreateEventEx(EVT_NOTIFY_SIGNAL,
		TPL_NOTIFY,
		SetVirtualAddressMapEvent,
		NULL,
		&VirtualGuid,
		&NotifyEvent);

	// Return if event create failed
	if (EFI_ERROR(status))
	{
		gST->ConOut->SetAttribute(gST->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
		Print(L"Can't create event (SetVirtualAddressMapEvent): %d\n", status);
		return status;
	}

	// Create global event for ExitBootServices
	status = BS->CreateEventEx(EVT_NOTIFY_SIGNAL,
		TPL_NOTIFY,
		ExitBootServicesEvent,
		NULL,
		&ExitGuid,
		&ExitEvent);

	// Return if event create failed (yet again)
	if (EFI_ERROR(status))
	{
		gST->ConOut->SetAttribute(gST->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
		Print(L"Can't create event (ExitBootServicesEvent): %d\n", status);
		return status;
	}

	// Hook SetVariable (should not fail)
	oSetVariable = (EFI_SET_VARIABLE)SetServicePointer(&RT->Hdr, (VOID**)&RT->SetVariable, (VOID**)&HookedSetVariable);
#ifdef _DEBUG
	Print(L"Hooked SetVariable: 0x%x -> 0x%x\n", (VOID*)oSetVariable, (VOID*)&HookedSetVariable);
#endif // _DEBUG

	// Hook all the other runtime services functions
	oGetTime = (EFI_GET_TIME)SetServicePointer(&RT->Hdr, (VOID**)&RT->GetTime, (VOID**)&HookedGetTime);
#ifdef _DEBUG
	Print(L"Hooked GetTime: 0x%x -> 0x%x\n", (VOID*)oGetTime, (VOID*)&HookedGetTime);
#endif // _DEBUG

	oSetTime = (EFI_SET_TIME)SetServicePointer(&RT->Hdr, (VOID**)&RT->SetTime, (VOID**)&HookedSetTime);
#ifdef _DEBUG
	Print(L"Hooked SetTime: 0x%x -> 0x%x\n", (VOID*)oSetTime, (VOID*)&HookedSetTime);
#endif // _DEBUG

	oGetWakeupTime = (EFI_GET_WAKEUP_TIME)SetServicePointer(&RT->Hdr, (VOID**)&RT->GetWakeupTime, (VOID**)&HookedGetWakeupTime);
#ifdef _DEBUG
	Print(L"Hooked GetWakeupTime: 0x%x -> 0x%x\n", (VOID*)oGetWakeupTime, (VOID*)&HookedGetWakeupTime);
#endif // _DEBUG

	oSetWakeupTime = (EFI_SET_WAKEUP_TIME)SetServicePointer(&RT->Hdr, (VOID**)&RT->SetWakeupTime, (VOID**)&HookedSetWakeupTime);
#ifdef _DEBUG
	Print(L"Hooked SetWakeupTime: 0x%x -> 0x%x\n", (VOID*)oSetWakeupTime, (VOID*)&HookedSetWakeupTime);
#endif // _DEBUG

	oSetVirtualAddressMap = (EFI_SET_VIRTUAL_ADDRESS_MAP)SetServicePointer(&RT->Hdr, (VOID**)&RT->SetVirtualAddressMap, (VOID**)&HookedSetVirtualAddressMap);
#ifdef _DEBUG
	Print(L"Hooked SetVirtualAddressMap: 0x%x -> 0x%x\n", (VOID*)oSetVirtualAddressMap, (VOID*)&HookedSetVirtualAddressMap);
#endif // _DEBUG

	oConvertPointer = (EFI_CONVERT_POINTER)SetServicePointer(&RT->Hdr, (VOID**)&RT->ConvertPointer, (VOID**)&HookedConvertPointer);
#ifdef _DEBUG
	Print(L"Hooked ConvertPointer: 0x%x -> 0x%x\n", (VOID*)oConvertPointer, (VOID*)&HookedConvertPointer);
#endif // _DEBUG

//	oGetVariable = (EFI_GET_VARIABLE)SetServicePointer(&RT->Hdr, (VOID**)&RT->GetVariable, (VOID**)&HookedGetVariable);
//#ifdef _DEBUG
//	Print(L"Hooked GetVariable: 0x%x -> 0x%x\n", (VOID*)oGetVariable, (VOID*)&HookedGetVariable);
//#endif // _DEBUG
//
//	oGetNextVariableName = (EFI_GET_NEXT_VARIABLE_NAME)SetServicePointer(&RT->Hdr, (VOID**)&RT->GetNextVariableName, (VOID**)&HookedGetNextVariableName);
//#ifdef _DEBUG
//	Print(L"Hooked GetNextVariableName: 0x%x -> 0x%x\n", (VOID*)oGetNextVariableName, (VOID*)&HookedGetNextVariableName);
//#endif // _DEBUG
	 
	//oSetVariable = (EFI_SET_VARIABLE)SetServicePointer(&RT->Hdr, (VOID**)&RT->SetVariable, (VOID**)&HookedSetVariable);
//#ifdef _DEBUG
	//Print(L"Hooked SetVariable: 0x%x -> 0x%x\n", (VOID*)oSetVariable, (VOID*)&HookedSetVariable);
//#endif // _DEBUG

	oGetNextHighMonotonicCount = (EFI_GET_NEXT_HIGH_MONO_COUNT)SetServicePointer(&RT->Hdr, (VOID**)&RT->GetNextHighMonotonicCount, (VOID**)&HookedGetNextHighMonotonicCount);
#ifdef _DEBUG
	Print(L"Hooked GetNextHighMonotonicCount: 0x%x -> 0x%x\n", (VOID*)oGetNextHighMonotonicCount, (VOID*)&HookedGetNextHighMonotonicCount);
#endif // _DEBUG

	oResetSystem = (EFI_RESET_SYSTEM)SetServicePointer(&RT->Hdr, (VOID**)&RT->ResetSystem, (VOID**)&HookedResetSystem);
#ifdef _DEBUG
	Print(L"Hooked ResetSystem: 0x%x -> 0x%x\n", (VOID*)oResetSystem, (VOID*)&HookedResetSystem);
#endif // _DEBUG

	oUpdateCapsule = (EFI_UPDATE_CAPSULE)SetServicePointer(&RT->Hdr, (VOID**)&RT->UpdateCapsule, (VOID**)&HookedUpdateCapsule);
#ifdef _DEBUG
	Print(L"Hooked UpdateCapsule: 0x%x -> 0x%x\n", (VOID*)oUpdateCapsule, (VOID*)&HookedUpdateCapsule);
#endif // _DEBUG

	oQueryCapsuleCapabilities = (EFI_QUERY_CAPSULE_CAPABILITIES)SetServicePointer(&RT->Hdr, (VOID**)&RT->QueryCapsuleCapabilities, (VOID**)&HookedQueryCapsuleCapabilities);
#ifdef _DEBUG
	Print(L"Hooked QueryCapsuleCapabilities: 0x%x -> 0x%x\n", (VOID*)oQueryCapsuleCapabilities, (VOID*)&HookedQueryCapsuleCapabilities);
#endif // _DEBUG

	oQueryVariableInfo = (EFI_QUERY_VARIABLE_INFO)SetServicePointer(&RT->Hdr, (VOID**)&RT->QueryVariableInfo, (VOID**)&HookedQueryVariableInfo);
#ifdef _DEBUG
	Print(L"Hooked QueryVariableInfo: 0x%x -> 0x%x\n", (VOID*)oQueryVariableInfo, (VOID*)&HookedQueryVariableInfo);
#endif // _DEBUG

	// Print confirmation text
	gST->ConOut->SetAttribute(gST->ConOut, EFI_GREEN | EFI_BACKGROUND_BLACK);
	Print(L"Driver has been loaded successfully. You can now boot to the OS.\n");
	Print(L"If you don't see a green screen while booting disable Secure Boot!.\n");
	return EFI_SUCCESS;
}