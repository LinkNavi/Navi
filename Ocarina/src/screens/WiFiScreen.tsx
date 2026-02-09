import React, {useState} from 'react';
import {View, Text, StyleSheet, TouchableOpacity, ScrollView} from 'react-native';

export default function WiFiScreen() {
  const [scanning, setScanning] = useState(false);
  const [networks, setNetworks] = useState([]);

  const startScan = () => {
    setScanning(true);
    // TODO: Send BLE command to ESP32 to scan
    setTimeout(() => {
      // Mock data for now
      setNetworks([
        {ssid: 'Network_1', rssi: -45, channel: 6, encryption: 'WPA2'},
        {ssid: 'Network_2', rssi: -67, channel: 11, encryption: 'WPA2'},
        {ssid: 'Network_3', rssi: -82, channel: 1, encryption: 'Open'},
      ]);
      setScanning(false);
    }, 2000);
  };

  return (
    <View style={styles.container}>
      <TouchableOpacity 
        style={[styles.scanButton, scanning && styles.scanning]}
        onPress={startScan}
        disabled={scanning}>
        <Text style={styles.scanText}>
          {scanning ? 'Scanning...' : 'Start Scan'}
        </Text>
      </TouchableOpacity>

      <ScrollView style={styles.networkList}>
        {networks.map((network, index) => (
          <View key={index} style={styles.networkItem}>
            <View style={styles.networkHeader}>
              <Text style={styles.ssid}>{network.ssid}</Text>
              <Text style={styles.rssi}>{network.rssi} dBm</Text>
            </View>
            <View style={styles.networkDetails}>
              <Text style={styles.detail}>Ch: {network.channel}</Text>
              <Text style={styles.detail}>{network.encryption}</Text>
            </View>
          </View>
        ))}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000',
    padding: 20,
  },
  scanButton: {
    backgroundColor: '#0a3d0a',
    padding: 20,
    borderRadius: 5,
    marginBottom: 20,
    borderWidth: 2,
    borderColor: '#0f0',
  },
  scanning: {
    opacity: 0.5,
  },
  scanText: {
    color: '#0f0',
    fontSize: 18,
    textAlign: 'center',
    fontWeight: 'bold',
  },
  networkList: {
    flex: 1,
  },
  networkItem: {
    backgroundColor: '#1a1a1a',
    padding: 15,
    marginBottom: 10,
    borderRadius: 5,
    borderLeftWidth: 3,
    borderLeftColor: '#0f0',
  },
  networkHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 5,
  },
  ssid: {
    color: '#0f0',
    fontSize: 16,
    fontWeight: 'bold',
  },
  rssi: {
    color: '#0f0',
    fontSize: 14,
  },
  networkDetails: {
    flexDirection: 'row',
    gap: 15,
  },
  detail: {
    color: '#0a0',
    fontSize: 12,
  },
});
