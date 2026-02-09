import React from 'react';
import {NavigationContainer} from '@react-navigation/native';
import {createNativeStackNavigator, NativeStackScreenProps} from '@react-navigation/native-stack';
import {View, Text, Pressable} from 'react-native';

type RootStackParamList = {
  Home: undefined;
  WiFi: undefined;
  Bluetooth: undefined;
};

type HomeScreenProps = NativeStackScreenProps<RootStackParamList, 'Home'>;
const Stack = createNativeStackNavigator<RootStackParamList>();

function HomeScreen({navigation}: HomeScreenProps) {
  return (
    <View className="flex-1 bg-black justify-center items-center">
      <Text className="text-green-400 text-4xl font-bold mb-8">Ocarina</Text>
      <Pressable 
        className="bg-green-500 px-6 py-3 rounded-lg mb-4"
        onPress={() => navigation.navigate('WiFi')}
      >
        <Text className="text-black font-bold">WiFi Scanner</Text>
      </Pressable>
      <Pressable 
        className="bg-blue-500 px-6 py-3 rounded-lg"
        onPress={() => navigation.navigate('Bluetooth')}
      >
        <Text className="text-white font-bold">Bluetooth</Text>
      </Pressable>
    </View>
  );
}

function WiFiScreen() {
  return (
    <View className="flex-1 bg-black justify-center items-center">
      <Text className="text-green-400 text-2xl">WiFi Scanner</Text>
    </View>
  );
}

function BluetoothScreen() {
  return (
    <View className="flex-1 bg-black justify-center items-center">
      <Text className="text-blue-400 text-2xl">Bluetooth</Text>
    </View>
  );
}

export default function App() {
  return (
    <NavigationContainer>
      <Stack.Navigator screenOptions={{headerShown: false}}>
        <Stack.Screen name="Home" component={HomeScreen} />
        <Stack.Screen name="WiFi" component={WiFiScreen} />
        <Stack.Screen name="Bluetooth" component={BluetoothScreen} />
      </Stack.Navigator>
    </NavigationContainer>
  );
}
