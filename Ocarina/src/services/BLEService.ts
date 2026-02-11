import {BleManager, Device, Characteristic} from 'react-native-ble-plx';
import {PermissionsAndroid, Platform, Alert} from 'react-native';

const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const CHAR_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';

class BLEService {
  manager: BleManager | null = null;
  device: Device | null = null;
  monitorSubscription: any = null;

  constructor() {
    this.initManager();
  }

  initManager() {
    try {
      this.manager = new BleManager();
      console.log('✓ BLE Manager initialized');
    } catch (e) {
      console.error('✗ BLE Manager init error:', e);
    }
  }

  async requestPermissions() {
    if (Platform.OS === 'android') {
      try {
        const granted = await PermissionsAndroid.requestMultiple([
          PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
          PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
          PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
        ]);
        const allGranted = Object.values(granted).every(v => v === 'granted');
        console.log(allGranted ? '✓ BLE permissions granted' : '✗ BLE permissions denied');
        return allGranted;
      } catch (err) {
        console.error('✗ Permission request error:', err);
        return false;
      }
    }
    return true;
  }

  async scan(onDeviceFound: (device: Device) => void, timeout = 10000) {
    if (!this.manager) throw new Error('BLE not initialized');
    
    const hasPermission = await this.requestPermissions();
    if (!hasPermission) throw new Error('Permissions denied');

    console.log('→ Starting BLE scan for Navi-Esp32...');
    
    this.manager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        console.error('✗ Scan error:', error);
        return;
      }
      if (device?.name?.includes('Navi')) {
        console.log(`✓ Found device: ${device.name} (${device.id})`);
        onDeviceFound(device);
      }
    });

    setTimeout(() => {
      this.manager?.stopDeviceScan();
      console.log('⊗ BLE scan stopped');
    }, timeout);
  }

  async connect(deviceId: string, deviceName?: string) {
    if (!this.manager) throw new Error('BLE not initialized');
    
    try {
      console.log(`→ Connecting to ${deviceName || deviceId}...`);
      this.device = await this.manager.connectToDevice(deviceId);
      console.log('✓ Device connected');
      
      console.log('→ Discovering services...');
      await this.device.discoverAllServicesAndCharacteristics();
      console.log('✓ Services discovered');
      
      // Test the connection by reading the characteristic
      try {
        const char = await this.device.readCharacteristicForService(
          SERVICE_UUID,
          CHAR_UUID
        );
        console.log('✓ Characteristic accessible');
      } catch (e) {
        console.warn('⚠ Could not read characteristic, but connection OK');
      }
      
      return this.device;
    } catch (error) {
      console.error('✗ Connection error:', error);
      this.device = null;
      throw error;
    }
  }

  async disconnect() {
    if (this.monitorSubscription) {
      this.monitorSubscription.remove();
      this.monitorSubscription = null;
      console.log('⊗ Monitoring stopped');
    }
    
    if (this.device) {
      try {
        await this.device.cancelConnection();
        console.log('⊗ Device disconnected');
      } catch (e) {
        console.error('✗ Disconnect error:', e);
      }
      this.device = null;
    }
  }

  async write(data: string) {
    if (!this.device) throw new Error('Not connected');
    
    try {
      console.log(`→ Sending: ${data}`);
      const encoded = Buffer.from(data).toString('base64');
      await this.device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        CHAR_UUID,
        encoded
      );
      console.log('✓ Data sent successfully');
    } catch (error) {
      console.error('✗ Write error:', error);
      throw error;
    }
  }

  async read() {
    if (!this.device) throw new Error('Not connected');
    
    try {
      const char = await this.device.readCharacteristicForService(
        SERVICE_UUID,
        CHAR_UUID
      );
      const data = Buffer.from(char.value || '', 'base64').toString();
      console.log(`← Received: ${data}`);
      return data;
    } catch (error) {
      console.error('✗ Read error:', error);
      throw error;
    }
  }

  async monitor(callback: (data: string) => void) {
    if (!this.device) throw new Error('Not connected');
    
    console.log('→ Starting notification monitor...');
    
    this.monitorSubscription = this.device.monitorCharacteristicForService(
      SERVICE_UUID,
      CHAR_UUID,
      (error, char) => {
        if (error) {
          console.error('✗ Monitor error:', error);
          return;
        }
        if (char?.value) {
          const data = Buffer.from(char.value, 'base64').toString();
          console.log(`← Notification: ${data}`);
          callback(data);
        }
      }
    );
    
    console.log('✓ Monitor started');
  }

  stopMonitoring() {
    if (this.monitorSubscription) {
      this.monitorSubscription.remove();
      this.monitorSubscription = null;
      console.log('⊗ Monitoring stopped');
    }
  }

  isConnected() {
    return this.device !== null;
  }

  getDeviceName() {
    return this.device?.name || 'Unknown';
  }

  getDeviceId() {
    return this.device?.id || '';
  }
}

export default new BLEService();
