#include "pch.h"
#include "WinUHidSteamController.h"

#include <algorithm>
#include <cstring>

//
// This device emulates a 2026 Steam Controller (Triton) connected over Bluetooth LE.
//

const BYTE k_SteamControllerReportDescriptor[] =
{
	0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x01,       // Usage (0x01)
	0xA1, 0x01,       // Collection (Application)
	0x15, 0x00,       //   Logical Minimum (0)
	0x26, 0xFF, 0x00, //   Logical Maximum (255)
	0x75, 0x08,       //   Report Size (8)

	0x85, 0x42,       //   Report ID (66)
	0x09, 0x42,       //   Usage (0x42)
	0x95, 0x2D,       //   Report Count (45)
	0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x85, 0x43,       //   Report ID (67)
	0x09, 0x43,       //   Usage (0x43)
	0x95, 0x0E,       //   Report Count (14)
	0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x85, 0x45,       //   Report ID (69)
	0x09, 0x45,       //   Usage (0x45)
	0x95, 0x2D,       //   Report Count (45)
	0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x85, 0x46,       //   Report ID (70)
	0x09, 0x46,       //   Usage (0x46)
	0x95, 0x01,       //   Report Count (1)
	0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x85, 0x79,       //   Report ID (121)
	0x09, 0x79,       //   Usage (0x79)
	0x95, 0x01,       //   Report Count (1)
	0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x85, 0x01,       //   Report ID (1)
	0x09, 0x01,       //   Usage (0x01)
	0x95, 0x3F,       //   Report Count (63)
	0xB1, 0x02,       //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x80,       //   Report ID (128)
	0x09, 0x80,       //   Usage (0x80)
	0x95, 0x09,       //   Report Count (9)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x81,       //   Report ID (129)
	0x09, 0x81,       //   Usage (0x81)
	0x95, 0x07,       //   Report Count (7)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x82,       //   Report ID (130)
	0x09, 0x82,       //   Usage (0x82)
	0x95, 0x03,       //   Report Count (3)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x83,       //   Report ID (131)
	0x09, 0x83,       //   Usage (0x83)
	0x95, 0x09,       //   Report Count (9)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x85,       //   Report ID (133)
	0x09, 0x85,       //   Usage (0x85)
	0x95, 0x03,       //   Report Count (3)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0x85, 0x86,       //   Report ID (134)
	0x09, 0x86,       //   Usage (0x86)
	0x95, 0x03,       //   Report Count (3)
	0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

	0xC0,             // End Collection
};

const WCHAR k_SteamControllerDefaultInstanceId[] = L"FXA99613035A2";
const WCHAR k_SteamControllerHardwareIds[] = L"HID\\VID_28DE&PID_1303\0\0";
const CHAR k_SteamControllerDefaultSerial[] = "FXA99613035A2";
const ULONG k_SteamControllerDefaultFirmwareBuildTime = 0x69fe17ff;
const ULONG k_SteamControllerDefaultBoardRevision = 0x48;
const ULONG k_SteamControllerBootloaderBuildTime = 0x62a9122b;
const ULONG k_SteamControllerSensorUpdateIntervalUs = 4032;

const WINUHID_DEVICE_CONFIG k_SteamControllerConfig =
{
	(WINUHID_EVENT_TYPE)(WINUHID_EVENT_READ_REPORT | WINUHID_EVENT_WRITE_REPORT | WINUHID_EVENT_GET_FEATURE | WINUHID_EVENT_SET_FEATURE),
	WINUHID_STEAM_CONTROLLER_VENDOR_ID,
	WINUHID_STEAM_CONTROLLER_TRITON_BLE_PRODUCT_ID,
	1,
	sizeof(k_SteamControllerReportDescriptor),
	k_SteamControllerReportDescriptor,
	{},
	k_SteamControllerDefaultInstanceId,
	k_SteamControllerHardwareIds,
	10000, // 10 ms input throttling interval
};

#define STEAM_CONTROLLER_FEATURE_GET_ATTRIBUTES_VALUES 0x83
#define STEAM_CONTROLLER_FEATURE_SET_SETTINGS_VALUES 0x87
#define STEAM_CONTROLLER_FEATURE_GET_SETTINGS_VALUES 0x89
#define STEAM_CONTROLLER_FEATURE_GET_SETTINGS_MAXS 0x8B
#define STEAM_CONTROLLER_FEATURE_GET_SETTINGS_DEFAULTS 0x8C
#define STEAM_CONTROLLER_FEATURE_SET_CONTROLLER_MODE 0x8D
#define STEAM_CONTROLLER_FEATURE_GET_DEVICE_INFO 0xA1
#define STEAM_CONTROLLER_FEATURE_GET_STRING_ATTRIBUTE 0xAE

#define STEAM_CONTROLLER_ATTRIB_PRODUCT_ID 0x01
#define STEAM_CONTROLLER_ATTRIB_CAPABILITIES 0x02
#define STEAM_CONTROLLER_ATTRIB_FIRMWARE_BUILD_TIME 0x04
#define STEAM_CONTROLLER_ATTRIB_BOARD_REVISION 0x09
#define STEAM_CONTROLLER_ATTRIB_BOOTLOADER_BUILD_TIME 0x0A
#define STEAM_CONTROLLER_ATTRIB_CONNECTION_INTERVAL_US 0x0B
#define STEAM_CONTROLLER_ATTRIB_COUNT_LEGACY 0x0C
#define STEAM_CONTROLLER_ATTRIB_RESERVED_0D 0x0D
#define STEAM_CONTROLLER_ATTRIB_RESERVED_0E 0x0E

#define STEAM_CONTROLLER_WIRELESS_DISCONNECT 1
#define STEAM_CONTROLLER_WIRELESS_CONNECT 2

typedef struct _WINUHID_STEAM_CONTROLLER {
	PWINUHID_DEVICE Device;
	SRWLOCK Lock;

	PWINUHID_STEAM_CONTROLLER_RAW_HID_CB RawHidCallback;
	PVOID CallbackContext;

	WINUHID_STEAM_CONTROLLER_INPUT_REPORT LastInputReport;
	WINUHID_STEAM_CONTROLLER_BATTERY_REPORT LastBatteryReport;
	WINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT LastWirelessStatusReport;

	UCHAR LastFeatureCommand;
	UCHAR LastStringAttribute;
	CHAR SerialNumber[21];
	ULONG FirmwareBuildTime;
	ULONG BoardRevision;
} WINUHID_STEAM_CONTROLLER, *PWINUHID_STEAM_CONTROLLER;

void PutU32Le(UCHAR* Out, ULONG Value)
{
	Out[0] = (UCHAR)(Value & 0xff);
	Out[1] = (UCHAR)((Value >> 8) & 0xff);
	Out[2] = (UCHAR)((Value >> 16) & 0xff);
	Out[3] = (UCHAR)((Value >> 24) & 0xff);
}

void PutAttribute(UCHAR* Out, UCHAR Tag, ULONG Value)
{
	Out[0] = Tag;
	PutU32Le(Out + 1, Value);
}

void BuildFeatureResponse(PWINUHID_STEAM_CONTROLLER Controller, PWINUHID_STEAM_CONTROLLER_FEATURE_REPORT Report)
{
	RtlZeroMemory(Report, sizeof(*Report));

	UCHAR* feature = (UCHAR*)Report;
	feature[0] = WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_ID;

	AcquireSRWLockShared(&Controller->Lock);
	const UCHAR command = Controller->LastFeatureCommand;
	const UCHAR stringAttribute = Controller->LastStringAttribute;
	const ULONG firmwareBuildTime = Controller->FirmwareBuildTime;
	const ULONG boardRevision = Controller->BoardRevision;
	CHAR serialNumber[sizeof(Controller->SerialNumber)];
	RtlCopyMemory(serialNumber, Controller->SerialNumber, sizeof(serialNumber));
	ReleaseSRWLockShared(&Controller->Lock);

	feature[1] = command;

	switch (command)
	{
	case STEAM_CONTROLLER_FEATURE_GET_ATTRIBUTES_VALUES:
		feature[2] = 45;
		PutAttribute(&feature[3], STEAM_CONTROLLER_ATTRIB_PRODUCT_ID, WINUHID_STEAM_CONTROLLER_TRITON_BLE_PRODUCT_ID);
		PutAttribute(&feature[8], STEAM_CONTROLLER_ATTRIB_CAPABILITIES, 0);
		PutAttribute(&feature[13], STEAM_CONTROLLER_ATTRIB_BOOTLOADER_BUILD_TIME, k_SteamControllerBootloaderBuildTime);
		PutAttribute(&feature[18], STEAM_CONTROLLER_ATTRIB_FIRMWARE_BUILD_TIME, firmwareBuildTime);
		PutAttribute(&feature[23], STEAM_CONTROLLER_ATTRIB_BOARD_REVISION, boardRevision);
		PutAttribute(&feature[28], STEAM_CONTROLLER_ATTRIB_CONNECTION_INTERVAL_US, k_SteamControllerSensorUpdateIntervalUs);
		PutAttribute(&feature[33], STEAM_CONTROLLER_ATTRIB_RESERVED_0D, 0);
		PutAttribute(&feature[38], STEAM_CONTROLLER_ATTRIB_COUNT_LEGACY, 0);
		PutAttribute(&feature[43], STEAM_CONTROLLER_ATTRIB_RESERVED_0E, 0);
		break;

	case STEAM_CONTROLLER_FEATURE_GET_STRING_ATTRIBUTE:
		feature[2] = 21;
		feature[3] = stringAttribute;
		RtlCopyMemory(&feature[4], serialNumber, std::min<size_t>(strlen(serialNumber), 20));
		break;

	case STEAM_CONTROLLER_FEATURE_GET_DEVICE_INFO:
	case STEAM_CONTROLLER_FEATURE_GET_SETTINGS_VALUES:
	case STEAM_CONTROLLER_FEATURE_GET_SETTINGS_MAXS:
	case STEAM_CONTROLLER_FEATURE_GET_SETTINGS_DEFAULTS:
	case STEAM_CONTROLLER_FEATURE_SET_SETTINGS_VALUES:
	case STEAM_CONTROLLER_FEATURE_SET_CONTROLLER_MODE:
		feature[2] = 0;
		break;

	default:
		feature[1] = 0;
		feature[2] = 0;
		break;
	}
}

BOOL SubmitInputReport(PWINUHID_STEAM_CONTROLLER Controller, LPCVOID Report, ULONG ReportLength)
{
	BOOL ret = WinUHidSubmitInputReport(Controller->Device, Report, ReportLength);

	//
	// Since we handle WINUHID_EVENT_READ_REPORT, WinUHidSubmitInputReport may fail with ERROR_NOT_READY
	// if a HID client hasn't asked for another report. This is fine because our callback function will
	// report this input once the caller is ready for it.
	//
	return ret || GetLastError() == ERROR_NOT_READY;
}

BOOL ValidateRawInputReport(CONST UCHAR* Report, ULONG ReportLength)
{
	if (!Report || ReportLength == 0) {
		return FALSE;
	}

	switch (Report[0])
	{
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT:
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE:
		return ReportLength == sizeof(WINUHID_STEAM_CONTROLLER_INPUT_REPORT);

	case WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY:
		return ReportLength == sizeof(WINUHID_STEAM_CONTROLLER_BATTERY_REPORT);

	case WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS_X:
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS:
		return ReportLength == sizeof(WINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT);

	default:
		return FALSE;
	}
}

VOID InvokeRawHidCallback(PWINUHID_STEAM_CONTROLLER Controller,
	WINUHID_STEAM_CONTROLLER_RAW_HID_REPORT_TYPE ReportType, PCWINUHID_EVENT Event)
{
	if (Controller->RawHidCallback && Event->Write.DataLength) {
		Controller->RawHidCallback(Controller->CallbackContext, ReportType, Event->Write.Data, Event->Write.DataLength);
	}
}

VOID WinUHidSteamControllerCallback(PVOID CallbackContext, PWINUHID_DEVICE Device, PCWINUHID_EVENT Event)
{
	auto controller = (PWINUHID_STEAM_CONTROLLER)CallbackContext;

	if (Event->Type == WINUHID_EVENT_GET_FEATURE) {
		WINUHID_STEAM_CONTROLLER_FEATURE_REPORT featureReport;
		BuildFeatureResponse(controller, &featureReport);
		WinUHidCompleteReadEvent(Device, Event, &featureReport, sizeof(featureReport));
	}
	else if (Event->Type == WINUHID_EVENT_SET_FEATURE) {
		InvokeRawHidCallback(controller, WinUHidSteamControllerRawHidReportTypeFeature, Event);

		if (Event->Write.DataLength >= 2 && Event->Write.Data[0] == WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_ID) {
			AcquireSRWLockExclusive(&controller->Lock);
			controller->LastFeatureCommand = Event->Write.Data[1];
			if (Event->Write.Data[1] == STEAM_CONTROLLER_FEATURE_GET_STRING_ATTRIBUTE && Event->Write.DataLength >= 4) {
				controller->LastStringAttribute = Event->Write.Data[3];
			}
			ReleaseSRWLockExclusive(&controller->Lock);
		}

		WinUHidCompleteWriteEvent(Device, Event, TRUE);
	}
	else if (Event->Type == WINUHID_EVENT_READ_REPORT) {
		UCHAR report[sizeof(WINUHID_STEAM_CONTROLLER_INPUT_REPORT)];
		ULONG reportLength;

		AcquireSRWLockShared(&controller->Lock);
		if (Event->ReportId == WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY) {
			RtlCopyMemory(report, &controller->LastBatteryReport, sizeof(controller->LastBatteryReport));
			reportLength = sizeof(controller->LastBatteryReport);
		}
		else if (Event->ReportId == WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS ||
			Event->ReportId == WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS_X) {
			RtlCopyMemory(report, &controller->LastWirelessStatusReport, sizeof(controller->LastWirelessStatusReport));
			report[0] = Event->ReportId;
			reportLength = sizeof(controller->LastWirelessStatusReport);
		}
		else {
			RtlCopyMemory(report, &controller->LastInputReport, sizeof(controller->LastInputReport));
			if (Event->ReportId == WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT) {
				report[0] = WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT;
			}
			reportLength = sizeof(controller->LastInputReport);
		}
		ReleaseSRWLockShared(&controller->Lock);

		WinUHidCompleteReadEvent(Device, Event, report, reportLength);
	}
	else if (Event->Type == WINUHID_EVENT_WRITE_REPORT) {
		InvokeRawHidCallback(controller, WinUHidSteamControllerRawHidReportTypeOutput, Event);
		WinUHidCompleteWriteEvent(Device, Event, TRUE);
	}
	else {
		WinUHidCompleteWriteEvent(Device, Event, FALSE);
	}
}

WINUHID_API PWINUHID_STEAM_CONTROLLER WinUHidSteamControllerCreate(PCWINUHID_STEAM_CONTROLLER_INFO Info,
	PWINUHID_STEAM_CONTROLLER_RAW_HID_CB RawHidCallback, PVOID CallbackContext)
{
	WINUHID_DEVICE_CONFIG config = k_SteamControllerConfig;
	PopulateDeviceInfo(&config, Info ? Info->BasicInfo : NULL);

	if (config.VendorID == 0) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	PWINUHID_STEAM_CONTROLLER controller = (PWINUHID_STEAM_CONTROLLER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*controller));
	if (!controller) {
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	InitializeSRWLock(&controller->Lock);
	controller->RawHidCallback = RawHidCallback;
	controller->CallbackContext = CallbackContext;
	controller->LastFeatureCommand = 0;
	controller->LastStringAttribute = 1;
	controller->FirmwareBuildTime = Info && Info->FirmwareBuildTime ? Info->FirmwareBuildTime : k_SteamControllerDefaultFirmwareBuildTime;
	controller->BoardRevision = Info && Info->BoardRevision ? Info->BoardRevision : k_SteamControllerDefaultBoardRevision;

	RtlCopyMemory(controller->SerialNumber, k_SteamControllerDefaultSerial, sizeof(k_SteamControllerDefaultSerial));
	if (Info && Info->SerialNumber) {
		RtlZeroMemory(controller->SerialNumber, sizeof(controller->SerialNumber));
		RtlCopyMemory(controller->SerialNumber, Info->SerialNumber, std::min<size_t>(strlen(Info->SerialNumber), sizeof(controller->SerialNumber) - 1));
	}

	WinUHidSteamControllerInitializeInputReport(&controller->LastInputReport);
	WinUHidSteamControllerInitializeBatteryReport(&controller->LastBatteryReport);
	WinUHidSteamControllerInitializeWirelessStatusReport(&controller->LastWirelessStatusReport, TRUE);

	controller->Device = WinUHidCreateDevice(&config);
	if (!controller->Device) {
		WinUHidSteamControllerDestroy(controller);
		return NULL;
	}

	if (!WinUHidStartDevice(controller->Device, WinUHidSteamControllerCallback, controller)) {
		WinUHidSteamControllerDestroy(controller);
		return NULL;
	}

	if (!WinUHidSteamControllerReportWirelessStatus(controller, &controller->LastWirelessStatusReport) ||
		!WinUHidSteamControllerReportBattery(controller, &controller->LastBatteryReport) ||
		!WinUHidSteamControllerReportInput(controller, &controller->LastInputReport)) {
		WinUHidSteamControllerDestroy(controller);
		return NULL;
	}

	return controller;
}

WINUHID_API VOID WinUHidSteamControllerInitializeInputReport(PWINUHID_STEAM_CONTROLLER_INPUT_REPORT Report)
{
	RtlZeroMemory(Report, sizeof(*Report));

	Report->ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE;
}

WINUHID_API VOID WinUHidSteamControllerInitializeBatteryReport(PWINUHID_STEAM_CONTROLLER_BATTERY_REPORT Report)
{
	RtlZeroMemory(Report, sizeof(*Report));

	Report->ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY;
	Report->ChargeState = 1;
	Report->BatteryLevel = 100;
}

WINUHID_API VOID WinUHidSteamControllerInitializeWirelessStatusReport(PWINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT Report, BOOL Connected)
{
	RtlZeroMemory(Report, sizeof(*Report));

	Report->ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS;
	Report->State = Connected ? STEAM_CONTROLLER_WIRELESS_CONNECT : STEAM_CONTROLLER_WIRELESS_DISCONNECT;
}

WINUHID_API BOOL WinUHidSteamControllerReportInput(PWINUHID_STEAM_CONTROLLER Controller, PCWINUHID_STEAM_CONTROLLER_INPUT_REPORT Report)
{
	if (Report->ReportId != WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT &&
		Report->ReportId != WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	AcquireSRWLockExclusive(&Controller->Lock);
	Controller->LastInputReport = *Report;
	ReleaseSRWLockExclusive(&Controller->Lock);

	return SubmitInputReport(Controller, Report, sizeof(*Report));
}

WINUHID_API BOOL WinUHidSteamControllerReportBattery(PWINUHID_STEAM_CONTROLLER Controller, PCWINUHID_STEAM_CONTROLLER_BATTERY_REPORT Report)
{
	if (Report->ReportId != WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	AcquireSRWLockExclusive(&Controller->Lock);
	Controller->LastBatteryReport = *Report;
	ReleaseSRWLockExclusive(&Controller->Lock);

	return SubmitInputReport(Controller, Report, sizeof(*Report));
}

WINUHID_API BOOL WinUHidSteamControllerReportWirelessStatus(PWINUHID_STEAM_CONTROLLER Controller, PCWINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT Report)
{
	if (Report->ReportId != WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS &&
		Report->ReportId != WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS_X) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	AcquireSRWLockExclusive(&Controller->Lock);
	Controller->LastWirelessStatusReport = *Report;
	ReleaseSRWLockExclusive(&Controller->Lock);

	return SubmitInputReport(Controller, Report, sizeof(*Report));
}

WINUHID_API BOOL WinUHidSteamControllerReportRawInput(PWINUHID_STEAM_CONTROLLER Controller, LPCVOID Report, ULONG ReportLength)
{
	auto rawReport = (CONST UCHAR*)Report;
	if (!ValidateRawInputReport(rawReport, ReportLength)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	AcquireSRWLockExclusive(&Controller->Lock);
	switch (rawReport[0])
	{
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT:
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE:
		RtlCopyMemory(&Controller->LastInputReport, rawReport, sizeof(Controller->LastInputReport));
		break;
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY:
		RtlCopyMemory(&Controller->LastBatteryReport, rawReport, sizeof(Controller->LastBatteryReport));
		break;
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS_X:
	case WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS:
		RtlCopyMemory(&Controller->LastWirelessStatusReport, rawReport, sizeof(Controller->LastWirelessStatusReport));
		break;
	}
	ReleaseSRWLockExclusive(&Controller->Lock);

	return SubmitInputReport(Controller, Report, ReportLength);
}

WINUHID_API VOID WinUHidSteamControllerDestroy(PWINUHID_STEAM_CONTROLLER Controller)
{
	if (!Controller) {
		return;
	}

	if (Controller->Device) {
		WinUHidDestroyDevice(Controller->Device);
	}

	HeapFree(GetProcessHeap(), 0, Controller);
}
