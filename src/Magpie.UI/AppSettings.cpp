#include "pch.h"
#include "AppSettings.h"
#include "StrUtils.h"
#include "Win32Utils.h"
#include "Logger.h"
#include "HotkeyHelper.h"
#include "ScalingProfile.h"
#include "CommonSharedConstants.h"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include "AutoStartHelper.h"
#include <Magpie.Core.h>
#include "ScalingModesService.h"
#include "JsonHelper.h"

using namespace Magpie::Core;


namespace winrt::Magpie::UI {

// 将热键存储为 uint32_t
// 不能存储为字符串，因为某些键有相同的名称，如句号和小键盘的点
static uint32_t EncodeHotkey(const HotkeySettings& hotkey) noexcept {
	uint32_t value = 0;
	value |= hotkey.code;
	if (hotkey.win) {
		value |= 0x100;
	}
	if (hotkey.ctrl) {
		value |= 0x200;
	}
	if (hotkey.alt) {
		value |= 0x400;
	}
	if (hotkey.shift) {
		value |= 0x800;
	}
	return value;
}

static void DecodeHotkey(uint32_t value, HotkeySettings& hotkey) noexcept {
	hotkey.code = value & 0xff;
	hotkey.win = value & 0x100;
	hotkey.ctrl = value & 0x200;
	hotkey.alt = value & 0x400;
	hotkey.shift = value & 0x800;
}

static void WriteScalingProfile(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ScalingProfile& scalingProfile) {
	writer.StartObject();
	if (!scalingProfile.name.empty()) {
		writer.Key("name");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.name).c_str());
		writer.Key("packaged");
		writer.Bool(scalingProfile.isPackaged);
		writer.Key("pathRule");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.pathRule).c_str());
		writer.Key("classNameRule");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.classNameRule).c_str());
	}

	writer.Key("scalingMode");
	writer.Int(scalingProfile.scalingMode);
	writer.Key("captureMode");
	writer.Uint((uint32_t)scalingProfile.captureMode);
	writer.Key("multiMonitorUsage");
	writer.Uint((uint32_t)scalingProfile.multiMonitorUsage);
	writer.Key("graphicsAdapter");
	writer.Uint(scalingProfile.graphicsAdapter);

	writer.Key("disableWindowResizing");
	writer.Bool(scalingProfile.IsDisableWindowResizing());
	writer.Key("3DGameMode");
	writer.Bool(scalingProfile.Is3DGameMode());
	writer.Key("showFPS");
	writer.Bool(scalingProfile.IsShowFPS());
	writer.Key("VSync");
	writer.Bool(scalingProfile.IsVSync());
	writer.Key("tripleBuffering");
	writer.Bool(scalingProfile.IsTripleBuffering());
	writer.Key("reserveTitleBar");
	writer.Bool(scalingProfile.IsReserveTitleBar());
	writer.Key("adjustCursorSpeed");
	writer.Bool(scalingProfile.IsAdjustCursorSpeed());
	writer.Key("drawCursor");
	writer.Bool(scalingProfile.IsDrawCursor());
	writer.Key("disableDirectFlip");
	writer.Bool(scalingProfile.IsDisableDirectFlip());

	writer.Key("cursorScaling");
	writer.Uint((uint32_t)scalingProfile.cursorScaling);
	writer.Key("customCursorScaling");
	writer.Double(scalingProfile.customCursorScaling);
	writer.Key("cursorInterpolationMode");
	writer.Uint((uint32_t)scalingProfile.cursorInterpolationMode);

	writer.Key("croppingEnabled");
	writer.Bool(scalingProfile.isCroppingEnabled);
	writer.Key("cropping");
	writer.StartObject();
	writer.Key("left");
	writer.Double(scalingProfile.cropping.Left);
	writer.Key("top");
	writer.Double(scalingProfile.cropping.Top);
	writer.Key("right");
	writer.Double(scalingProfile.cropping.Right);
	writer.Key("bottom");
	writer.Double(scalingProfile.cropping.Bottom);
	writer.EndObject();

	writer.EndObject();
}

static bool LoadScalingProfile(
	const rapidjson::GenericObject<true, rapidjson::Value>& scalingConfigObj,
	ScalingProfile& scalingProfile,
	bool isDefault = false
) {
	if (!isDefault) {
		if (!JsonHelper::ReadString(scalingConfigObj, "name", scalingProfile.name, true)) {
			return false;
		}

		if (!JsonHelper::ReadBool(scalingConfigObj, "packaged", scalingProfile.isPackaged, true)) {
			return false;
		}

		if (!JsonHelper::ReadString(scalingConfigObj, "pathRule", scalingProfile.pathRule, true)) {
			return false;
		}

		if (!JsonHelper::ReadString(scalingConfigObj, "classNameRule", scalingProfile.classNameRule, true)) {
			return false;
		}
	}

	if (!JsonHelper::ReadInt(scalingConfigObj, "scalingMode", scalingProfile.scalingMode)) {
		return false;
	}

	if (!JsonHelper::ReadUInt(scalingConfigObj, "captureMode", (uint32_t&)scalingProfile.captureMode)) {
		return false;
	}

	if (!JsonHelper::ReadUInt(scalingConfigObj, "multiMonitorUsage", (uint32_t&)scalingProfile.multiMonitorUsage)) {
		return false;
	}

	if (!JsonHelper::ReadUInt(scalingConfigObj, "graphicsAdapter", scalingProfile.graphicsAdapter)) {
		return false;
	}

	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "disableWindowResizing", MagFlags::DisableDirectFlip, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "3DGameMode", MagFlags::Is3DGameMode, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "showFPS", MagFlags::ShowFPS, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "VSync", MagFlags::VSync, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "tripleBuffering", MagFlags::TripleBuffering, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "reserveTitleBar", MagFlags::ReserveTitleBar, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "adjustCursorSpeed", MagFlags::AdjustCursorSpeed, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "drawCursor", MagFlags::DrawCursor, scalingProfile.flags)) {
		return false;
	}
	if (!JsonHelper::ReadBoolFlag(scalingConfigObj, "disableDirectFlip", MagFlags::DisableDirectFlip, scalingProfile.flags)) {
		return false;
	}

	if (!JsonHelper::ReadUInt(scalingConfigObj, "cursorScaling", (uint32_t&)scalingProfile.cursorScaling)) {
		return false;
	}
	if (!JsonHelper::ReadFloat(scalingConfigObj, "customCursorScaling", scalingProfile.customCursorScaling)) {
		return false;
	}
	if (!JsonHelper::ReadUInt(scalingConfigObj, "cursorInterpolationMode", (uint32_t&)scalingProfile.cursorInterpolationMode)) {
		return false;
	}

	{
		if (!JsonHelper::ReadBool(scalingConfigObj, "croppingEnabled", scalingProfile.isCroppingEnabled)) {
			return false;
		}

		auto croppingNode = scalingConfigObj.FindMember("cropping");
		if (croppingNode != scalingConfigObj.MemberEnd()) {
			if (!croppingNode->value.IsObject()) {
				return false;
			}

			const auto& croppingObj = croppingNode->value.GetObj();

			if (!JsonHelper::ReadFloat(croppingObj, "left", scalingProfile.cropping.Left, true)
				|| !JsonHelper::ReadFloat(croppingObj, "top", scalingProfile.cropping.Top, true)
				|| !JsonHelper::ReadFloat(croppingObj, "right", scalingProfile.cropping.Right, true)
				|| !JsonHelper::ReadFloat(croppingObj, "bottom", scalingProfile.cropping.Bottom, true)
				) {
				return false;
			}
		}
	}

	return true;
}



bool AppSettings::Initialize() {
	Logger& logger = Logger::Get();

	// 若程序所在目录存在配置文件则为便携模式
	_isPortableMode = Win32Utils::FileExists(
		StrUtils::ConcatW(CommonSharedConstants::CONFIG_DIR, CommonSharedConstants::CONFIG_NAME).c_str());
	_UpdateConfigPath();

	if (!Win32Utils::FileExists(_configPath.c_str())) {
		logger.Info("不存在配置文件");
		// 只有不存在配置文件时才生成默认缩放模式
		_SetDefaultScalingModes();
		_SetDefaultHotkeys();
		return true;
	}

	std::string configText;
	if (!Win32Utils::ReadTextFile(_configPath.c_str(), configText)) {
		logger.Error("读取配置文件失败");
		return false;
	}

	if (!_LoadSettings(configText)) {
		logger.Error("解析配置文件失败");
		return false;
	}
	
	_SetDefaultHotkeys();
	return true;
}

bool AppSettings::Save() {
	if (!Win32Utils::CreateDir(_configDir)) {
		Logger::Get().Error("创建配置文件夹失败");
		return false;
	}

	rapidjson::StringBuffer json;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(json);
	writer.StartObject();

	writer.Key("theme");
	writer.Uint(_theme);

	{
		if (HWND hwndMain = (HWND)Application::Current().as<App>().HwndMain()) {
			WINDOWPLACEMENT wp{};
			wp.length = sizeof(wp);
			if (GetWindowPlacement(hwndMain, &wp)) {
				_windowRect = {
					(float)wp.rcNormalPosition.left,
					(float)wp.rcNormalPosition.top,
					float(wp.rcNormalPosition.right - wp.rcNormalPosition.left),
					float(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top)
				};
				_isWindowMaximized = wp.showCmd == SW_MAXIMIZE;
				
			} else {
				Logger::Get().Win32Error("GetWindowPlacement 失败");
			}
		}

		writer.Key("windowPos");
		writer.StartObject();
		writer.Key("x");
		writer.Int((int)std::lroundf(_windowRect.X));
		writer.Key("y");
		writer.Int((int)std::lroundf(_windowRect.Y));
		writer.Key("width");
		writer.Int((int)std::lroundf(_windowRect.Width));
		writer.Key("height");
		writer.Int((int)std::lroundf(_windowRect.Height));
		writer.Key("maximized");
		writer.Bool(_isWindowMaximized);
		writer.EndObject();
	}

	writer.Key("hotkeys");
	writer.StartObject();
	writer.Key("scale");
	writer.Uint(EncodeHotkey(_hotkeys[(size_t)HotkeyAction::Scale]));
	writer.Key("overlay");
	writer.Uint(EncodeHotkey(_hotkeys[(size_t)HotkeyAction::Overlay]));
	writer.EndObject();

	writer.Key("autoRestore");
	writer.Bool(_isAutoRestore);
	writer.Key("downCount");
	writer.Uint(_downCount);
	writer.Key("debugMode");
	writer.Bool(_isDebugMode);
	writer.Key("disableEffectCache");
	writer.Bool(_isDisableEffectCache);
	writer.Key("saveEffectSources");
	writer.Bool(_isSaveEffectSources);
	writer.Key("warningsAreErrors");
	writer.Bool(_isWarningsAreErrors);
	writer.Key("simulateExclusiveFullscreen");
	writer.Bool(_isSimulateExclusiveFullscreen);
	writer.Key("alwaysRunAsElevated");
	writer.Bool(_isAlwaysRunAsElevated);
	writer.Key("showTrayIcon");
	writer.Bool(_isShowTrayIcon);
	writer.Key("inlineParams");
	writer.Bool(_isInlineParams);

	if (!_downscalingEffect.name.empty()) {
		writer.Key("downscalingEffect");
		writer.StartObject();
		writer.Key("name");
		writer.String(StrUtils::UTF16ToUTF8(_downscalingEffect.name).c_str());
		if (!_downscalingEffect.parameters.empty()) {
			writer.Key("parameters");
			writer.StartObject();
			for (const auto& [name, value] : _downscalingEffect.parameters) {
				writer.Key(StrUtils::UTF16ToUTF8(name).c_str());
				writer.Double(value);
			}
			writer.EndObject();
		}
		writer.EndObject();
	}

	ScalingModesService::Get().Export(writer);

	writer.Key("scalingProfiles");
	writer.StartArray();
	WriteScalingProfile(writer, _defaultScalingProfile);
	for (const ScalingProfile& rule : _scalingProfiles) {
		WriteScalingProfile(writer, rule);
	}
	writer.EndArray();

	writer.EndObject();

	if (!Win32Utils::WriteTextFile(_configPath.c_str(), { json.GetString(), json.GetLength() })) {
		Logger::Get().Error("保存配置失败");
		return false;
	}

	return true;
}

void AppSettings::IsPortableMode(bool value) {
	if (_isPortableMode == value) {
		return;
	}

	if (!value) {
		// 关闭便携模式需删除本地配置文件
		// 不关心是否成功
		DeleteFile(StrUtils::ConcatW(_configDir, CommonSharedConstants::CONFIG_NAME).c_str());
	}

	Logger::Get().Info(value ? "已开启便携模式" : "已关闭便携模式");

	_isPortableMode = value;
	_UpdateConfigPath();
}

void AppSettings::Theme(uint32_t value) {
	assert(value <= 2);

	if (_theme == value) {
		return;
	}

	_theme = value;
	_themeChangedEvent(value);
}

void AppSettings::SetHotkey(HotkeyAction action, const Magpie::UI::HotkeySettings& value) {
	if (_hotkeys[(size_t)action] == value) {
		return;
	}

	_hotkeys[(size_t)action] = value;
	Logger::Get().Info(fmt::format("热键 {} 已更改为 {}", HotkeyHelper::ToString(action), StrUtils::UTF16ToUTF8(value.ToString())));
	_hotkeyChangedEvent(action);
}

void AppSettings::IsAutoRestore(bool value) noexcept {
	if (_isAutoRestore == value) {
		return;
	}

	_isAutoRestore = value;
	_isAutoRestoreChangedEvent(value);
}

void AppSettings::DownCount(uint32_t value) noexcept {
	if (_downCount == value) {
		return;
	}

	_downCount = value;
	_downCountChangedEvent(value);
}

void AppSettings::IsAlwaysRunAsElevated(bool value) noexcept {
	if (_isAlwaysRunAsElevated == value) {
		return;
	}

	_isAlwaysRunAsElevated = value;
	std::wstring arguments;
	if (AutoStartHelper::IsAutoStartEnabled(arguments)) {
		// 更新启动任务
		AutoStartHelper::EnableAutoStart(value, _isShowTrayIcon ? arguments.c_str() : nullptr);
	}
}

void AppSettings::IsShowTrayIcon(bool value) noexcept {
	if (_isShowTrayIcon == value) {
		return;
	}

	_isShowTrayIcon = value;
	_isShowTrayIconChangedEvent(value);
}

// 遇到不合法的配置项会失败，因此用户不应直接编辑配置文件
bool AppSettings::_LoadSettings(std::string& text) {
	if (text.empty()) {
		Logger::Get().Info("配置文件为空");
		return true;
	}

	rapidjson::Document doc;
	doc.ParseInsitu(text.data());
	if (doc.HasParseError()) {
		Logger::Get().Error(fmt::format("解析配置失败\n\t错误码：{}", (int)doc.GetParseError()));
		return false;
	}

	if (!doc.IsObject()) {
		return false;
	}

	auto root = ((const rapidjson::Document&)doc).GetObj();

	if (!JsonHelper::ReadUInt(root, "theme", _theme)) {
		return false;
	}

	{
		auto windowPosNode = root.FindMember("windowPos");
		if (windowPosNode != root.MemberEnd()) {
			if (!windowPosNode->value.IsObject()) {
				return false;
			}

			const auto& windowRectObj = windowPosNode->value.GetObj();

			if (!JsonHelper::ReadFloat(windowRectObj, "x", _windowRect.X, true)) {
				return false;
			}
			if (!JsonHelper::ReadFloat(windowRectObj, "y", _windowRect.Y, true)) {
				return false;
			}
			if (!JsonHelper::ReadFloat(windowRectObj, "width", _windowRect.Width, true)) {
				return false;
			}
			if (!JsonHelper::ReadFloat(windowRectObj, "height", _windowRect.Height, true)) {
				return false;
			}
			if (!JsonHelper::ReadBool(windowRectObj, "maximized", _isWindowMaximized, true)) {
				return false;
			}
		}
	}

	{
		auto hotkeysNode = root.FindMember("hotkeys");
		if (hotkeysNode != root.MemberEnd()) {
			if (!hotkeysNode->value.IsObject()) {
				return false;
			}

			const auto& hotkeysObj = hotkeysNode->value.GetObj();

			auto scaleNode = hotkeysObj.FindMember("scale");
			if (scaleNode != hotkeysObj.MemberEnd() && scaleNode->value.IsUint()) {
				DecodeHotkey(scaleNode->value.GetUint(), _hotkeys[(size_t)HotkeyAction::Scale]);
			}

			auto overlayNode = hotkeysObj.FindMember("overlay");
			if (overlayNode != hotkeysObj.MemberEnd() && overlayNode->value.IsUint()) {
				DecodeHotkey(overlayNode->value.GetUint(), _hotkeys[(size_t)HotkeyAction::Overlay]);
			}
		}
	}

	if (!JsonHelper::ReadBool(root, "autoRestore", _isAutoRestore)) {
		return false;
	}

	if (!JsonHelper::ReadUInt(root, "downCount", _downCount)) {
		return false;
	}

	if (!JsonHelper::ReadBool(root, "debugMode", _isDebugMode)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "disableEffectCache", _isDisableEffectCache)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "saveEffectSources", _isSaveEffectSources)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "warningsAreErrors", _isWarningsAreErrors)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "simulateExclusiveFullscreen", _isSimulateExclusiveFullscreen)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "alwaysRunAsElevated", _isAlwaysRunAsElevated)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "showTrayIcon", _isShowTrayIcon)) {
		return false;
	}
	if (!JsonHelper::ReadBool(root, "inlineParams", _isInlineParams)) {
		return false;
	}

	{
		auto downscalingEffectNode = root.FindMember("downscalingEffect");
		if (downscalingEffectNode != root.MemberEnd()) {
			if (!downscalingEffectNode->value.IsObject()) {
				return false;
			}

			auto downscalingEffectObj = downscalingEffectNode->value.GetObj();

			if (!JsonHelper::ReadString(downscalingEffectObj, "name", _downscalingEffect.name)) {
				return false;
			}

			{
				auto parametersNode = downscalingEffectObj.FindMember("parameters");
				if (parametersNode != downscalingEffectObj.MemberEnd()) {
					if (!parametersNode->value.IsObject()) {
						return false;
					}

					auto paramsObj = parametersNode->value.GetObj();
					_downscalingEffect.parameters.reserve(paramsObj.MemberCount());
					for (auto& param : paramsObj) {
						std::wstring name = StrUtils::UTF8ToUTF16(param.name.GetString());

						if (!param.value.IsNumber()) {
							return false;
						}
						_downscalingEffect.parameters[name] = param.value.GetFloat();
					}
				}
			}
		}
	}

	if (!ScalingModesService::Get().Import(root)) {
		return false;
	}

	{
		auto scaleProfilesNode = root.FindMember("scalingProfiles");
		if (scaleProfilesNode != root.MemberEnd()) {
			if (!scaleProfilesNode->value.IsArray()) {
				return false;
			}

			const auto& scaleProfilesArray = scaleProfilesNode->value.GetArray();

			const rapidjson::SizeType size = scaleProfilesArray.Size();
			if (size > 0) {
				if (!scaleProfilesArray[0].IsObject()) {
					return false;
				}

				if (!LoadScalingProfile(scaleProfilesArray[0].GetObj(), _defaultScalingProfile, true)) {
					return false;
				}

				if (size > 1) {
					_scalingProfiles.resize((size_t)size - 1);
					for (rapidjson::SizeType i = 1; i < size; ++i) {
						if (!scaleProfilesArray[i].IsObject()) {
							return false;
						}

						ScalingProfile& rule = _scalingProfiles[(size_t)i - 1];

						if (!LoadScalingProfile(scaleProfilesArray[i].GetObj(), rule)) {
							return false;
						}

						if (rule.name.empty() || rule.pathRule.empty() || rule.classNameRule.empty()) {
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

void AppSettings::_SetDefaultHotkeys() {
	HotkeySettings& scaleHotkey = _hotkeys[(size_t)HotkeyAction::Scale];
	if (scaleHotkey.IsEmpty()) {
		scaleHotkey.win = true;
		scaleHotkey.shift = true;
		scaleHotkey.code = 'A';
	}

	HotkeySettings& overlayHotkey = _hotkeys[(size_t)HotkeyAction::Overlay];
	if (overlayHotkey.IsEmpty()) {
		overlayHotkey.win = true;
		overlayHotkey.shift = true;
		overlayHotkey.code = 'D';
	}
}

void AppSettings::_SetDefaultScalingModes() {
	_scalingModes.resize(7);

	// Lanczos
	{
		auto& lanczos = _scalingModes[0];
		lanczos.name = L"Lanczos";

		auto& lanczosEffect = lanczos.effects.emplace_back();
		lanczosEffect.name = L"Lanczos";
		lanczosEffect.scalingType = ::Magpie::Core::ScalingType::Fit;
	}
	// FSR
	{
		auto& fsr = _scalingModes[1];
		fsr.name = L"FSR";

		fsr.effects.resize(2);
		auto& easu = fsr.effects[0];
		easu.name = L"FSR\\FSR_EASU";
		easu.scalingType = ::Magpie::Core::ScalingType::Fit;
		auto& rcas = fsr.effects[1];
		rcas.name = L"FSR\\FSR_RCAS";
		rcas.parameters[L"sharpness"] = 0.87f;
	}
	// FSRCNNX
	{
		auto& fsrcnnx = _scalingModes[2];
		fsrcnnx.name = L"FSRCNNX";
		fsrcnnx.effects.emplace_back().name = L"FSRCNNX\\FSRCNNX";
	}
	// ACNet
	{
		auto& acnet = _scalingModes[3];
		acnet.name = L"ACNet";
		acnet.effects.emplace_back().name = L"ACNet";
	}
	// Anime4K
	{
		auto& anime4k = _scalingModes[4];
		anime4k.name = L"Anime4K";
		anime4k.effects.emplace_back().name = L"Anime4K\\Anime4K_Upscale_Denoise_L";
	}
	// CRT-Geom
	{
		auto& crtGeom = _scalingModes[5];
		crtGeom.name = L"CRT-Geom";

		auto& crtGeomEffect = crtGeom.effects.emplace_back();
		crtGeomEffect.name = L"CRT\\CRT_Geom";
		crtGeomEffect.scalingType = ::Magpie::Core::ScalingType::Fit;
		crtGeomEffect.parameters[L"curvature"] = 0.0f;
		crtGeomEffect.parameters[L"cornerSize"] = 0.001f;
		crtGeomEffect.parameters[L"CRTGamma"] = 1.5f;
		crtGeomEffect.parameters[L"monitorGamma"] = 2.2f;
		crtGeomEffect.parameters[L"interlace"] = 0.0f;
	}
	// Integer Scale 2x
	{
		auto& integer2x = _scalingModes[6];
		integer2x.name = L"Integer Scale 2x";

		auto& nearest = integer2x.effects.emplace_back();
		nearest.name = L"Nearest";
		nearest.scalingType = ::Magpie::Core::ScalingType::Normal;
		nearest.scale = { 2.0f,2.0f };
	}

	// 降采样效果默认为 Bicubic (B=0, C=0.5)
	_downscalingEffect.name = L"Bicubic";
	_downscalingEffect.parameters[L"paramB"] = 0.0f;
	_downscalingEffect.parameters[L"paramC"] = 0.5f;
}

void AppSettings::_UpdateConfigPath() noexcept {
	if (_isPortableMode) {
		wchar_t curDir[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, curDir);

		_configDir = curDir;
		if (_configDir.back() != L'\\') {
			_configDir.push_back(L'\\');
		}
		_configDir += CommonSharedConstants::CONFIG_DIR;
	} else {
		wchar_t localAppDataDir[MAX_PATH];
		HRESULT hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppDataDir);
		if (SUCCEEDED(hr)) {
			_configDir = StrUtils::ConcatW(
				localAppDataDir,
				localAppDataDir[StrUtils::StrLen(localAppDataDir) - 1] == L'\\' ? L"Magpie\\" : L"\\Magpie\\",
				CommonSharedConstants::CONFIG_DIR
			);
		} else {
			Logger::Get().ComError("SHGetFolderPath 失败", hr);
			_configDir = CommonSharedConstants::CONFIG_DIR;
		}
	}

	_configPath = _configDir + CommonSharedConstants::CONFIG_NAME;

	// 确保 ConfigDir 存在
	Win32Utils::CreateDir(_configDir.c_str(), true);
}

}
