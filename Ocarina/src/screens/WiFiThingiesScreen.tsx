import React from 'react';
import {View, Text, Pressable, StyleSheet} from 'react-native';

export default function WiFiThingiesScreen({navigation}: any) {
  const MenuItem = ({title, icon, onPress}: any) => (
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
        <View style={styles.headerRow}>
          <Pressable onPress={() => navigation.goBack()} style={styles.backButton}>
            <Text style={styles.backText}>‹</Text>
          </Pressable>
          <View style={styles.titleContainer}>
            <Text style={styles.title}>WiFi THINGIES</Text>
            <View style={styles.titleUnderline} />
          </View>
        </View>
      </View>

      <View style={styles.menu}>
        <MenuItem 
          title="WiFi Scanner" 
          icon="🔍"
          onPress={() => navigation.navigate('WiFiScanner')} 
        />
        <MenuItem 
          title="Create Bridge" 
          icon="🌉"
          onPress={() => navigation.navigate('CreateBridge')} 
        />
        <MenuItem 
          title="Create MITM Network" 
          icon="🕵️"
          onPress={() => navigation.navigate('CreateMITM')} 
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {flex: 1, backgroundColor: '#000'},
  background: {position: 'absolute', top: 0, bottom: 0, left: 0, right: 0, backgroundColor: '#001a1a', opacity: 0.1},
  header: {marginTop: 48, paddingHorizontal: 24, marginBottom: 32},
  headerRow: {flexDirection: 'row', alignItems: 'center'},
  backButton: {marginRight: 16},
  backText: {fontSize: 28, color: '#00ffff'},
  titleContainer: {flex: 1},
  title: {fontSize: 28, fontWeight: 'bold', letterSpacing: 2, color: '#00ffff', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 10},
  titleUnderline: {height: 2, backgroundColor: '#00ffff', marginTop: 4, width: '66%'},
  menu: {flex: 1, paddingHorizontal: 24},
  menuButton: {marginBottom: 16},
  menuOuter: {backgroundColor: 'rgba(0, 26, 26, 0.3)', borderWidth: 2, borderColor: '#00ffff', borderRadius: 8, padding: 2, shadowColor: '#00ffff', shadowOffset: {width: 0, height: 0}, shadowOpacity: 0.8, shadowRadius: 10, elevation: 8},
  menuInner: {borderWidth: 1, borderColor: '#006666', borderRadius: 6, backgroundColor: 'rgba(0, 0, 0, 0.9)', flexDirection: 'row', alignItems: 'center', padding: 16},
  menuIcon: {fontSize: 28, marginRight: 12},
  menuTextContainer: {flex: 1},
  menuTitle: {color: '#00ffff', fontSize: 18, fontWeight: 'bold', letterSpacing: 2, textTransform: 'uppercase', textShadowColor: '#00ffff', textShadowOffset: {width: 0, height: 0}, textShadowRadius: 8},
  menuUnderline: {height: 2, backgroundColor: '#00ffff', marginTop: 4, width: '50%'},
  menuArrow: {fontSize: 24, color: '#00ffff'},
});
