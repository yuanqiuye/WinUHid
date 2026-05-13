#include "pch.h"
#include "WinUHidDevs.h"

BOOL IsZeroGuid(REFGUID Guid)
{
	return Guid.Data1 == 0 &&
		Guid.Data2 == 0 &&
		Guid.Data3 == 0 &&
		Guid.Data4[0] == 0 &&
		Guid.Data4[1] == 0 &&
		Guid.Data4[2] == 0 &&
		Guid.Data4[3] == 0 &&
		Guid.Data4[4] == 0 &&
		Guid.Data4[5] == 0 &&
		Guid.Data4[6] == 0 &&
		Guid.Data4[7] == 0;
}

VOID PopulateDeviceInfo(PWINUHID_DEVICE_CONFIG Config, PCWINUHID_PRESET_DEVICE_INFO Info)
{
	if (!Info) {
		return;
	}

	if (Info->VendorID != 0) {
		Config->VendorID = Info->VendorID;
		Config->ProductID = Info->ProductID;
		Config->VersionNumber = Info->VersionNumber;

		if (!Info->HardwareIDs) {
			Config->HardwareIDs = NULL;
		}
	}

	if (!IsZeroGuid(Info->ContainerId)) {
		Config->ContainerId = Info->ContainerId;
	}
	if (Info->InstanceID) {
		Config->InstanceID = Info->InstanceID;
	}
	if (Info->HardwareIDs) {
		Config->HardwareIDs = Info->HardwareIDs;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
