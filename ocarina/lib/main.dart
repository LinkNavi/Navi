import 'package:flutter/material.dart';

void main() => runApp(OcarinaApp());

class OcarinaApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Ocarina',
      theme: ThemeData.dark().copyWith(
        primaryColor: Colors.cyan,
        scaffoldBackgroundColor: Colors.grey[900],
      ),
      home: HomeScreen(),
    );
  }
}

class HomeScreen extends StatefulWidget {
  @override
  _HomeScreenState createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  bool isConnected = false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Ocarina'),
        actions: [
          IconButton(
            icon: Icon(Icons.settings),
            onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => SettingsScreen())),
          ),
        ],
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
              size: 80,
              color: isConnected ? Colors.cyan : Colors.grey,
            ),
            SizedBox(height: 20),
            Text(
              isConnected ? 'Connected to Navi' : 'Not Connected',
              style: TextStyle(fontSize: 18),
            ),
            SizedBox(height: 40),
            ElevatedButton(
              onPressed: () {
                setState(() => isConnected = !isConnected);
              },
              child: Text(isConnected ? 'Disconnect' : 'Connect to Navi'),
            ),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: isConnected ? () => Navigator.push(context, MaterialPageRoute(builder: (_) => ScanScreen())) : null,
              child: Text('Start WiFi Scan'),
            ),
          ],
        ),
      ),
    );
  }
}

class ScanScreen extends StatefulWidget {
  @override
  _ScanScreenState createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  bool isScanning = false;
  List<WiFiNetwork> networks = [];

  void startScan() async {
    setState(() {
      isScanning = true;
      networks = [];
    });

    await Future.delayed(Duration(seconds: 2));

    setState(() {
      isScanning = false;
      networks = [
        WiFiNetwork('HomeWiFi-5G', -45, true),
        WiFiNetwork('Starbucks Guest', -67, false),
        WiFiNetwork('ATT_2.4GHz', -82, true),
        WiFiNetwork('NETGEAR42', -55, true),
        WiFiNetwork('xfinitywifi', -71, false),
      ];
    });
  }

  @override
  void initState() {
    super.initState();
    startScan();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Scan Results'),
        actions: [
          IconButton(
            icon: Icon(Icons.refresh),
            onPressed: isScanning ? null : startScan,
          ),
        ],
      ),
      body: isScanning
          ? Center(child: CircularProgressIndicator())
          : ListView.builder(
              itemCount: networks.length,
              itemBuilder: (context, i) {
                final net = networks[i];
                return ListTile(
                  leading: Icon(
                    net.isSecured ? Icons.lock : Icons.lock_open,
                    color: net.isSecured ? Colors.red : Colors.green,
                  ),
                  title: Text(net.ssid),
                  subtitle: Text('Signal: ${net.rssi} dBm'),
                  trailing: Icon(Icons.signal_cellular_alt, color: _getSignalColor(net.rssi)),
                  onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => NetworkDetailScreen(net))),
                );
              },
            ),
    );
  }

  Color _getSignalColor(int rssi) {
    if (rssi > -50) return Colors.green;
    if (rssi > -70) return Colors.orange;
    return Colors.red;
  }
}

class NetworkDetailScreen extends StatelessWidget {
  final WiFiNetwork network;
  NetworkDetailScreen(this.network);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Network Details')),
      body: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('SSID: ${network.ssid}', style: TextStyle(fontSize: 18)),
            SizedBox(height: 10),
            Text('RSSI: ${network.rssi} dBm', style: TextStyle(fontSize: 16)),
            SizedBox(height: 10),
            Text('Security: ${network.isSecured ? "Secured" : "Open"}', style: TextStyle(fontSize: 16)),
          ],
        ),
      ),
    );
  }
}

class SettingsScreen extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Settings')),
      body: ListView(
        children: [
          SwitchListTile(
            title: Text('Auto-scan on connect'),
            value: true,
            onChanged: (v) {},
          ),
          ListTile(
            title: Text('Scan interval'),
            subtitle: Text('5 seconds'),
            trailing: Icon(Icons.chevron_right),
          ),
        ],
      ),
    );
  }
}

class WiFiNetwork {
  final String ssid;
  final int rssi;
  final bool isSecured;

  WiFiNetwork(this.ssid, this.rssi, this.isSecured);
}
