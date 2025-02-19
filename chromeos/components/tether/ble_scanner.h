// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLE_SCANNER_H_
#define CHROMEOS_COMPONENTS_BLE_SCANNER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace device {
class BluetoothDevice;
class BluetoothDiscoverySession;
}

namespace chromeos {

namespace tether {

class BleSynchronizerBase;

// Performs BLE scans for devices which are advertising to this device.
class BleScanner : public device::BluetoothAdapter::Observer {
 public:
  class Observer {
   public:
    virtual void OnReceivedAdvertisementFromDevice(
        const cryptauth::RemoteDevice& remote_device,
        device::BluetoothDevice* bluetooth_device) {}
    virtual void OnDiscoverySessionStateChanged(bool discovery_session_active) {
    }
  };

  BleScanner(scoped_refptr<device::BluetoothAdapter> adapter,
             cryptauth::LocalDeviceDataProvider* local_device_data_provider,
             BleSynchronizerBase* ble_synchronizer);
  ~BleScanner() override;

  virtual bool RegisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);
  virtual bool UnregisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);

  // A discovery session should be active if at least one device has been
  // registered. However, discovery sessions are started and stopped
  // asynchronously, so these two functions may return different values.
  bool ShouldDiscoverySessionBeActive();
  bool IsDiscoverySessionActive();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // device::BluetoothAdapter::Observer
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* bluetooth_device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* bluetooth_device) override;

 protected:
  void NotifyReceivedAdvertisementFromDevice(
      const cryptauth::RemoteDevice& remote_device,
      device::BluetoothDevice* bluetooth_device);
  void NotifyDiscoverySessionStateChanged(bool discovery_session_active);

  scoped_refptr<device::BluetoothAdapter> adapter() { return adapter_; }

 private:
  friend class BleScannerTest;

  class ServiceDataProvider {
   public:
    virtual ~ServiceDataProvider() {}
    virtual const std::vector<uint8_t>* GetServiceDataForUUID(
        device::BluetoothDevice* bluetooth_device) = 0;
  };

  class ServiceDataProviderImpl : public ServiceDataProvider {
   public:
    ServiceDataProviderImpl();
    ~ServiceDataProviderImpl() override;
    const std::vector<uint8_t>* GetServiceDataForUUID(
        device::BluetoothDevice* bluetooth_device) override;
  };

  void SetTestDoubles(
      std::unique_ptr<ServiceDataProvider> service_data_provider,
      std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator);

  bool IsDeviceRegistered(const std::string& device_id);

  // A discovery session should stay active until it has been stopped. However,
  // due to bugs in Bluetooth code, it is possible for a discovery status to
  // transition to being off without a Stop() call ever succeeding. This
  // function corrects the state of Bluetooth if such a bug occurs.
  void ResetDiscoverySessionIfNotActive();

  void UpdateDiscoveryStatus();

  void EnsureDiscoverySessionActive();
  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnStartDiscoverySessionError();

  void EnsureDiscoverySessionNotActive();
  void OnDiscoverySessionStopped();
  void OnStopDiscoverySessionError();

  void HandleDeviceUpdated(device::BluetoothDevice* bluetooth_device);
  void CheckForMatchingScanFilters(device::BluetoothDevice* bluetooth_device,
                                   std::string& service_data);


  scoped_refptr<device::BluetoothAdapter> adapter_;
  cryptauth::LocalDeviceDataProvider* local_device_data_provider_;
  BleSynchronizerBase* ble_synchronizer_;

  std::unique_ptr<ServiceDataProvider> service_data_provider_;
  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;

  std::vector<cryptauth::RemoteDevice> registered_remote_devices_;
  base::ObserverList<Observer> observer_list_;

  bool is_initializing_discovery_session_ = false;
  bool is_stopping_discovery_session_ = false;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::unique_ptr<base::WeakPtrFactory<device::BluetoothDiscoverySession>>
      discovery_session_weak_ptr_factory_;

  base::WeakPtrFactory<BleScanner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLE_SCANNER_H_
