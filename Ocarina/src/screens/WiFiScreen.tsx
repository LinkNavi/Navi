import React, {useState, useEffect} from 'react';
import {View, Text, Pressable, ScrollView, Animated, StyleSheet, Alert} from 'react-native';

let BLEService: any = null;
try {
  BLEService = require('../services/BLEService').default;
} catch (e) {
  console.log('BLE not available:', e);
}

export default function WiFiScreen({navigation}: any) {
  const [scanning, setScanning] = useState(false);
  const [networks, setNetworks] = useState<any[]>([]);
  const [scanProgress] = useState(new Animated.Value(0));
  const [bleConnected, setBleConnected] = useState(false);

  useEffect(() => {
    checkBleConnection();
  }, []);

  const checkBleConnection = () => {
    if (BLEService && BLEService.isConnected()) {
      setBleConnected(true);
    }
  };

  const startScan = async () => {
    if (!BLEService || !BLEService.isConnected()) {
      Alert.alert('Not Connected', 'Please connect to ESP32 via Bluetooth first');
      return;
    }

    setScanning(true);
    setNetworks([]);
    
    Animated.timing(scanProgress, {toValue: 1, duration: 2000, useNativeDriver: false}).start();

    try {
      // Send WiFi scan command to ESP32
      await BLEService.write('WIFI_SCAN');
      
      // Monitor for WiFi scan results
      let receivedData = '';
      const timeout = setTimeout(() => {
        setScanning(false);
        scanProgress.setValue(0);
        if (networks.length === 0) {
          Alert.alert('No Results', 'No networks found or scan timed out');
        }
      }, 10000);

      BLEService.monitor((data: string) => {
        console.log('Received:', data);
        receivedData += data;
        
        // Check if we received complete WiFi data (format: WIFI:ssid,rssi,channel,encryption;...)
        if (data.includes('WIFI_END')) {
          clearTimeout(timeout);
          parseWifiData(receivedData);
          setScanning(false);
          scanProgress.setValue(0);
        }
      });

    } catch (error) {
      Alert.alert('Error', 'Failed to scan: ' + error);
      setScanning(false);
      scanProgress.setValue(0);
    }
  };

  const parseWifiData = (data: string) => {
    try {
      // Parse format: WIFI:ssid,rssi,channel,encryption;WIFI:ssid,rssi...WIFI_END
      const wifiLines = data.split('WIFI:').filter(line => line.length > 0 && !line.includes('WIFI_END'));
      const parsedNetworks = wifiLines.map(line => {
        const [ssid, rssi, channel, encryption] = line.replace('WIFI_END', '').split(',').map(s => s.trim());
        return {
          ssid: ssid || 'Hidden',
          rssi: parseInt(rssi) || -100,
          channel: parseInt(channel) || 0,
          encryption: encryption || 'Unknown',
          band: parseInt(channel) > 14 ? '5GHz' : '2.4GHz',
        };
      }).filter(n => n.ssid);

      setNetworks(parsedNetworks);
    } catch (error) {
      console.error('Parse error:', error);
      Alert.alert('Error', 'Failed to parse WiFi data');
    }
  };

  const getSignalBars = (rssi: number) => {
    if (rssi > -50) return '▰▰▰▰';
    if (rssi > -60) return '▰▰▰▱';
    if (rssi > -70) return '▰▰▱▱';
    if (rssi > -80) return '▰▱▱▱';
    return '▱▱▱▱';
  };

  const getSignalColor = (rssi: number) => {
    if (rssi > -50) return '#00ffff';
    if (rssi > -70) return '#ffff00';
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
            <Text style={styles.title}>WiFi SCANNER</Text>
            <View style={styles.titleUnderline} />
          </View>
        </View>

        {/* BLE Status Indicator */}
        <View style={[styles.statusIndicator, bleConnected ? styles.statusConnected : styles.statusDisconnected]}>
          <Text style={styles.statusText}>
            {bleConnected ? '● ESP32 CONNECTED' : '○ ESP32 NOT CONNECTED'}
          </Text>
        </View>

        <Pressable onPress={startScan} disabled={scanning} style={styles.scanButton}>
          <View style={[styles.scanOuter, scanning && styles.scanningOuter]}>
            <View style={styles.scanInner}>
              <Text style={styles.scanText}>
                {scanning ? '⟳ SCANNING...' : '▶ START SCAN'}
              </Text>
              {scanning && (
                <View style={styles.progressContainer}>
                  <Animated.View 
                    style={[styles.progressBar, {
                      width: scanProgress.interpolate({
                        inputRange: [0, 1],
                        outputRange: ['0%', '100%'],
                      }),
                    }]}
                  />
                </View>
              )}
            </View>
          </View>
        </Pressable>

        <View style={styles.statsBar}>
          <View style={styles.stat}>
            <Text style={styles.statLabel}>NETWORKS</Text>
            <Text style={styles.statValue}>{networks.length}</Text>
          </View>
          <View style={styles.statDivider} />
          <View style={styles.stat}>
            <Text style={styles.statLabel}>SECURED</Text>
            <Text style={styles.statValue}>
              {networks.filter(n => n.encryption !== 'Open').length}
            </Text>
          </View>
          <View style={styles.statDivider} />
          <View style={styles.stat}>
            <Text style={styles.statLabel}>OPEN</Text>
            <Text style={styles.statValue}>
              {networks.filter(n => n.encryption === 'Open').length}
            </Text>
          </View>
        </View>
      </View>

      <ScrollView style={styles.list} contentContainerStyle={styles.listContent}>
        {networks.map((network, index) => (
          <View key={index} style={styles.networkCard}>
            <View style={styles.networkOuter}>
              <View style={styles.networkInner}>
                <View style={styles.networkHeader}>
                  <Text style={styles.ssid} numberOfLines={1}>{network.ssid}</Text>
                  <Text style={[styles.bars, {color: getSignalColor(network.rssi)}]}>
                    {getSignalBars(network.rssi)}
                  </Text>
                </View>
                
                <View style={styles.networkFooter}>
                  <View style={styles.tags}>
                    <View style={styles.tag}>
                      <Text style={styles.tagText}>Ch {network.channel}</Text>
                    </View>
                    <View style={styles.tag}>
                      <Text style={styles.tagText}>{network.band}</Text>
                    </View>
                    <View style={styles.tag}>
                      <Text style={styles.tagText}>{network.encryption}</Text>
                    </View>
                  </View>
                  <Text style={[styles.rssi, {color: getSignalColor(network.rssi)}]}>
                    {network.rssi} dBm
                  </Text>
                </View>
              </View>
            </View>
          </View>
        ))}
        
        {networks.length === 0 && !scanning && (
          <View style={styles.empty}>
            <Text style={styles.emptyTitle}>▫ NO NETWORKS DETECTED ▫</Text>
            <Text style={styles.emptySubtitle}>
              {bleConnected ? 'Press scan to begin' : 'Connect to ESP32 first'}
            </Text>
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
  headerRow: {flexDirection: 'row', alignItems: 'center', marginBottom: 16},
  backButton: {marginRight: 16},
  backText: {fontSize: 28, color: '#00ffff'},
  titleContainer: {flex: 1},
  title: {fontSize: 28, fontWeight: 'bold', letterSpacing: 2, color: '#00ffff', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 10},
  titleUnderline: {height: 2, backgroundColor: '#00ffff', marginTop: 4, width: '66%'},
  statusIndicator: {marginBottom: 16, padding: 8, borderRadius: 6, alignItems: 'center'},
  statusConnected: {backgroundColor: 'rgba(0, 255, 0, 0.1)', borderWidth: 1, borderColor: 'rgba(0, 255, 0, 0.3)'},
  statusDisconnected: {backgroundColor: 'rgba(255, 0, 0, 0.1)', borderWidth: 1, borderColor: 'rgba(255, 0, 0, 0.3)'},
  statusText: {color: '#00ffff', fontSize: 12, letterSpacing: 1},
  scanButton: {marginBottom: 24},
  scanOuter: {borderWidth: 2, borderColor: '#00ffff', borderRadius: 8, padding: 2, backgroundColor: 'rgba(0, 26, 26, 0.2)', shadowColor: '#00ffff', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.5, shadowRadius: 15, elevation: 8},
  scanningOuter: {backgroundColor: 'rgba(0, 26, 26, 0.5)', shadowOpacity: 1},
  scanInner: {borderWidth: 1, borderColor: '#006666', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 20},
  scanText: {color: '#00ffff', textAlign: 'center', fontSize: 18, fontWeight: 'bold', letterSpacing: 3},
  progressContainer: {marginTop: 12, backgroundColor: 'rgba(0, 0, 0, 0.8)', height: 8, borderRadius: 4, overflow: 'hidden', borderWidth: 1, borderColor: 'rgba(0, 255, 255, 0.3)'},
  progressBar: {height: '100%', backgroundColor: '#00ffff'},
  statsBar: {borderWidth: 2, borderColor: 'rgba(0, 255, 255, 0.4)', borderRadius: 8, padding: 12, backgroundColor: 'rgba(0, 0, 0, 0.6)', flexDirection: 'row', justifyContent: 'space-around', marginBottom: 16},
  stat: {alignItems: 'center'},
  statLabel: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 10, letterSpacing: 1.5},
  statValue: {color: '#00ffff', fontSize: 16, fontWeight: 'bold', marginTop: 4},
  statDivider: {width: 1, backgroundColor: 'rgba(0, 255, 255, 0.3)'},
  list: {flex: 1, paddingHorizontal: 24},
  listContent: {paddingBottom: 24},
  networkCard: {marginBottom: 12},
  networkOuter: {backgroundColor: 'rgba(0, 26, 26, 0.2)', borderWidth: 1, borderColor: 'rgba(0, 255, 255, 0.5)', borderRadius: 8, padding: 2},
  networkInner: {borderWidth: 1, borderColor: 'rgba(0, 102, 102, 0.3)', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 16},
  networkHeader: {flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8},
  ssid: {color: '#00ffff', fontSize: 16, fontWeight: 'bold', flex: 1},
  bars: {fontSize: 13, fontFamily: 'monospace', marginLeft: 8},
  networkFooter: {flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center'},
  tags: {flexDirection: 'row', gap: 8},
  tag: {backgroundColor: 'rgba(0, 26, 26, 0.8)', borderWidth: 1, borderColor: 'rgba(0, 255, 255, 0.4)', borderRadius: 4, paddingHorizontal: 8, paddingVertical: 4},
  tagText: {color: '#00ffff', fontSize: 11, fontFamily: 'monospace'},
  rssi: {fontSize: 11, fontFamily: 'monospace', marginLeft: 8},
  empty: {alignItems: 'center', paddingVertical: 80},
  emptyTitle: {color: 'rgba(0, 255, 255, 0.4)', fontSize: 16, letterSpacing: 2},
  emptySubtitle: {color: 'rgba(0, 255, 255, 0.2)', fontSize: 13, marginTop: 8},
});
