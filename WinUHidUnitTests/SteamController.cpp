#include "pch.h"
#include "Utilities.h"

#include "../WinUHidDevs/WinUHidSteamController.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace {
	constexpr USHORT k_TestVendorId = WINUHID_STEAM_CONTROLLER_VENDOR_ID;
	constexpr USHORT k_TestProductId = WINUHID_STEAM_CONTROLLER_TRITON_BLE_PRODUCT_ID;
	constexpr WCHAR k_TestInstanceId[] = L"WinUHidSteamControllerUnitTest";
	std::string g_LastHidOpenError;

	struct SteamControllerDeleter
	{
		void operator()(PWINUHID_STEAM_CONTROLLER controller) const
		{
			WinUHidSteamControllerDestroy(controller);
		}
	};
	using SteamControllerPtr = std::unique_ptr<std::remove_pointer_t<PWINUHID_STEAM_CONTROLLER>, SteamControllerDeleter>;

	struct HidDeviceDeleter
	{
		void operator()(SDL_hid_device* hid) const
		{
			SDL_hid_close(hid);
		}
	};
	using HidDevicePtr = std::unique_ptr<SDL_hid_device, HidDeviceDeleter>;

	ULONGLONG MakeRawCallbackValue(WINUHID_STEAM_CONTROLLER_RAW_HID_REPORT_TYPE type,
		ULONG length,
		UCHAR byte0,
		UCHAR byte1,
		UCHAR byte2,
		UCHAR byte3)
	{
		return ((ULONGLONG)type << 56) |
			((ULONGLONG)length << 48) |
			((ULONGLONG)byte0 << 24) |
			((ULONGLONG)byte1 << 16) |
			((ULONGLONG)byte2 << 8) |
			byte3;
	}

	void RawHidCallback(PVOID callbackContext,
		WINUHID_STEAM_CONTROLLER_RAW_HID_REPORT_TYPE reportType,
		CONST UCHAR* report,
		ULONG reportLength)
	{
		UCHAR bytes[4] = {};
		for (ULONG i = 0; i < reportLength && i < ARRAYSIZE(bytes); i++) {
			bytes[i] = report[i];
		}

		((CallbackData<ULONGLONG>*)callbackContext)->Signal(MakeRawCallbackValue(
			reportType,
			reportLength,
			bytes[0],
			bytes[1],
			bytes[2],
			bytes[3]));
	}

	WINUHID_STEAM_CONTROLLER_INFO MakeTestControllerInfo(PCWINUHID_PRESET_DEVICE_INFO basicInfo)
	{
		WINUHID_STEAM_CONTROLLER_INFO info = {};
		info.BasicInfo = basicInfo;
		info.SerialNumber = "UNITTESTTRITON0001";
		info.FirmwareBuildTime = 0x11223344;
		info.BoardRevision = 0x55;
		return info;
	}

	SteamControllerPtr CreateTestController(PWINUHID_STEAM_CONTROLLER_RAW_HID_CB callback = NULL,
		PVOID callbackContext = NULL)
	{
		WINUHID_PRESET_DEVICE_INFO basicInfo = {};
		basicInfo.InstanceID = k_TestInstanceId;

		auto info = MakeTestControllerInfo(&basicInfo);
		return SteamControllerPtr(WinUHidSteamControllerCreate(&info, callback, callbackContext));
	}

	void DrainInputReports(SDL_hid_device* hid)
	{
		UCHAR buffer[128];
		while (SDL_hid_read_timeout(hid, buffer, sizeof(buffer), 0) > 0) {
		}
	}

	std::string DumpMatchingHidDevices(USHORT vendorId, USHORT productId)
	{
		std::ostringstream oss;
		auto devices = SDL_hid_enumerate(vendorId, productId);
		int count = 0;

		for (auto device = devices; device; device = device->next) {
			count++;
			oss << "\n  path=" << (device->path ? device->path : "<null>")
				<< " vid=0x" << std::hex << device->vendor_id
				<< " pid=0x" << std::hex << device->product_id
				<< " usage_page=0x" << std::hex << device->usage_page
				<< " usage=0x" << std::hex << device->usage
				<< " interface=" << std::dec << device->interface_number;
		}

		SDL_hid_free_enumeration(devices);

		if (count == 0) {
			oss << "\n  no matching HID devices enumerated";
		}

		return oss.str();
	}

	std::string ToLower(std::string value)
	{
		for (auto& ch : value) {
			if (ch >= 'A' && ch <= 'Z') {
				ch = ch - 'A' + 'a';
			}
		}

		return value;
	}

	bool IsTestHidPath(const char* path)
	{
		if (!path) {
			return false;
		}

		auto lowerPath = ToLower(path);

		if (lowerPath.find("winuhidsteamcontrollerunittest") != std::string::npos) {
			return true;
		}

		//
		// Real directly-paired 2026 Steam Controllers enumerate through BTHLEDevice.
		// The WinUHid VHF child is a synthetic HID path, so this avoids touching the
		// user's physical Bluetooth controller if it is connected during the tests.
		//
		return lowerPath.find("bthledevice") == std::string::npos &&
			lowerPath.find("vid_28de") != std::string::npos &&
			lowerPath.find("pid_1303") != std::string::npos;
	}

	HidDevicePtr OpenTestSteamControllerHid()
	{
		g_LastHidOpenError.clear();

		for (int i = 0; i < 5; i++) {
			SDL_Delay(1000);

			std::string selectedPath;
			int totalMatchingDevices = 0;
			int candidateDevices = 0;
			auto devices = SDL_hid_enumerate(k_TestVendorId, k_TestProductId);
			for (auto device = devices; device; device = device->next) {
				totalMatchingDevices++;
				if (IsTestHidPath(device->path)) {
					candidateDevices++;
					if (selectedPath.empty()) {
						selectedPath = device->path;
					}
				}
			}
			SDL_hid_free_enumeration(devices);

			if (!selectedPath.empty()) {
				if (auto hid = SDL_hid_open_path(selectedPath.c_str())) {
					return HidDevicePtr(hid);
				}

				g_LastHidOpenError = SDL_GetError();

				if (totalMatchingDevices == 1 && candidateDevices == 1) {
					if (auto hid = SDL_hid_open(k_TestVendorId, k_TestProductId, NULL)) {
						return HidDevicePtr(hid);
					}
					g_LastHidOpenError = SDL_GetError();
				}
			}
		}

		return HidDevicePtr();
	}
}

TEST(SteamController, ReportLayoutsMatchDescriptor)
{
	EXPECT_EQ(sizeof(WINUHID_STEAM_CONTROLLER_INPUT_REPORT), 46u);
	EXPECT_EQ(sizeof(WINUHID_STEAM_CONTROLLER_BATTERY_REPORT), 15u);
	EXPECT_EQ(sizeof(WINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT), 2u);
	EXPECT_EQ(sizeof(WINUHID_STEAM_CONTROLLER_FEATURE_REPORT), WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_BYTES);
}

TEST(SteamController, InitializersUseTritonBleDefaults)
{
	WINUHID_STEAM_CONTROLLER_INPUT_REPORT input;
	WinUHidSteamControllerInitializeInputReport(&input);
	EXPECT_EQ(input.ReportId, WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE);
	EXPECT_EQ(input.SequenceNumber, 0);
	EXPECT_EQ(input.Buttons, 0u);
	EXPECT_EQ(input.TriggerLeft, 0);
	EXPECT_EQ(input.TriggerRight, 0);
	EXPECT_EQ(input.LeftStickX, 0);
	EXPECT_EQ(input.LeftStickY, 0);
	EXPECT_EQ(input.RightStickX, 0);
	EXPECT_EQ(input.RightStickY, 0);

	WINUHID_STEAM_CONTROLLER_BATTERY_REPORT battery;
	WinUHidSteamControllerInitializeBatteryReport(&battery);
	EXPECT_EQ(battery.ReportId, WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY);
	EXPECT_EQ(battery.ChargeState, 1);
	EXPECT_EQ(battery.BatteryLevel, 100);
	EXPECT_EQ(battery.BatteryVoltage, 0);
	EXPECT_EQ(battery.SystemVoltage, 0);
	EXPECT_EQ(battery.InputVoltage, 0);

	WINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT wireless;
	WinUHidSteamControllerInitializeWirelessStatusReport(&wireless, TRUE);
	EXPECT_EQ(wireless.ReportId, WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS);
	EXPECT_EQ(wireless.State, 2);

	WinUHidSteamControllerInitializeWirelessStatusReport(&wireless, FALSE);
	EXPECT_EQ(wireless.ReportId, WINUHID_STEAM_CONTROLLER_REPORT_ID_WIRELESS_STATUS);
	EXPECT_EQ(wireless.State, 1);
}

TEST(SteamController, RejectsInvalidInputReportIds)
{
	WINUHID_STEAM_CONTROLLER_INPUT_REPORT input;
	WinUHidSteamControllerInitializeInputReport(&input);
	input.ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY;
	EXPECT_FALSE(WinUHidSteamControllerReportInput(NULL, &input));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);

	WINUHID_STEAM_CONTROLLER_BATTERY_REPORT battery;
	WinUHidSteamControllerInitializeBatteryReport(&battery);
	battery.ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_INPUT_BLE;
	EXPECT_FALSE(WinUHidSteamControllerReportBattery(NULL, &battery));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);

	WINUHID_STEAM_CONTROLLER_WIRELESS_STATUS_REPORT wireless;
	WinUHidSteamControllerInitializeWirelessStatusReport(&wireless, TRUE);
	wireless.ReportId = WINUHID_STEAM_CONTROLLER_REPORT_ID_BATTERY;
	EXPECT_FALSE(WinUHidSteamControllerReportWirelessStatus(NULL, &wireless));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);
}

TEST(SteamController, RejectsInvalidRawInputReports)
{
	UCHAR outputReport[] = { 0x82, 0x34, 0x12, 0x56 };
	EXPECT_FALSE(WinUHidSteamControllerReportRawInput(NULL, outputReport, sizeof(outputReport)));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);

	WINUHID_STEAM_CONTROLLER_INPUT_REPORT input;
	WinUHidSteamControllerInitializeInputReport(&input);
	EXPECT_FALSE(WinUHidSteamControllerReportRawInput(NULL, &input, sizeof(input) - 1));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);

	input.ReportId = 0;
	EXPECT_FALSE(WinUHidSteamControllerReportRawInput(NULL, &input, sizeof(input)));
	EXPECT_EQ(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER);
}

TEST(SteamController, CreateBasicHidDevice)
{
	auto controller = CreateTestController();
	ASSERT_NE(controller, nullptr) << "Failed to create Steam Controller, last error " << GetLastError();

	HidDevicePtr hid = OpenTestSteamControllerHid();
	ASSERT_NE(hid, nullptr) << "Unable to detect Steam Controller HID device"
		<< DumpMatchingHidDevices(k_TestVendorId, k_TestProductId)
		<< "\n  last open error=" << g_LastHidOpenError;
}

TEST(SteamController, RawInputReportIsReadable)
{
	auto controller = CreateTestController();
	ASSERT_NE(controller, nullptr) << "Failed to create Steam Controller, last error " << GetLastError();

	HidDevicePtr hid = OpenTestSteamControllerHid();
	ASSERT_NE(hid, nullptr) << "Unable to detect Steam Controller HID device"
		<< DumpMatchingHidDevices(k_TestVendorId, k_TestProductId)
		<< "\n  last open error=" << g_LastHidOpenError;

	DrainInputReports(hid.get());

	WINUHID_STEAM_CONTROLLER_INPUT_REPORT expected;
	WinUHidSteamControllerInitializeInputReport(&expected);
	expected.SequenceNumber = 0x5A;
	expected.Buttons = WINUHID_STEAM_CONTROLLER_BUTTON_A |
		WINUHID_STEAM_CONTROLLER_BUTTON_STEAM |
		WINUHID_STEAM_CONTROLLER_TOUCH_RIGHT_PAD;
	expected.TriggerLeft = 0x1234;
	expected.RightPadX = -1234;
	expected.RightPadY = 2345;

	ASSERT_TRUE(WinUHidSteamControllerReportRawInput(controller.get(), &expected, sizeof(expected)));

	UCHAR actual[128];
	ASSERT_EQ(SDL_hid_read_timeout(hid.get(), actual, sizeof(actual), 1000), sizeof(expected));
	EXPECT_TRUE(RtlEqualMemory(actual, &expected, sizeof(expected)));
}

TEST(SteamController, RawHidCallbackForwardsOutputAndFeatureReports)
{
	CallbackData<ULONGLONG> rawState;
	auto controller = CreateTestController(RawHidCallback, &rawState);
	ASSERT_NE(controller, nullptr) << "Failed to create Steam Controller, last error " << GetLastError();

	HidDevicePtr hid = OpenTestSteamControllerHid();
	ASSERT_NE(hid, nullptr) << "Unable to detect Steam Controller HID device"
		<< DumpMatchingHidDevices(k_TestVendorId, k_TestProductId)
		<< "\n  last open error=" << g_LastHidOpenError;

	UCHAR hapticOutput[] = { 0x82, 0x34, 0x12, 0x56 };
	ASSERT_GT(SDL_hid_write(hid.get(), hapticOutput, sizeof(hapticOutput)), 0);
	EXPECT_CB_VALUE(rawState, MakeRawCallbackValue(
		WinUHidSteamControllerRawHidReportTypeOutput,
		sizeof(hapticOutput),
		hapticOutput[0],
		hapticOutput[1],
		hapticOutput[2],
		hapticOutput[3]));

	UCHAR featureReport[WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_BYTES] = {};
	featureReport[0] = WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_ID;
	featureReport[1] = 0x83;
	ASSERT_GT(SDL_hid_send_feature_report(hid.get(), featureReport, sizeof(featureReport)), 0);
	EXPECT_CB_VALUE(rawState, MakeRawCallbackValue(
		WinUHidSteamControllerRawHidReportTypeFeature,
		sizeof(featureReport),
		featureReport[0],
		featureReport[1],
		featureReport[2],
		featureReport[3]));

	UCHAR response[WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_BYTES] = {};
	response[0] = WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_ID;
	ASSERT_EQ(SDL_hid_get_feature_report(hid.get(), response, sizeof(response)), sizeof(response));
	EXPECT_EQ(response[0], WINUHID_STEAM_CONTROLLER_FEATURE_REPORT_ID);
	EXPECT_EQ(response[1], 0x83);
	EXPECT_EQ(response[2], 45);
}
