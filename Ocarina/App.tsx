import React from 'react';
import {SafeAreaView, StatusBar} from 'react-native';
import {NavigationContainer} from '@react-navigation/native';
import {createNativeStackNavigator} from '@react-navigation/native-stack';
import HomeScreen from './src/screens/HomeScreen';
import WiFiThingiesScreen from './src/screens/WiFiThingiesScreen';
import WiFiScreen from './src/screens/WiFiScreen';
import CreateBridgeScreen from './src/screens/CreateBridgeScreen';
import CreateMITMScreen from './src/screens/CreateMITMScreen';
import BluetoothScreen from './src/screens/BluetoothScreen';

const Stack = createNativeStackNavigator();

export default function App() {
  return (
    <SafeAreaView style={{flex: 1, backgroundColor: '#000'}}>
      <StatusBar barStyle="light-content" backgroundColor="#000" />
      <NavigationContainer>
        <Stack.Navigator screenOptions={{headerShown: false}}>
          <Stack.Screen name="Home" component={HomeScreen} />
          <Stack.Screen name="WiFiThingies" component={WiFiThingiesScreen} />
          <Stack.Screen name="WiFiScanner" component={WiFiScreen} />
          <Stack.Screen name="CreateBridge" component={CreateBridgeScreen} />
          <Stack.Screen name="CreateMITM" component={CreateMITMScreen} />
          <Stack.Screen name="Bluetooth" component={BluetoothScreen} />
        </Stack.Navigator>
      </NavigationContainer>
    </SafeAreaView>
  );
}
