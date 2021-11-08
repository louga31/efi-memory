#pragma once
// This header contains dummy functions to hook in runtime services table
// We want to do this since then, the function pointers are going to be pointing
// to somewhat similar memory location

#include <efi.h>
#include <efilib.h>

static EFI_GET_TIME oGetTime;
EFI_STATUS EFIAPI HookedGetTime(EFI_TIME* time, EFI_TIME_CAPABILITIES* capabilities)
{
    return oGetTime(time, capabilities);
}

static EFI_SET_TIME oSetTime;
EFI_STATUS EFIAPI HookedSetTime(EFI_TIME* time)
{
    return oSetTime(time);
}

static EFI_GET_WAKEUP_TIME oGetWakeupTime;
EFI_STATUS EFIAPI HookedGetWakeupTime(BOOLEAN* enabled, BOOLEAN* pending, EFI_TIME* time)
{
    return oGetWakeupTime(enabled, pending, time);
}

static EFI_SET_WAKEUP_TIME oSetWakeupTime;
EFI_STATUS EFIAPI HookedSetWakeupTime(BOOLEAN enable, EFI_TIME* time)
{
    return oSetWakeupTime(enable, time);
}

static EFI_SET_VIRTUAL_ADDRESS_MAP oSetVirtualAddressMap;
EFI_STATUS EFIAPI HookedSetVirtualAddressMap(UINTN mapSize, UINTN descriptorSize, UINT32 version, EFI_MEMORY_DESCRIPTOR* virtualMap)
{
    return oSetVirtualAddressMap(mapSize, descriptorSize, version, virtualMap);
}

static EFI_CONVERT_POINTER oConvertPointer;
EFI_STATUS EFIAPI HookedConvertPointer(UINTN debug, void** address)
{
    return oConvertPointer(debug, address);
}

static EFI_GET_VARIABLE oGetVariable;
EFI_STATUS EFIAPI HookedGetVariable(CHAR16* variableName, EFI_GUID* vendorGuid, UINT32* attributes, UINTN* dataSize, void* data)
{
    return oGetVariable(variableName, vendorGuid, attributes, dataSize, data);
}

static EFI_GET_NEXT_VARIABLE_NAME oGetNextVariableName;
EFI_STATUS EFIAPI HookedGetNextVariableName(UINTN* variableNameSize, CHAR16* variableName, EFI_GUID* vendorGuid)
{
    return oGetNextVariableName(variableNameSize, variableName, vendorGuid);
}

/*static EFI_SET_VARIABLE oSetVariable;
EFI_STATUS EFIAPI HookedSetVariable(CHAR16* variableName, EFI_GUID* vendorGuid, UINT32 attributes, UINTN dataSize, void* data)
{
    return oSetVariable(variableName, vendorGuid, attributes, dataSize, data);
}*/

static EFI_GET_NEXT_HIGH_MONO_COUNT oGetNextHighMonotonicCount;
EFI_STATUS EFIAPI HookedGetNextHighMonotonicCount(UINT32* highCount)
{
    return oGetNextHighMonotonicCount(highCount);
}

static EFI_RESET_SYSTEM oResetSystem;
EFI_STATUS EFIAPI HookedResetSystem(EFI_RESET_TYPE resetType, EFI_STATUS resetStatus, UINTN dataSize, CHAR16* resetData)
{
    return oResetSystem(resetType, resetStatus, dataSize, resetData);
}

static EFI_UPDATE_CAPSULE oUpdateCapsule;
EFI_STATUS EFIAPI HookedUpdateCapsule(EFI_CAPSULE_HEADER** capsuleHeaderArray, UINTN capsuleCount, EFI_PHYSICAL_ADDRESS scatterGatherList)
{
    return oUpdateCapsule(capsuleHeaderArray, capsuleCount, scatterGatherList);
}

static EFI_QUERY_CAPSULE_CAPABILITIES oQueryCapsuleCapabilities;
EFI_STATUS EFIAPI HookedQueryCapsuleCapabilities(EFI_CAPSULE_HEADER** capsuleHeaderArray, UINTN capsuleCount, UINT64* maximumCapsuleSize, EFI_RESET_TYPE* resetType)
{
    return oQueryCapsuleCapabilities(capsuleHeaderArray, capsuleCount, maximumCapsuleSize, resetType);
}

static EFI_QUERY_VARIABLE_INFO oQueryVariableInfo;
EFI_STATUS EFIAPI HookedQueryVariableInfo(UINT32 attributes, UINT64* maximumVariableStorageSize, UINT64* remainingVariableStorageSize, UINT64* maximumVariableSize)
{
    return oQueryVariableInfo(attributes, maximumVariableStorageSize, remainingVariableStorageSize, maximumVariableSize);
}