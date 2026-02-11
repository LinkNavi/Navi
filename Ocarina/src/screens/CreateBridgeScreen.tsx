import React, {useState} from 'react';
import {View, Text, Pressable, ScrollView, TextInput, StyleSheet, Alert} from 'react-native';

export default function CreateBridgeScreen({navigation}: any) {
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [bridgeActive, setBridgeActive] = useState(false);
  const [connectedDevices, setConnectedDevices] = useState(0);

  const startBridge = () => {
    if (!ssid.trim()) {
      Alert.alert('Error', 'Please enter a network name');
      return;
    }
    
    Alert.alert('Bridge Started', `Creating bridge to ${ssid}`);
    setBridgeActive(true);
    // TODO: Send BLE command to ESP32
  };

  const stopBridge = () => {
    Alert.alert('Bridge Stopped', 'Network bridge deactivated');
    setBridgeActive(false);
    setConnectedDevices(0);
    // TODO: Send BLE command to ESP32
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
            <Text style={styles.title}>CREATE BRIDGE</Text>
            <View style={styles.titleUnderline} />
          </View>
        </View>

        <View style={styles.infoBox}>
          <Text style={styles.infoText}>🌉 Bridge Mode</Text>
          <Text style={styles.infoSubtext}>ESP32 connects to a WiFi network and shares it as a new access point</Text>
        </View>
      </View>

      <ScrollView style={styles.content} contentContainerStyle={styles.contentInner}>
        <View style={styles.section}>
          <Text style={styles.label}>TARGET NETWORK (SSID)</Text>
          <TextInput
            style={styles.input}
            value={ssid}
            onChangeText={setSsid}
            placeholder="Enter WiFi name"
            placeholderTextColor="rgba(0, 255, 255, 0.3)"
            editable={!bridgeActive}
          />
        </View>

        <View style={styles.section}>
          <Text style={styles.label}>PASSWORD</Text>
          <TextInput
            style={styles.input}
            value={password}
            onChangeText={setPassword}
            placeholder="Enter password"
            placeholderTextColor="rgba(0, 255, 255, 0.3)"
            secureTextEntry
            editable={!bridgeActive}
          />
        </View>

        {bridgeActive && (
          <View style={styles.statsBox}>
            <Text style={styles.statsTitle}>● BRIDGE ACTIVE</Text>
            <View style={styles.statsGrid}>
              <View style={styles.statItem}>
                <Text style={styles.statLabel}>STATUS</Text>
                <Text style={styles.statValue}>RUNNING</Text>
              </View>
              <View style={styles.statItem}>
                <Text style={styles.statLabel}>DEVICES</Text>
                <Text style={styles.statValue}>{connectedDevices}</Text>
              </View>
            </View>
          </View>
        )}

        <Pressable 
          onPress={bridgeActive ? stopBridge : startBridge} 
          style={styles.actionButton}>
          <View style={[styles.actionOuter, bridgeActive && styles.actionActive]}>
            <View style={styles.actionInner}>
              <Text style={[styles.actionText, bridgeActive && styles.actionTextActive]}>
                {bridgeActive ? '■ STOP BRIDGE' : '▶ START BRIDGE'}
              </Text>
            </View>
          </View>
        </Pressable>
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
  infoBox: {borderWidth: 2, borderColor: 'rgba(0, 255, 255, 0.3)', borderRadius: 8, padding: 16, backgroundColor: 'rgba(0, 0, 0, 0.6)', marginBottom: 16},
  infoText: {color: '#00ffff', fontSize: 16, fontWeight: 'bold', marginBottom: 8},
  infoSubtext: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 12, lineHeight: 18},
  content: {flex: 1, paddingHorizontal: 24},
  contentInner: {paddingBottom: 24},
  section: {marginBottom: 24},
  label: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 12, letterSpacing: 1.5, marginBottom: 8},
  input: {borderWidth: 2, borderColor: '#00ffff', backgroundColor: 'rgba(0, 26, 26, 0.3)', borderRadius: 8, padding: 16, color: '#00ffff', fontSize: 16, fontFamily: 'monospace'},
  statsBox: {borderWidth: 2, borderColor: 'rgba(0, 255, 0, 0.4)', borderRadius: 8, padding: 16, backgroundColor: 'rgba(0, 26, 0, 0.2)', marginBottom: 24},
  statsTitle: {color: '#00ff00', fontSize: 16, fontWeight: 'bold', marginBottom: 16, textAlign: 'center'},
  statsGrid: {flexDirection: 'row', justifyContent: 'space-around'},
  statItem: {alignItems: 'center'},
  statLabel: {color: 'rgba(0, 255, 0, 0.6)', fontSize: 10, letterSpacing: 1.5, marginBottom: 4},
  statValue: {color: '#00ff00', fontSize: 18, fontWeight: 'bold'},
  actionButton: {marginTop: 16},
  actionOuter: {borderWidth: 2, borderColor: '#00ffff', borderRadius: 8, padding: 2, backgroundColor: 'rgba(0, 26, 26, 0.3)', shadowColor: '#00ffff', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.8, shadowRadius: 15, elevation: 8},
  actionActive: {borderColor: '#ff0000', backgroundColor: 'rgba(26, 0, 0, 0.3)', shadowColor: '#ff0000'},
  actionInner: {borderWidth: 1, borderColor: '#006666', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 20},
  actionText: {color: '#00ffff', textAlign: 'center', fontSize: 18, fontWeight: 'bold', letterSpacing: 3},
  actionTextActive: {color: '#ff0000'},
});
