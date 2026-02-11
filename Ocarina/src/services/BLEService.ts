import {BleManager, Device} from 'react-native-ble-plx';
import {PermissionsAndroid, Platform} from 'react-native';

const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const CHAR_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';

class BLEService {
  manager: BleManager | null = null;
  device: Device | null = null;

  constructor() {
    this.initManager();
  }

  initManager() {
    try {
      this.manager = new BleManager();
    } catch (e) {
      console.error('BLE Manager init error:', e);
    }
  }

  async requestPermissions() {
    if (Platform.OS === 'android') {
      const granted = await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
      ]);
      return Object.values(granted).every(v => v === 'granted');
    }
    return true;
  }

  async scan(onDeviceFound: (device: Device) => void, timeout = 10000) {
    if (!this.manager) throw new Error('BLE not initialized');
    
    const hasPermission = await this.requestPermissions();
    if (!hasPermission) throw new Error('Permissions denied');

    this.manager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        console.error('Scan error:', error);
        return;
      }
      if (device?.name?.includes('Ocarina')) {
        onDeviceFound(device);
      }
    });

    setTimeout(() => this.manager?.stopDeviceScan(), timeout);
  }

  async connect(deviceId: string) {
    if (!this.manager) throw new Error('BLE not initialized');
    this.device = await this.manager.connectToDevice(deviceId);
    await this.device.discoverAllServicesAndCharacteristics();
    return this.device;
  }

  async disconnect() {
    if (this.device) {
      await this.device.cancelConnection();
      this.device = null;
    }
  }

  async write(data: string) {
    if (!this.device) throw new Error('Not connected');
    const encoded = Buffer.from(data).toString('base64');
    await this.device.writeCharacteristicWithResponseForService(
      SERVICE_UUID,
      CHAR_UUID,
      encoded
    );
  }

  async read() {
    if (!this.device) throw new Error('Not connected');
    const char = await this.device.readCharacteristicForService(
      SERVICE_UUID,
      CHAR_UUID
    );
    return Buffer.from(char.value || '', 'base64').toString();
  }

  async monitor(callback: (data: string) => void) {
    if (!this.device) throw new Error('Not connected');
    this.device.monitorCharacteristicForService(
      SERVICE_UUID,
      CHAR_UUID,
      (error, char) => {
        if (error) {
          console.error('Monitor error:', error);
          return;
        }
        const data = Buffer.from(char?.value || '', 'base64').toString();
        callback(data);
      }
    );
  }

  isConnected() {
    return this.device !== null;
  }
}

export default new BLEService();
