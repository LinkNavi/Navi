import React, {useState} from 'react';
import {View, Text, StyleSheet, TouchableOpacity} from 'react-native';

export default function HomeScreen({navigation}) {
  const [bleConnected, setBleConnected] = useState(false);

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>OCARINA</Text>
        <View style={[styles.bleStatus, bleConnected ? styles.connected : styles.disconnected]}>
          <Text style={styles.bleText}>
            {bleConnected ? '● Connected' : '○ Disconnected'}
          </Text>
        </View>
      </View>

      <View style={styles.menu}>
        <TouchableOpacity 
          style={styles.menuItem}
          onPress={() => navigation.navigate('WiFi')}>
          <Text style={styles.menuText}>WiFi Scanner</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.menuItem}>
          <Text style={styles.menuText}>Bluetooth</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.menuItem}>
          <Text style={styles.menuText}>IR Remote</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.menuItem}>
          <Text style={styles.menuText}>Settings</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000',
    padding: 20,
  },
  header: {
    marginTop: 40,
    marginBottom: 40,
  },
  title: {
    fontSize: 36,
    color: '#0f0',
    fontWeight: 'bold',
    textAlign: 'center',
    marginBottom: 20,
  },
  bleStatus: {
    padding: 10,
    borderRadius: 5,
    alignItems: 'center',
  },
  connected: {
    backgroundColor: '#0a3d0a',
  },
  disconnected: {
    backgroundColor: '#3d0a0a',
  },
  bleText: {
    color: '#0f0',
    fontSize: 14,
  },
  menu: {
    flex: 1,
  },
  menuItem: {
    backgroundColor: '#1a1a1a',
    padding: 20,
    marginBottom: 15,
    borderRadius: 5,
    borderLeftWidth: 3,
    borderLeftColor: '#0f0',
  },
  menuText: {
    color: '#0f0',
    fontSize: 18,
  },
});
