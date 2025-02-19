// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

namespace base {
class SequencedTaskRunner;
class TimeDelta;
}  // namespace base

namespace device {
class BluetoothDevice;
class BluetoothSocketThread;
class BluetoothTestBlueZ;
}  // namespace device

namespace bluez {

class BluetoothBlueZTest;
class BluetoothAdapterProfileBlueZ;
class BluetoothAdvertisementBlueZ;
class BluetoothDeviceBlueZ;
class BluetoothLocalGattCharacteristicBlueZ;
class BluetoothLocalGattServiceBlueZ;
class BluetoothGattApplicationServiceProvider;
class BluetoothPairingBlueZ;

// The BluetoothAdapterBlueZ class implements BluetoothAdapter for platforms
// that use BlueZ.
//
// All methods are called from the dbus origin / UI thread and are generally
// not assumed to be thread-safe.
//
// This class interacts with sockets using the BluetoothSocketThread to ensure
// single-threaded calls, and posts tasks to the UI thread.
//
// Methods tolerate a shutdown scenario where BluetoothAdapterBlueZ::Shutdown
// causes IsPresent to return false just before the dbus system is shutdown but
// while references to the BluetoothAdapterBlueZ object still exists.
//
// When adding methods to this class verify shutdown behavior in
// BluetoothBlueZTest, Shutdown.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterBlueZ
    : public device::BluetoothAdapter,
      public bluez::BluetoothAdapterClient::Observer,
      public bluez::BluetoothDeviceClient::Observer,
      public bluez::BluetoothInputClient::Observer,
      public bluez::BluetoothAgentServiceProvider::Delegate {
 public:
  using ErrorCompletionCallback =
      base::Callback<void(const std::string& error_message)>;
  using ProfileRegisteredCallback =
      base::Callback<void(BluetoothAdapterProfileBlueZ* profile)>;
  using ServiceRecordCallback = base::Callback<void(uint32_t)>;
  using ServiceRecordErrorCallback =
      base::Callback<void(BluetoothServiceRecordBlueZ::ErrorCode)>;

  // Calls |init_callback| after a BluetoothAdapter is fully initialized.
  static base::WeakPtr<BluetoothAdapter> CreateAdapter(
      const InitCallback& init_callback);

  // BluetoothAdapter:
  void Shutdown() override;
  UUIDList GetUUIDs() const override;
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  uint32_t GetDiscoverableTimeout() const;
  bool IsDiscovering() const override;
  std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const device::BluetoothDiscoveryFilter& discovery_filter) override;
  void CreateRfcommService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;

  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override;

  void ResetAdvertising(
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override;

  device::BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  // These functions are specifically for use with ARC. They have no need to
  // exist for other platforms, hence we're putting them directly in the BlueZ
  // specific code.

  // Creates a service record with the SDP server running on this adapter. This
  // only creates the record, it does not create a listening socket for the
  // service.
  void CreateServiceRecord(const BluetoothServiceRecordBlueZ& record,
                           const ServiceRecordCallback& callback,
                           const ServiceRecordErrorCallback& error_callback);

  // Removes a service record from the SDP server. This would result in the
  // service not being discoverable in any further scans of the adapter. Any
  // sockets listening on this service will need to be closed separately.
  void RemoveServiceRecord(uint32_t handle,
                           const base::Closure& callback,
                           const ServiceRecordErrorCallback& error_callback);

  // Locates the device object by object path (the devices map and
  // BluetoothDevice methods are by address).
  BluetoothDeviceBlueZ* GetDeviceWithPath(const dbus::ObjectPath& object_path);

  // Announce to observers a device address change.
  void NotifyDeviceAddressChanged(BluetoothDeviceBlueZ* device,
                                  const std::string& old_address);

  // Returns the object path of the adapter.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Request a profile on the adapter for a custom service with a
  // specific UUID for the device at |device_path| to be sent to |delegate|.
  // If |device_path| is the empty string, incoming connections will be
  // assigned to |delegate|.  When the profile is
  // successfully registered, |success_callback| will be called with a pointer
  // to the profile which is managed by BluetoothAdapterBlueZ.  On failure,
  // |error_callback| will be called.
  void UseProfile(const device::BluetoothUUID& uuid,
                  const dbus::ObjectPath& device_path,
                  const bluez::BluetoothProfileManagerClient::Options& options,
                  bluez::BluetoothProfileServiceProvider::Delegate* delegate,
                  const ProfileRegisteredCallback& success_callback,
                  const ErrorCompletionCallback& error_callback);

  // Release use of a profile by a device.
  void ReleaseProfile(const dbus::ObjectPath& device_path,
                      BluetoothAdapterProfileBlueZ* profile);

  // Add a local GATT service to the list of services owned by this adapter.
  void AddLocalGattService(
      std::unique_ptr<BluetoothLocalGattServiceBlueZ> service);

  // Removes a local GATT service from the list of services owned by this
  // adapter and deletes it. If the service was registered, it is unregistered.
  void RemoveLocalGattService(BluetoothLocalGattServiceBlueZ* service);

  // Register a GATT service. The service must belong to this adapter.
  void RegisterGattService(
      BluetoothLocalGattServiceBlueZ* service,
      const base::Closure& callback,
      const device::BluetoothGattService::ErrorCallback& error_callback);

  // Unregister a GATT service. The service must already be registered.
  void UnregisterGattService(
      BluetoothLocalGattServiceBlueZ* service,
      const base::Closure& callback,
      const device::BluetoothGattService::ErrorCallback& error_callback);

  // Returns if a given service is currently registered.
  bool IsGattServiceRegistered(BluetoothLocalGattServiceBlueZ* service);

  // Send a notification for this characteristic that its value has been
  // updated. If the service that owns that characteristic is not registered,
  // this method will return false.
  bool SendValueChanged(BluetoothLocalGattCharacteristicBlueZ* characteristic,
                        const std::vector<uint8_t>& value);

  // Returns the object path of the adapter.
  dbus::ObjectPath GetApplicationObjectPath() const;

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothBlueZTest;
  friend class BluetoothBlueZTest_Shutdown_Test;
  friend class BluetoothBlueZTest_Shutdown_OnStartDiscovery_Test;
  friend class BluetoothBlueZTest_Shutdown_OnStartDiscoveryError_Test;
  friend class BluetoothBlueZTest_Shutdown_OnStopDiscovery_Test;
  friend class BluetoothBlueZTest_Shutdown_OnStopDiscoveryError_Test;
  friend class device::BluetoothTestBlueZ;

  // typedef for callback parameters that are passed to AddDiscoverySession
  // and RemoveDiscoverySession. This is used to queue incoming requests while
  // a call to BlueZ is pending.
  using DiscoveryParamTuple = std::tuple<device::BluetoothDiscoveryFilter*,
                                         base::Closure,
                                         DiscoverySessionErrorCallback>;
  using DiscoveryCallbackQueue = base::queue<DiscoveryParamTuple>;

  // Callback pair for the profile registration queue.
  using RegisterProfileCompletionPair =
      std::pair<base::Closure, ErrorCompletionCallback>;

  explicit BluetoothAdapterBlueZ(const InitCallback& init_callback);
  ~BluetoothAdapterBlueZ() override;

  // Init will get asynchronouly called once we know if Object Manager is
  // supported.
  void Init();

  // bluez::BluetoothAdapterClient::Observer override.
  void AdapterAdded(const dbus::ObjectPath& object_path) override;
  void AdapterRemoved(const dbus::ObjectPath& object_path) override;
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override;

  // bluez::BluetoothDeviceClient::Observer override.
  void DeviceAdded(const dbus::ObjectPath& object_path) override;
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // bluez::BluetoothInputClient::Observer override.
  void InputPropertyChanged(const dbus::ObjectPath& object_path,
                            const std::string& property_name) override;

  // bluez::BluetoothAgentServiceProvider::Delegate override.
  void Released() override;
  void RequestPinCode(const dbus::ObjectPath& device_path,
                      const PinCodeCallback& callback) override;
  void DisplayPinCode(const dbus::ObjectPath& device_path,
                      const std::string& pincode) override;
  void RequestPasskey(const dbus::ObjectPath& device_path,
                      const PasskeyCallback& callback) override;
  void DisplayPasskey(const dbus::ObjectPath& device_path,
                      uint32_t passkey,
                      uint16_t entered) override;
  void RequestConfirmation(const dbus::ObjectPath& device_path,
                           uint32_t passkey,
                           const ConfirmationCallback& callback) override;
  void RequestAuthorization(const dbus::ObjectPath& device_path,
                            const ConfirmationCallback& callback) override;
  void AuthorizeService(const dbus::ObjectPath& device_path,
                        const std::string& uuid,
                        const ConfirmationCallback& callback) override;
  void Cancel() override;

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent();
  void OnRegisterAgentError(const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to request that
  // the pairing agent be made the default.
  void OnRequestDefaultAgent();
  void OnRequestDefaultAgentError(const std::string& error_name,
                                  const std::string& error_message);

  // Internal method to obtain a BluetoothPairingBlueZ object for the device
  // with path |object_path|. Returns the existing pairing object if the device
  // already has one (usually an outgoing connection in progress) or a new
  // pairing object with the default pairing delegate if not. If no default
  // pairing object exists, NULL will be returned.
  BluetoothPairingBlueZ* GetPairing(const dbus::ObjectPath& object_path);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

#if defined(OS_CHROMEOS)
  // Set the adapter name to one chosen from the system information.
  void SetStandardChromeOSAdapterName();
#endif

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void DiscoverableChanged(bool discoverable);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(const base::Closure& callback,
                                 const ErrorCallback& error_callback,
                                 bool success);

  // BluetoothAdapter:
  void AddDiscoverySession(
      device::BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;
  void RemoveDiscoverySession(
      device::BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;
  void SetDiscoveryFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(const base::Closure& callback,
                        const DiscoverySessionErrorCallback& error_callback);
  void OnStartDiscoveryError(
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(const base::Closure& callback);
  void OnStopDiscoveryError(const DiscoverySessionErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  void OnPreSetDiscoveryFilter(
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback);
  void OnPreSetDiscoveryFilterError(
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback,
      device::UMABluetoothDiscoverySessionOutcome outcome);
  void OnSetDiscoveryFilter(
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback);
  void OnSetDiscoveryFilterError(
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method to register a profile.
  void OnRegisterProfile(const device::BluetoothUUID& uuid,
                         std::unique_ptr<BluetoothAdapterProfileBlueZ> profile);

  void SetProfileDelegate(
      const device::BluetoothUUID& uuid,
      const dbus::ObjectPath& device_path,
      bluez::BluetoothProfileServiceProvider::Delegate* delegate,
      const ProfileRegisteredCallback& success_callback,
      const ErrorCompletionCallback& error_callback);
  void OnRegisterProfileError(const device::BluetoothUUID& uuid,
                              const std::string& error_name,
                              const std::string& error_message);

  // Called by BluetoothAdapterProfileBlueZ when no users of a profile
  // remain.
  void RemoveProfile(const device::BluetoothUUID& uuid);

  // Processes the queued discovery requests. For each DiscoveryParamTuple in
  // the queue, this method will try to add a new discovery session. This method
  // is called whenever a pending D-Bus call to start or stop discovery has
  // ended (with either success or failure).
  void ProcessQueuedDiscoveryRequests();

  // Make the call to GattManager1 to unregister then re-register the GATT
  // application. If the ignore_unregister_failure flag is set, we attempt to
  // register even if the initial unregister call fails.
  void UpdateRegisteredApplication(
      bool ignore_unregister_failure,
      const base::Closure& callback,
      const device::BluetoothGattService::ErrorCallback& error_callback);

  // Make the call to GattManager1 to register the services currently
  // registered.
  void RegisterApplication(
      const base::Closure& callback,
      const device::BluetoothGattService::ErrorCallback& error_callback);

  // Register application, ignoring the given errors. Used to register a GATT
  // application even if a previous unregister application call fails.
  void RegisterApplicationOnError(
      const base::Closure& callback,
      const device::BluetoothGattService::ErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called by dbus:: on an error while trying to create or remove a service
  // record. Translates the error name/message into a
  // BluetoothServiceRecordBlueZ::ErrorCode value.
  void ServiceRecordErrorConnector(
      const ServiceRecordErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  InitCallback init_callback_;

  bool initialized_;

  // Set in |Shutdown()|, makes IsPresent()| return false.
  bool dbus_is_shutdown_;

  // Number of discovery sessions that have been added.
  int num_discovery_sessions_;

  // True, if there is a pending request to start or stop discovery.
  bool discovery_request_pending_;

  // List of queued requests to add new discovery sessions. While there is a
  // pending request to BlueZ to start or stop discovery, many requests from
  // within Chrome to start or stop discovery sessions may occur. We only
  // queue requests to add new sessions to be processed later. All requests to
  // remove a session while a call is pending immediately return failure. Note
  // that since BlueZ keeps its own reference count of applications that have
  // requested discovery, dropping our count to 0 won't necessarily result in
  // the controller actually stopping discovery if, for example, an application
  // other than Chrome, such as bt_console, was also used to start discovery.
  DiscoveryCallbackQueue discovery_request_queue_;

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // Instance of the D-Bus agent object used for pairing, initialized with
  // our own class as its delegate.
  std::unique_ptr<bluez::BluetoothAgentServiceProvider> agent_;

  // UI thread task runner and socket thread object used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // The profiles we have registered with the bluetooth daemon.
  std::map<device::BluetoothUUID, BluetoothAdapterProfileBlueZ*> profiles_;

  // Profiles that have been released and are pending removal.
  std::map<device::BluetoothUUID, BluetoothAdapterProfileBlueZ*>
      released_profiles_;

  // Queue of delegates waiting for a profile to register.
  std::map<device::BluetoothUUID, std::vector<RegisterProfileCompletionPair>*>
      profile_queues_;

  std::unique_ptr<device::BluetoothDiscoveryFilter> current_filter_;

  // List of GATT services that are owned by this adapter.
  std::map<dbus::ObjectPath, std::unique_ptr<BluetoothLocalGattServiceBlueZ>>
      owned_gatt_services_;

  // GATT services that are currently available on the GATT server.
  std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>
      registered_gatt_services_;

  // DBus Object Manager that acts as a service provider for all the services
  // that are registered with this adapter.
  std::unique_ptr<BluetoothGattApplicationServiceProvider>
      gatt_application_provider_;

  // List of advertisements registered with this adapter. This list is used
  // to ensure we unregister any advertisements that were registered with
  // this adapter on adapter shutdown. This is a sub-optimal solution since
  // we'll keep a list of all advertisements ever created by this adapter (the
  // unregistered ones will just be inactive). This will be fixed with
  // crbug.com/687396.
  std::vector<scoped_refptr<BluetoothAdvertisementBlueZ>> advertisements_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_
