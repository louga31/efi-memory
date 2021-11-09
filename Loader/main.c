/*
 * UEFI:SIMPLE - UEFI development made easy
 * Copyright ©️ 2014-2021 Pete Batard <pete@akeo.ie> - Public Domain
 * See COPYING for the full licensing terms.
 */
#include <efi.h>
#include <efilib.h>
#include <libsmbios.h>

#define ARRAY_SIZE(Array) (sizeof (Array) / sizeof ((Array)[0]))

//
// Paths to the driver to try
//
#define DRIVER_FILENAME		L"Driver.efi"
STATIC CHAR16* mDriverPaths[] = {
	L"\\EFI\\Boot\\" DRIVER_FILENAME,
	L"\\EFI\\" DRIVER_FILENAME,
	L"\\" DRIVER_FILENAME
};

extern
VOID
EFIAPI
BmSetMemoryTypeInformationVariable(
	IN BOOLEAN Boot
);


STATIC
BOOLEAN
EFIAPI
WaitForKey(
)
{
	gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
	Print(L"Press any key to continue...");
	EFI_INPUT_KEY Key = { 0, 0 };
	UINTN Index = 0;
	gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
	gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

	return (BOOLEAN)(Key.ScanCode != SCAN_ESC);
}

// 
// Try to find a file by browsing each device
// 
STATIC
EFI_STATUS
LocateFile(
	IN CHAR16* ImagePath,
	IN EFI_HANDLE ImageHandle,
	OUT EFI_DEVICE_PATH** DevicePath
)
{
	*DevicePath = NULL;

	UINTN NumHandles;
	EFI_HANDLE* Handles;
	EFI_STATUS Status = gBS->LocateHandleBuffer(ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&Handles);
	if (EFI_ERROR(Status))
		return Status;

#ifdef _DEBUG
	Print(L"[LOADER] Number of UEFI Filesystem Devices: %llu\r\n", NumHandles);
#endif // _DEBUG

	for (UINTN i = 0; i < NumHandles; i++)
	{
		EFI_FILE_IO_INTERFACE* IoDevice;
		Status = gBS->OpenProtocol(Handles[i],
			&gEfiSimpleFileSystemProtocolGuid,
			(VOID**)&IoDevice,
			ImageHandle,
			NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (Status != EFI_SUCCESS)
			continue;

		EFI_FILE_HANDLE VolumeHandle;
		Status = IoDevice->OpenVolume(IoDevice, &VolumeHandle);
		if (EFI_ERROR(Status))
			continue;

		EFI_FILE_HANDLE FileHandle;
		Status = VolumeHandle->Open(VolumeHandle,
			&FileHandle,
			ImagePath,
			EFI_FILE_MODE_READ,
			EFI_FILE_READ_ONLY);
		if (!EFI_ERROR(Status))
		{
			VolumeHandle->Close(FileHandle);
			*DevicePath = FileDevicePath(Handles[i], ImagePath);
			break;
		}
	}

	FreePool(Handles);

	return Status;
}

//
// Find the optimal available console output mode and set it if it's not already the current mode
//
STATIC
EFI_STATUS
EFIAPI
SetHighestAvailableMode(
	VOID
)
{
	INT32 MaxModeNum = 0;
	UINTN Cols, Rows, MaxColsXRows = 0;

	for (INT32 ModeNum = 0; ModeNum < gST->ConOut->Mode->MaxMode; ModeNum++)
	{
		CONST EFI_STATUS Status = gST->ConOut->QueryMode(gST->ConOut, ModeNum, &Cols, &Rows);
		if (EFI_ERROR(Status))
			continue;

		// Accept only modes where the total of (Rows * Columns) >= the previous known best
		if ((Cols * Rows) >= MaxColsXRows)
		{
			MaxColsXRows = Cols * Rows;
			MaxModeNum = ModeNum;
		}
	}

	if (gST->ConOut->Mode->Mode == MaxModeNum)
	{
		// We're already at the correct mode
		return EFI_SUCCESS;
	}

	return gST->ConOut->SetMode(gST->ConOut, MaxModeNum);
}

//
// Connects all current system handles recursively.
//
STATIC
EFI_STATUS
EFIAPI
BdsLibConnectAllEfi(
	VOID
)
{
	UINTN HandleCount;
	EFI_HANDLE* HandleBuffer;
	CONST EFI_STATUS Status = gBS->LocateHandleBuffer(AllHandles,
		NULL,
		NULL,
		&HandleCount,
		&HandleBuffer);
	if (EFI_ERROR(Status))
		return Status;

	for (UINTN Index = 0; Index < HandleCount; ++Index)
	{
		gBS->ConnectController(HandleBuffer[Index],
			NULL,
			NULL,
			TRUE);
	}

	if (HandleBuffer != NULL)
		FreePool(HandleBuffer);

	return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
StartDriver(
	IN EFI_HANDLE ImageHandle,
	IN EFI_SYSTEM_TABLE* SystemTable
)
{
	EFI_DEVICE_PATH* DriverDevicePath = NULL;

	// 
	// Check if the driver is loaded 
	// 
	EFI_STATUS Status;
	Print(L"[LOADER] Locating and loading driver file %s...\r\n", DRIVER_FILENAME);
	for (UINT32 i = 0; i < ARRAY_SIZE(mDriverPaths); ++i)
	{
		Status = LocateFile(mDriverPaths[i], ImageHandle, &DriverDevicePath);
		if (!EFI_ERROR(Status))
			break;
	}
	if (EFI_ERROR(Status))
	{
		Print(L"[LOADER] Failed to find driver file %s.\r\n", DRIVER_FILENAME);
		goto Exit;
	}

	EFI_HANDLE DriverHandle = NULL;
	Status = gBS->LoadImage(FALSE, // Request is not from boot manager
		ImageHandle,
		DriverDevicePath,
		NULL,
		0,
		&DriverHandle);
	if (EFI_ERROR(Status))
	{
		Print(L"[LOADER] LoadImage failed: %llx (%r).\r\n", Status, Status);
		goto Exit;
	}

	Status = gBS->StartImage(DriverHandle, NULL, NULL);
	if (EFI_ERROR(Status))
	{
		Print(L"[LOADER] StartImage failed: %llx (%r).\r\n", Status, Status);
		goto Exit;
	}

Exit:
	if (DriverDevicePath != NULL)
		FreePool(DriverDevicePath);

	return Status;
}

// Application entrypoint (must be set to 'efi_main' for gnu-efi crt0 compatibility)
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	UINTN Event;

#if defined(_GNU_EFI)
	InitializeLib(ImageHandle, SystemTable);
#endif

	//
	// Set the highest available console mode and clear the screen
	//
	SetHighestAvailableMode();
	gST->ConOut->ClearScreen(gST->ConOut);

	//
	// Turn off the watchdog timer
	//
	gBS->SetWatchdogTimer(0, 0, 0, NULL);

	//
	// Locate, load, start and configure the driver
	//
	CONST EFI_STATUS DriverStatus = StartDriver(ImageHandle, SystemTable);
	if (DriverStatus == EFI_ALREADY_STARTED)
		return EFI_SUCCESS;

	if (EFI_ERROR(DriverStatus))
	{
		Print(L"\r\nERROR: driver load failed with status %llx (%r).\r\n", DriverStatus, DriverStatus);
	}

	WaitForKey();

	gBS->Exit(ImageHandle, EFI_SUCCESS, 0, NULL);

	return EFI_SUCCESS;
}
