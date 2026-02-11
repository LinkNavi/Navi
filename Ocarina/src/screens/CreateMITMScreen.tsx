import React, {useState} from 'react';
import {View, Text, Pressable, ScrollView, TextInput, StyleSheet, Alert} from 'react-native';

export default function CreateMITMScreen({navigation}: any) {
  const [networkName, setNetworkName] = useState('Free_WiFi');
  const [mitmActive, setMitmActive] = useState(false);
  const [capturedPackets, setCapturedPackets] = useState(0);
  const [connectedVictims, setConnectedVictims] = useState(0);

  const startMITM = () => {
    if (!networkName.trim()) {
      Alert.alert('Error', 'Please enter a network name');
      return;
    }
    
    Alert.alert(
      '⚠️ Warning',
      'This creates a fake access point for security testing. Only use on networks you own or have permission to test.',
      [
        {text: 'Cancel', style: 'cancel'},
        {text: 'I Understand', onPress: () => {
          setMitmActive(true);
          // TODO: Send BLE command to ESP32
        }}
      ]
    );
  };

  const stopMITM = () => {
    Alert.alert('MITM Stopped', 'Network deactivated');
    setMitmActive(false);
    setCapturedPackets(0);
    setConnectedVictims(0);
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
            <Text style={styles.title}>MITM NETWORK</Text>
            <View style={styles.titleUnderline} />
          </View>
        </View>

        <View style={styles.warningBox}>
          <Text style={styles.warningText}>⚠️ Security Tool</Text>
          <Text style={styles.warningSubtext}>Creates a fake AP for testing. Use only on authorized networks.</Text>
        </View>
      </View>

      <ScrollView style={styles.content} contentContainerStyle={styles.contentInner}>
        <View style={styles.section}>
          <Text style={styles.label}>FAKE NETWORK NAME</Text>
          <TextInput
            style={styles.input}
            value={networkName}
            onChangeText={setNetworkName}
            placeholder="Enter network name"
            placeholderTextColor="rgba(255, 255, 0, 0.3)"
            editable={!mitmActive}
          />
          <Text style={styles.hint}>💡 Tip: Use common names like "Free_WiFi" or "Airport_WiFi"</Text>
        </View>

        {mitmActive && (
          <View style={styles.statsBox}>
            <Text style={styles.statsTitle}>● MITM ACTIVE</Text>
            <View style={styles.statsGrid}>
              <View style={styles.statItem}>
                <Text style={styles.statLabel}>VICTIMS</Text>
                <Text style={styles.statValue}>{connectedVictims}</Text>
              </View>
              <View style={styles.statItem}>
                <Text style={styles.statLabel}>PACKETS</Text>
                <Text style={styles.statValue}>{capturedPackets}</Text>
              </View>
            </View>
            
            <View style={styles.captureLog}>
              <Text style={styles.logTitle}>CAPTURE LOG</Text>
              <Text style={styles.logText}>Waiting for connections...</Text>
            </View>
          </View>
        )}

        <Pressable 
          onPress={mitmActive ? stopMITM : startMITM} 
          style={styles.actionButton}>
          <View style={[styles.actionOuter, mitmActive && styles.actionActive]}>
            <View style={styles.actionInner}>
              <Text style={[styles.actionText, mitmActive && styles.actionTextActive]}>
                {mitmActive ? '■ STOP MITM' : '▶ START MITM'}
              </Text>
            </View>
          </View>
        </Pressable>

        <View style={styles.disclaimer}>
          <Text style={styles.disclaimerText}>
            ⚖️ Legal Notice: Unauthorized network interception is illegal. This tool is for educational and authorized security testing only.
          </Text>
        </View>
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
  warningBox: {borderWidth: 2, borderColor: 'rgba(255, 255, 0, 0.5)', borderRadius: 8, padding: 16, backgroundColor: 'rgba(26, 26, 0, 0.3)', marginBottom: 16},
  warningText: {color: '#ffff00', fontSize: 16, fontWeight: 'bold', marginBottom: 8},
  warningSubtext: {color: 'rgba(255, 255, 0, 0.7)', fontSize: 12, lineHeight: 18},
  content: {flex: 1, paddingHorizontal: 24},
  contentInner: {paddingBottom: 24},
  section: {marginBottom: 24},
  label: {color: 'rgba(255, 255, 0, 0.7)', fontSize: 12, letterSpacing: 1.5, marginBottom: 8},
  input: {borderWidth: 2, borderColor: '#ffff00', backgroundColor: 'rgba(26, 26, 0, 0.3)', borderRadius: 8, padding: 16, color: '#ffff00', fontSize: 16, fontFamily: 'monospace'},
  hint: {color: 'rgba(255, 255, 0, 0.5)', fontSize: 11, marginTop: 8, fontStyle: 'italic'},
  statsBox: {borderWidth: 2, borderColor: 'rgba(255, 0, 0, 0.5)', borderRadius: 8, padding: 16, backgroundColor: 'rgba(26, 0, 0, 0.3)', marginBottom: 24},
  statsTitle: {color: '#ff0000', fontSize: 16, fontWeight: 'bold', marginBottom: 16, textAlign: 'center'},
  statsGrid: {flexDirection: 'row', justifyContent: 'space-around', marginBottom: 16},
  statItem: {alignItems: 'center'},
  statLabel: {color: 'rgba(255, 0, 0, 0.7)', fontSize: 10, letterSpacing: 1.5, marginBottom: 4},
  statValue: {color: '#ff0000', fontSize: 18, fontWeight: 'bold'},
  captureLog: {borderTopWidth: 1, borderTopColor: 'rgba(255, 0, 0, 0.3)', paddingTop: 12},
  logTitle: {color: 'rgba(255, 0, 0, 0.7)', fontSize: 10, letterSpacing: 1.5, marginBottom: 8},
  logText: {color: '#ff0000', fontSize: 11, fontFamily: 'monospace', lineHeight: 16},
  actionButton: {marginTop: 16},
  actionOuter: {borderWidth: 2, borderColor: '#ffff00', borderRadius: 8, padding: 2, backgroundColor: 'rgba(26, 26, 0, 0.3)', shadowColor: '#ffff00', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.8, shadowRadius: 15, elevation: 8},
  actionActive: {borderColor: '#ff0000', backgroundColor: 'rgba(26, 0, 0, 0.3)', shadowColor: '#ff0000'},
  actionInner: {borderWidth: 1, borderColor: '#666600', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', padding: 20},
  actionText: {color: '#ffff00', textAlign: 'center', fontSize: 18, fontWeight: 'bold', letterSpacing: 3},
  actionTextActive: {color: '#ff0000'},
  disclaimer: {borderWidth: 1, borderColor: 'rgba(255, 255, 255, 0.2)', borderRadius: 8, padding: 12, backgroundColor: 'rgba(0, 0, 0, 0.5)', marginTop: 24},
  disclaimerText: {color: 'rgba(255, 255, 255, 0.5)', fontSize: 10, textAlign: 'center', lineHeight: 14},
});
