import React, {useState, useEffect} from 'react';
import {View, Text, Pressable, ScrollView, StyleSheet, Alert} from 'react-native';

let BLEService: any = null;
try {
  BLEService = require('../services/BLEService').default;
} catch (e) {
  console.log('BLE not available:', e);
}

export default function BluetoothScreen({navigation}: any) {
  const [scanning, setScanning] = useState(false);
  const [devices, setDevices] = useState<any[]>([]);
  const [connected, setConnected] = useState<any>(null);

  useEffect(() => {
    return () => {
      if (BLEService) {
        BLEService.disconnect();
      }
    };
  }, []);

  if (!BLEService) {
    return (
      <View style={styles.container}>
        <View style={styles.background} />
        <View style={styles.header}>
          <View style={styles.headerRow}>
            <Pressable onPress={() => navigation.goBack()} style={styles.backButton}>
              <Text style={styles.backText}>‹</Text>
            </Pressable>
            <View style={styles.titleContainer}>
              <Text style={styles.title}>BLUETOOTH</Text>
              <View style={styles.titleUnderline} />
            </View>
          </View>
          <View style={styles.messageBox}>
            <Text style={styles.messageText}>⚠ BLE NOT AVAILABLE</Text>
            <Text style={styles.messageSubtext}>Install react-native-ble-plx and rebuild</Text>
          </View>
        </View>
      </View>
    );
  }

  const startScan = async () => {
    setScanning(true);
    setDevices([]);
    
    try {
      const foundDevices: any[] = [];
      await BLEService.scan((device) => {
        if (!foundDevices.find(d => d.id === device.id)) {
          foundDevices.push(device);
          setDevices([...foundDevices]);
        }
      }, 8000);
      
      setTimeout(() => setScanning(false), 8000);
    } catch (error) {
      Alert.alert('Error', 'Failed to scan: ' + error);
      setScanning(false);
    }
  };

  const connectDevice = async (deviceId: string, deviceName: string) => {
    try {
      const device = await BLEService.connect(deviceId);
      setConnected(device);
      Alert.alert('Connected', `Connected to ${deviceName}`);
    } catch (error) {
      Alert.alert('Error', 'Failed to connect: ' + error);
    }
  };

  const disconnectDevice = async () => {
    try {
      await BLEService.disconnect();
      setConnected(null);
      Alert.alert('Disconnected', 'Device disconnected');
    } catch (error) {
      Alert.alert('Error', 'Failed to disconnect: ' + error);
    }
  };

  const getSignalColor = (rssi?: number) => {
    if (!rssi) return '#00ffff';
    if (rssi > -60) return '#00ffff';
    if (rssi > -80) return '#ffff00';
    return '#ff0000';
  };

  return (
    <View style={styles.container}>
      <View style={styles.background} />
      
      <View style={styles.header}>
        <View style={styles.headerRow}>
          <Pressable onPress={() => navigation.goBack()} style={styles.backButton}>
            <Text style={styles.backText}>‹</Text>
          </Pressable>
          <View style={styles.titleContainer}>
            <Text style={styles.title}>BLUETOOTH</Text>
            <View style={styles.titleUnderline} />
          </View>
        </View>

        {connected ? (
          <Pressable onPress={disconnectDevice} style={styles.disconnectButton}>
            <View style={styles.disconnectOuter}>
              <View style={styles.disconnectInner}>
                <Text style={styles.disconnectText}>● CONNECTED TO {connected.name}</Text>
                <Text style={styles.disconnectSubtext}>Tap to disconnect</Text>
              </View>
            </View>
          </Pressable>
        ) : (
          <Pressable onPress={startScan} disabled={scanning} style={styles.scanButton}>
            <View style={[styles.scanOuter, scanning && styles.scanningOuter]}>
              <View style={styles.scanInner}>
                <Text style={styles.scanText}>
                  {scanning ? '⟳ SCANNING...' : '▶ SCAN FOR DEVICES'}
                </Text>
              </View>
            </View>
          </Pressable>
        )}

        <View style={styles.statsBar}>
          <View style={styles.stat}>
            <Text style={styles.statLabel}>DEVICES</Text>
            <Text style={styles.statValue}>{devices.length}</Text>
          </View>
          <View style={styles.statDivider} />
          <View style={styles.stat}>
            <Text style={styles.statLabel}>STATUS</Text>
            <Text style={styles.statValue}>{connected ? 'CONNECTED' : 'IDLE'}</Text>
          </View>
        </View>
      </View>

      <ScrollView style={styles.list} contentContainerStyle={styles.listContent}>
        {devices.map((device) => (
          <Pressable
            key={device.id}
            onPress={() => connectDevice(device.id, device.name || 'Unknown')}
            disabled={!!connected}
            style={styles.deviceCard}>
            <View style={styles.deviceOuter}>
              <View style={styles.deviceInner}>
                <View style={styles.deviceHeader}>
                  <View style={styles.deviceInfo}>
                    <Text style={styles.deviceName}>{device.name || 'Unknown Device'}</Text>
                    <Text style={styles.deviceId}>{device.id.substring(0, 17)}...</Text>
                  </View>
                  <Text style={[styles.rssi, {color: getSignalColor(device.rssi)}]}>
                    {device.rssi} dBm
                  </Text>
                </View>
              </View>
            </View>
          </Pressable>
        ))}
        
        {devices.length === 0 && !scanning && (
          <View style={styles.empty}>
            <Text style={styles.emptyTitle}>▫ NO DEVICES FOUND ▫</Text>
            <Text style={styles.emptySubtitle}>Press scan to search</Text>
          </View>
        )}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {flex: 1, backgroundColor: '#000'},
  background: {position: 'absolute', top: 0, bottom: 0, left: 0, right: 0, backgroundColor: '#001a1a', opacity: 0.1},
  header: {marginTop: 48, paddingHorizontal: 24},
  headerRow: {flexDirection: 'row', alignItems: 'center', marginBottom: 24},
  backButton: {marginRight: 16},
  backText: {fontSize: 28, color: '#00ffff'},
  titleContainer: {flex: 1},
  title: {fontSize: 28, fontWeight: 'bold', letterSpacing: 2, color: '#00ffff', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 10},
  titleUnderline: {height: 2, backgroundColor: '#00ffff', marginTop: 4, width: '66%'},
  scanButton: {marginBottom: 24},
  scanOuter: {borderWidth: 2, borderColor: '#00ffff', borderRadius: 8, padding: 2, backgroundColor: 'rgba(0, 26, 26, 0.2)', shadowColor: '#00ffff', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.5, shadowRadius: 15, elevation: 8},
  scanningOuter: {backgroundColor: 'rgba(0, 26, 26, 0.5)', shadowOpacity: 1},
  scanInner: {borderWidth: 1, borderColor: '#006666', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 20},
  scanText: {color: '#00ffff', textAlign: 'center', fontSize: 18, fontWeight: 'bold', letterSpacing: 3},
  disconnectButton: {marginBottom: 24},
  disconnectOuter: {borderWidth: 2, borderColor: '#00ff00', borderRadius: 8, padding: 2, backgroundColor: 'rgba(0, 26, 0, 0.3)', shadowColor: '#00ff00', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.8, shadowRadius: 15, elevation: 8},
  disconnectInner: {borderWidth: 1, borderColor: '#006600', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 20},
  disconnectText: {color: '#00ff00', textAlign: 'center', fontSize: 16, fontWeight: 'bold', letterSpacing: 2},
  disconnectSubtext: {color: '#00ff00', textAlign: 'center', fontSize: 11, marginTop: 4, opacity: 0.6},
  statsBar: {borderWidth: 2, borderColor: 'rgba(0, 255, 255, 0.4)', borderRadius: 8, padding: 12, backgroundColor: 'rgba(0, 0, 0, 0.6)', flexDirection: 'row', justifyContent: 'space-around', marginBottom: 16},
  stat: {alignItems: 'center'},
  statLabel: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 10, letterSpacing: 1.5},
  statValue: {color: '#00ffff', fontSize: 16, fontWeight: 'bold', marginTop: 4},
  statDivider: {width: 1, backgroundColor: 'rgba(0, 255, 255, 0.3)'},
  list: {flex: 1, paddingHorizontal: 24},
  listContent: {paddingBottom: 24},
  deviceCard: {marginBottom: 12},
  deviceOuter: {backgroundColor: 'rgba(0, 26, 26, 0.2)', borderWidth: 1, borderColor: 'rgba(0, 255, 255, 0.5)', borderRadius: 8, padding: 2},
  deviceInner: {borderWidth: 1, borderColor: 'rgba(0, 102, 102, 0.3)', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 16},
  deviceHeader: {flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between'},
  deviceInfo: {flex: 1},
  deviceName: {color: '#00ffff', fontSize: 16, fontWeight: 'bold', marginBottom: 4},
  deviceId: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 11, fontFamily: 'monospace'},
  rssi: {fontSize: 12, fontFamily: 'monospace', marginLeft: 12},
  empty: {alignItems: 'center', paddingVertical: 80},
  emptyTitle: {color: 'rgba(0, 255, 255, 0.4)', fontSize: 16, letterSpacing: 2},
  emptySubtitle: {color: 'rgba(0, 255, 255, 0.2)', fontSize: 13, marginTop: 8},
  messageBox: {borderWidth: 2, borderColor: 'rgba(255, 255, 0, 0.4)', borderRadius: 8, padding: 32, backgroundColor: 'rgba(0, 0, 0, 0.6)', alignItems: 'center'},
  messageText: {color: '#ffff00', fontSize: 18, fontWeight: 'bold', marginBottom: 8},
  messageSubtext: {color: 'rgba(255, 255, 0, 0.6)', fontSize: 13},
});
