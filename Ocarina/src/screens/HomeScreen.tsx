import React, {useState, useEffect} from 'react';
import {View, Text, Pressable, Animated, StyleSheet} from 'react-native';

export default function HomeScreen({navigation}: any) {
  const [pulse] = useState(new Animated.Value(1));

  useEffect(() => {
    Animated.loop(
      Animated.sequence([
        Animated.timing(pulse, {toValue: 1.05, duration: 1000, useNativeDriver: true}),
        Animated.timing(pulse, {toValue: 1, duration: 1000, useNativeDriver: true}),
      ])
    ).start();
  }, []);

  const MenuItem = ({title, onPress, icon}) => (
    <Pressable onPress={onPress} style={styles.menuButton}>
      <View style={styles.menuOuter}>
        <View style={styles.menuInner}>
          <Text style={styles.menuIcon}>{icon}</Text>
          <View style={styles.menuTextContainer}>
            <Text style={styles.menuTitle}>{title}</Text>
            <View style={styles.menuUnderline} />
          </View>
          <Text style={styles.menuArrow}>›</Text>
        </View>
      </View>
    </Pressable>
  );

  return (
    <View style={styles.container}>
      <View style={styles.background} />
      <View style={styles.header}>
        <Animated.View style={{transform: [{scale: pulse}]}}>
          <Text style={styles.title}>OCARINA</Text>
        </Animated.View>
        
        <View style={styles.subtitle}>
          <Text style={styles.subtitleText}>✦ NETWORK SWISS ARMY KNIFE ✦</Text>
        </View>
      </View>

      <View style={styles.menu}>
        <MenuItem title="WiFi Thingies" icon="📡" onPress={() => navigation.navigate('WiFiThingies')} />
        <MenuItem title="Bluetooth" icon="🔵" onPress={() => navigation.navigate('Bluetooth')} />
        <MenuItem title="IR Remote" icon="📺" onPress={() => {}} />
        <MenuItem title="Settings" icon="⚙️" onPress={() => {}} />

        <View style={styles.statusBar}>
          <View style={styles.statusItem}>
            <Text style={styles.statusLabel}>STATUS</Text>
            <Text style={styles.statusValue}>● READY</Text>
          </View>
          <View style={styles.divider} />
          <View style={styles.statusItem}>
            <Text style={styles.statusLabel}>VERSION</Text>
            <Text style={styles.statusValue}>v1.0</Text>
          </View>
          <View style={styles.divider} />
          <View style={styles.statusItem}>
            <Text style={styles.statusLabel}>DEVICES</Text>
            <Text style={styles.statusValue}>0</Text>
          </View>
        </View>

        <View style={styles.bottomBar} />
        <View style={styles.dots}>
          <View style={[styles.dot, {opacity: 1}]} />
          <View style={[styles.dot, {opacity: 0.6}]} />
          <View style={[styles.dot, {opacity: 0.3}]} />
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {flex: 1, backgroundColor: '#000'},
  background: {position: 'absolute', top: 0, bottom: 0, left: 0, right: 0, backgroundColor: '#001a1a', opacity: 0.1},
  header: {marginTop: 64, marginBottom: 32, paddingHorizontal: 24},
  title: {textAlign: 'center', fontSize: 48, fontWeight: 'bold', color: '#00ffff', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 20},
  subtitle: {borderTopWidth: 2, borderBottomWidth: 2, borderColor: '#00ffff', paddingVertical: 8, marginTop: 16, backgroundColor: 'rgba(0, 26, 26, 0.2)'},
  subtitleText: {color: '#99ffff', textAlign: 'center', fontSize: 11, letterSpacing: 3},
  menu: {flex: 1, paddingHorizontal: 24},
  menuButton: {marginBottom: 16},
  menuOuter: {backgroundColor: 'rgba(0, 26, 26, 0.3)', borderWidth: 2, borderColor: '#00ffff', borderRadius: 8, padding: 2, shadowColor: '#00ffff', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.8, shadowRadius: 10, elevation: 8},
  menuInner: {borderWidth: 1, borderColor: '#006666', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', flexDirection: 'row', alignItems: 'center', padding: 16},
  menuIcon: {fontSize: 28, marginRight: 12},
  menuTextContainer: {flex: 1},
  menuTitle: {color: '#00ffff', fontSize: 18, fontWeight: 'bold', letterSpacing: 2, textTransform: 'uppercase', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 8},
  menuUnderline: {height: 2, backgroundColor: '#00ffff', marginTop: 4, width: '50%'},
  menuArrow: {fontSize: 24, color: '#00ffff'},
  statusBar: {marginTop: 32, borderWidth: 2, borderColor: 'rgba(0, 255, 255, 0.4)', borderRadius: 8, padding: 16, backgroundColor: 'rgba(0, 0, 0, 0.6)', flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center'},
  statusItem: {alignItems: 'center'},
  statusLabel: {color: 'rgba(0, 255, 255, 0.6)', fontSize: 10, letterSpacing: 1.5},
  statusValue: {color: '#00ffff', fontSize: 13, fontWeight: 'bold', marginTop: 4},
  divider: {width: 1, height: 32, backgroundColor: 'rgba(0, 255, 255, 0.3)'},
  bottomBar: {marginTop: 16, height: 4, backgroundColor: 'rgba(0, 255, 255, 0.3)', borderRadius: 2},
  dots: {marginTop: 4, flexDirection: 'row', justifyContent: 'center', gap: 8},
  dot: {width: 8, height: 8, borderRadius: 4, backgroundColor: '#00ffff'},
});
