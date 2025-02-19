// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private_api.h"

#include <stddef.h>

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/devicetype.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_util.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;

namespace extensions {

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the customization ID setting.
const char kPropertyCustomizationID[] = "customizationId";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

// Key which corresponds to the board property in JS.
const char kPropertyBoard[] = "board";

// Key which corresponds to the isOwner property in JS.
const char kPropertyOwner[] = "isOwner";

// Key which corresponds to the clientId property in JS.
const char kPropertyClientId[] = "clientId";

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Key which corresponds to the timezone property in JS.
const char kPropertySupportedTimezones[] = "supportedTimezones";

// Key which corresponds to the large cursor A11Y property in JS.
const char kPropertyLargeCursorEnabled[] = "a11yLargeCursorEnabled";

// Key which corresponds to the sticky keys A11Y property in JS.
const char kPropertyStickyKeysEnabled[] = "a11yStickyKeysEnabled";

// Key which corresponds to the spoken feedback A11Y property in JS.
const char kPropertySpokenFeedbackEnabled[] = "a11ySpokenFeedbackEnabled";

// Key which corresponds to the high contrast mode A11Y property in JS.
const char kPropertyHighContrastEnabled[] = "a11yHighContrastEnabled";

// Key which corresponds to the screen magnifier A11Y property in JS.
const char kPropertyScreenMagnifierEnabled[] = "a11yScreenMagnifierEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyAutoclickEnabled[] = "a11yAutoClickEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyVirtualKeyboardEnabled[] = "a11yVirtualKeyboardEnabled";

// Key which corresponds to the caret highlight A11Y property in JS.
const char kPropertyCaretHighlightEnabled[] = "a11yCaretHighlightEnabled";

// Key which corresponds to the cursor highlight A11Y property in JS.
const char kPropertyCursorHighlightEnabled[] = "a11yCursorHighlightEnabled";

// Key which corresponds to the focus highlight A11Y property in JS.
const char kPropertyFocusHighlightEnabled[] = "a11yFocusHighlightEnabled";

// Key which corresponds to the select-to-speak A11Y property in JS.
const char kPropertySelectToSpeakEnabled[] = "a11ySelectToSpeakEnabled";

// Key which corresponds to the switch access A11Y property in JS.
const char kPropertySwitchAccessEnabled[] = "a11ySwitchAccessEnabled";

// Key which corresponds to the send-function-keys property in JS.
const char kPropertySendFunctionsKeys[] = "sendFunctionKeys";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";

// Key which corresponds to the sessionType property in JS.
const char kPropertySessionType[] = "sessionType";

// Key which corresponds to the "kiosk" value of the SessionType enum in JS.
const char kSessionTypeKiosk[] = "kiosk";

// Key which corresponds to the "public session" value of the SessionType enum
// in JS.
const char kSessionTypePublicSession[] = "public session";

// Key which corresponds to the "normal" value of the SessionType enum in JS.
const char kSessionTypeNormal[] = "normal";

// Key which corresponds to the playStoreStatus property in JS.
const char kPropertyPlayStoreStatus[] = "playStoreStatus";

// Key which corresponds to the "not available" value of the PlayStoreStatus
// enum in JS.
const char kPlayStoreStatusNotAvailable[] = "not available";

// Key which corresponds to the "available" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusAvailable[] = "available";

// Key which corresponds to the "enabled" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusEnabled[] = "enabled";

// Key which corresponds to the managedDeviceStatus property in JS.
const char kPropertyManagedDeviceStatus[] = "managedDeviceStatus";

// Value to which managedDeviceStatus property is set for unmanaged devices.
const char kManagedDeviceStatusNotManaged[] = "not managed";

// Value to which managedDeviceStatus property is set for managed devices.
const char kManagedDeviceStatusManaged[] = "managed";

// Key which corresponds to the deviceType property in JS.
const char kPropertyDeviceType[] = "deviceType";

// Value to which deviceType property is set for Chromebase.
const char kDeviceTypeChromebase[] = "chromebase";

// Value to which deviceType property is set for Chromebit.
const char kDeviceTypeChromebit[] = "chromebit";

// Value to which deviceType property is set for Chromebook.
const char kDeviceTypeChromebook[] = "chromebook";

// Value to which deviceType property is set for Chromebox.
const char kDeviceTypeChromebox[] = "chromebox";

// Value to which deviceType property is set when the specific type is unknown.
const char kDeviceTypeChromedevice[] = "chromedevice";

const struct {
  const char* api_name;
  const char* preference_name;
} kPreferencesMap[] = {
    {kPropertyLargeCursorEnabled, ash::prefs::kAccessibilityLargeCursorEnabled},
    {kPropertyStickyKeysEnabled, ash::prefs::kAccessibilityStickyKeysEnabled},
    {kPropertySpokenFeedbackEnabled,
     ash::prefs::kAccessibilitySpokenFeedbackEnabled},
    {kPropertyHighContrastEnabled,
     ash::prefs::kAccessibilityHighContrastEnabled},
    {kPropertyScreenMagnifierEnabled,
     ash::prefs::kAccessibilityScreenMagnifierEnabled},
    {kPropertyAutoclickEnabled, ash::prefs::kAccessibilityAutoclickEnabled},
    {kPropertyVirtualKeyboardEnabled,
     ash::prefs::kAccessibilityVirtualKeyboardEnabled},
    {kPropertyCaretHighlightEnabled,
     ash::prefs::kAccessibilityCaretHighlightEnabled},
    {kPropertyCursorHighlightEnabled,
     ash::prefs::kAccessibilityCursorHighlightEnabled},
    {kPropertyFocusHighlightEnabled,
     ash::prefs::kAccessibilityFocusHighlightEnabled},
    {kPropertySelectToSpeakEnabled,
     ash::prefs::kAccessibilitySelectToSpeakEnabled},
    {kPropertySwitchAccessEnabled,
     ash::prefs::kAccessibilitySwitchAccessEnabled},
    {kPropertySendFunctionsKeys, prefs::kLanguageSendFunctionKeys}};

const char* GetBoolPrefNameForApiProperty(const char* api_name) {
  for (size_t i = 0;
       i < (sizeof(kPreferencesMap)/sizeof(*kPreferencesMap));
       i++) {
    if (strcmp(kPreferencesMap[i].api_name, api_name) == 0)
      return kPreferencesMap[i].preference_name;
  }

  return NULL;
}

bool IsEnterpriseKiosk() {
  if (!chrome::IsRunningInForcedAppMode())
    return false;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

std::string GetClientId() {
  return IsEnterpriseKiosk()
             ? g_browser_process->metrics_service()->GetClientId()
             : std::string();
}

}  // namespace

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() {
}

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() {
}

bool ChromeosInfoPrivateGetFunction::RunAsync() {
  base::ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &list));
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string property_name;
    EXTENSION_FUNCTION_VALIDATE(list->GetString(i, &property_name));
    std::unique_ptr<base::Value> value = GetValue(property_name);
    if (value)
      result->Set(property_name, std::move(value));
  }
  SetResult(std::move(result));
  SendResponse(true);
  return true;
}

std::unique_ptr<base::Value> ChromeosInfoPrivateGetFunction::GetValue(
    const std::string& property_name) {
  if (property_name == kPropertyHWID) {
    std::string hwid;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid);
    return base::MakeUnique<base::Value>(hwid);
  }

  if (property_name == kPropertyCustomizationID) {
    std::string customization_id;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                  &customization_id);
    return base::MakeUnique<base::Value>(customization_id);
  }

  if (property_name == kPropertyHomeProvider) {
    const chromeos::DeviceState* cellular_device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            chromeos::NetworkTypePattern::Cellular());
    std::string home_provider_id;
    if (cellular_device)
      home_provider_id = cellular_device->home_provider_id();
    return base::MakeUnique<base::Value>(home_provider_id);
  }

  if (property_name == kPropertyInitialLocale) {
    return base::MakeUnique<base::Value>(
        chromeos::StartupUtils::GetInitialLocale());
  }

  if (property_name == kPropertyBoard) {
    return base::MakeUnique<base::Value>(base::SysInfo::GetLsbReleaseBoard());
  }

  if (property_name == kPropertyOwner) {
    return base::MakeUnique<base::Value>(
        user_manager::UserManager::Get()->IsCurrentUserOwner());
  }

  if (property_name == kPropertySessionType) {
    if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
      return base::MakeUnique<base::Value>(kSessionTypeKiosk);
    if (ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount())
      return base::MakeUnique<base::Value>(kSessionTypePublicSession);
    return base::MakeUnique<base::Value>(kSessionTypeNormal);
  }

  if (property_name == kPropertyPlayStoreStatus) {
    if (arc::IsArcAllowedForProfile(Profile::FromBrowserContext(context_)))
      return base::MakeUnique<base::Value>(kPlayStoreStatusEnabled);
    if (arc::IsArcAvailable())
      return base::MakeUnique<base::Value>(kPlayStoreStatusAvailable);
    return base::MakeUnique<base::Value>(kPlayStoreStatusNotAvailable);
  }

  if (property_name == kPropertyManagedDeviceStatus) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    if (connector->IsEnterpriseManaged()) {
      return base::MakeUnique<base::Value>(kManagedDeviceStatusManaged);
    }
    return base::MakeUnique<base::Value>(kManagedDeviceStatusNotManaged);
  }

  if (property_name == kPropertyDeviceType) {
    switch (chromeos::GetDeviceType()) {
      case chromeos::DeviceType::kChromebox:
        return base::MakeUnique<base::Value>(kDeviceTypeChromebox);
      case chromeos::DeviceType::kChromebase:
        return base::MakeUnique<base::Value>(kDeviceTypeChromebase);
      case chromeos::DeviceType::kChromebit:
        return base::MakeUnique<base::Value>(kDeviceTypeChromebit);
      case chromeos::DeviceType::kChromebook:
        return base::MakeUnique<base::Value>(kDeviceTypeChromebook);
      default:
        return base::MakeUnique<base::Value>(kDeviceTypeChromedevice);
    }
  }

  if (property_name == kPropertyClientId) {
    return base::MakeUnique<base::Value>(GetClientId());
  }

  if (property_name == kPropertyTimezone) {
    if (chromeos::system::PerUserTimezoneEnabled()) {
      const PrefService::Preference* timezone =
          Profile::FromBrowserContext(context_)->GetPrefs()->FindPreference(
              prefs::kUserTimezone);
      return base::MakeUnique<base::Value>(timezone->GetValue()->Clone());
    }
    // TODO(crbug.com/697817): Convert CrosSettings::Get to take a unique_ptr.
    return base::WrapUnique<base::Value>(
        chromeos::CrosSettings::Get()
            ->GetPref(chromeos::kSystemTimezone)
            ->DeepCopy());
  }

  if (property_name == kPropertySupportedTimezones) {
    std::unique_ptr<base::ListValue> values =
        chromeos::system::GetTimezoneList();
    // TODO(crbug.com/703565): remove std::move() once Xcode 9.0+ is required.
    return std::move(values);
  }

  const char* pref_name = GetBoolPrefNameForApiProperty(property_name.c_str());
  if (pref_name) {
    return base::MakeUnique<base::Value>(
        Profile::FromBrowserContext(context_)->GetPrefs()->GetBoolean(
            pref_name));
  }

  DLOG(ERROR) << "Unknown property request: " << property_name;
  return nullptr;
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() {
}

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() {
}

ExtensionFunction::ResponseAction ChromeosInfoPrivateSetFunction::Run() {
  std::string param_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &param_name));
  if (param_name == kPropertyTimezone) {
    std::string param_value;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &param_value));
    if (chromeos::system::PerUserTimezoneEnabled()) {
      Profile::FromBrowserContext(context_)->GetPrefs()->SetString(
          prefs::kUserTimezone, param_value);
    } else {
      chromeos::CrosSettings::Get()->Set(chromeos::kSystemTimezone,
                                         base::Value(param_value));
    }
  } else {
    const char* pref_name = GetBoolPrefNameForApiProperty(param_name.c_str());
    if (pref_name) {
      bool param_value;
      EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &param_value));
      Profile::FromBrowserContext(context_)->GetPrefs()->SetBoolean(
          pref_name,
          param_value);
    } else {
      return RespondNow(Error(kPropertyNotFound, param_name));
    }
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
