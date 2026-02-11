package com.navi.ocarina;

import android.Manifest;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.*;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import java.util.*;

public class MainActivity extends AppCompatActivity {
    private static final String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    private static final String CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    private static final int PERMISSION_REQUEST = 1;
    
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic characteristic;
    private ScanCallback scanCallback;  // Keep reference to prevent GC
    
    // UI Elements
    private LinearLayout mainMenu, connectMenu, bluetoothMenu;
    private ScrollView wifiMenu;
    private TextView statusText, menuTitle;
    private Button backButton;
    private ListView deviceList;
    private ArrayAdapter<String> deviceAdapter;
    private Map<String, BluetoothDevice> devices = new HashMap<>();
    private String selectedDevice = null;
    private boolean isConnected = false;
    private boolean isScanning = false;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // Initialize UI
        mainMenu = findViewById(R.id.mainMenu);
        connectMenu = findViewById(R.id.connectMenu);
        wifiMenu = findViewById(R.id.wifiMenu);
        bluetoothMenu = findViewById(R.id.bluetoothMenu);
        statusText = findViewById(R.id.statusText);
        menuTitle = findViewById(R.id.menuTitle);
        backButton = findViewById(R.id.backButton);
        deviceList = findViewById(R.id.deviceList);
        
deviceAdapter = new ArrayAdapter<>(this, R.layout.list_item, new ArrayList<>());
        deviceList.setAdapter(deviceAdapter);
        
        // Setup Bluetooth
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        bluetoothAdapter = manager.getAdapter();
        scanner = bluetoothAdapter.getBluetoothLeScanner();
        
        // Request permissions
        requestPermissions();
        
        // Main menu buttons
        findViewById(R.id.btnConnect).setOnClickListener(v -> showConnectMenu());
        findViewById(R.id.btnWifiThingies).setOnClickListener(v -> {
            if (isConnected) showWifiMenu();
            else Toast.makeText(this, "Connect to device first", Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.btnBluetoothThingies).setOnClickListener(v -> {
            if (isConnected) showBluetoothMenu();
            else Toast.makeText(this, "Connect to device first", Toast.LENGTH_SHORT).show();
        });
        
        // Connect menu buttons
        findViewById(R.id.btnScan).setOnClickListener(v -> startScan());
        findViewById(R.id.btnDisconnect).setOnClickListener(v -> disconnect());
        
        deviceList.setOnItemClickListener((parent, view, position, id) -> {
            selectedDevice = deviceAdapter.getItem(position);
            connectToDevice();
        });
        
        // WiFi menu buttons
        findViewById(R.id.btnWifiScan).setOnClickListener(v -> sendCommand("WIFI_SCAN"));
        findViewById(R.id.btnWifiConnect).setOnClickListener(v -> showWifiConnectDialog());
        findViewById(R.id.btnWifiDisconnect).setOnClickListener(v -> sendCommand("WIFI_DISCONNECT"));
        findViewById(R.id.btnWifiStatus).setOnClickListener(v -> sendCommand("WIFI_STATUS"));
        findViewById(R.id.btnBridgeStart).setOnClickListener(v -> sendCommand("BRIDGE_START"));
        findViewById(R.id.btnBridgeStop).setOnClickListener(v -> sendCommand("BRIDGE_STOP"));
        findViewById(R.id.btnBridgeStatus).setOnClickListener(v -> sendCommand("BRIDGE_STATUS"));
        
        // Bluetooth menu buttons
        findViewById(R.id.btnBleStatus).setOnClickListener(v -> sendCommand("BLE_STATUS"));
        findViewById(R.id.btnBleSend).setOnClickListener(v -> sendCommand("PING"));
        
        // Back button
        backButton.setOnClickListener(v -> showMainMenu());
        
        // Start at main menu
        showMainMenu();
        updateStatus();
    }
    
    private void showMainMenu() {
        mainMenu.setVisibility(View.VISIBLE);
        connectMenu.setVisibility(View.GONE);
        wifiMenu.setVisibility(View.GONE);
        bluetoothMenu.setVisibility(View.GONE);
        backButton.setVisibility(View.GONE);
        menuTitle.setText("Navi Ocarina");
    }
    
    private void showConnectMenu() {
        mainMenu.setVisibility(View.GONE);
        connectMenu.setVisibility(View.VISIBLE);
        wifiMenu.setVisibility(View.GONE);
        bluetoothMenu.setVisibility(View.GONE);
        backButton.setVisibility(View.VISIBLE);
        menuTitle.setText("Connect");
    }
    
    private void showWifiMenu() {
        mainMenu.setVisibility(View.GONE);
        connectMenu.setVisibility(View.GONE);
        wifiMenu.setVisibility(View.VISIBLE);
        bluetoothMenu.setVisibility(View.GONE);
        backButton.setVisibility(View.VISIBLE);
        menuTitle.setText("WiFi Thingies");
    }
    
    private void showBluetoothMenu() {
        mainMenu.setVisibility(View.GONE);
        connectMenu.setVisibility(View.GONE);
        wifiMenu.setVisibility(View.GONE);
        bluetoothMenu.setVisibility(View.VISIBLE);
        backButton.setVisibility(View.VISIBLE);
        menuTitle.setText("Bluetooth Thingies");
    }
    
    private void updateStatus() {
        runOnUiThread(() -> {
            if (isConnected && selectedDevice != null) {
                statusText.setText("● Connected to " + selectedDevice);
                statusText.setTextColor(0xFF50E3C2); // Cyan
            } else {
                statusText.setText("○ Not Connected");
                statusText.setTextColor(0xFFAAAAAA); // Gray
            }
        });
    }
    
  private void requestPermissions() {
    String[] permissions = {
        Manifest.permission.BLUETOOTH_SCAN,
        Manifest.permission.BLUETOOTH_CONNECT,
        Manifest.permission.ACCESS_FINE_LOCATION
    };
    
    // Check which permissions are missing
    boolean needsRequest = false;
    for (String permission : permissions) {
        if (ActivityCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
            needsRequest = true;
            break;
        }
    }
    
    if (needsRequest) {
        ActivityCompat.requestPermissions(this, permissions, PERMISSION_REQUEST);
        Toast.makeText(this, "Please grant all permissions", Toast.LENGTH_LONG).show();
    }
}

@Override
public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (requestCode == PERMISSION_REQUEST) {
        boolean allGranted = true;
        for (int result : grantResults) {
            if (result != PackageManager.PERMISSION_GRANTED) {
                allGranted = false;
                break;
            }
        }
        if (allGranted) {
            Toast.makeText(this, "Permissions granted!", Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(this, "Permissions denied - app may not work properly", Toast.LENGTH_LONG).show();
        }
    }
}
    
    private void startScan() {
 if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
        Toast.makeText(this, "Need Bluetooth permissions - check settings", Toast.LENGTH_LONG).show();
        requestPermissions();
        return;
    }
        if (isScanning) {
            Toast.makeText(this, "Already scanning", Toast.LENGTH_SHORT).show();
            return;
        }
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions();
            return;
        }
        
        devices.clear();
        deviceAdapter.clear();
        deviceAdapter.notifyDataSetChanged();
        isScanning = true;
        Toast.makeText(this, "Scanning for 5 seconds...", Toast.LENGTH_SHORT).show();
        
      scanCallback = new ScanCallback() {
    @Override
    public void onScanResult(int callbackType, ScanResult result) {
        BluetoothDevice device = result.getDevice();
        String address = device.getAddress();
        
        // Show ALL devices by MAC address
        if (!devices.containsKey(address)) {
            devices.put(address, device);
            runOnUiThread(() -> {
                deviceAdapter.add(address);
                deviceAdapter.notifyDataSetChanged();
            });
        }
    }
};
        
        scanner.startScan(scanCallback);
        
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            if (isScanning) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                    scanner.stopScan(scanCallback);
                }
                isScanning = false;
                Toast.makeText(this, "Found " + devices.size() + " device(s)", Toast.LENGTH_SHORT).show();
            }
        }, 5000);
    }
    
    private void connectToDevice() {
        if (selectedDevice == null) return;
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions();
            return;
        }
        
        BluetoothDevice device = devices.get(selectedDevice);
        Toast.makeText(this, "Connecting to " + selectedDevice + "...", Toast.LENGTH_SHORT).show();
        
        BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
            @Override
            public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Connected! Discovering services...", Toast.LENGTH_SHORT).show());
                    if (ActivityCompat.checkSelfPermission(MainActivity.this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                        gatt.discoverServices();
                    }
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread(() -> {
                        isConnected = false;
                        updateStatus();
                        Toast.makeText(MainActivity.this, "Disconnected", Toast.LENGTH_SHORT).show();
                    });
                }
            }
            
            @Override
            public void onServicesDiscovered(BluetoothGatt gatt, int status) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    BluetoothGattService service = gatt.getService(UUID.fromString(SERVICE_UUID));
                    if (service != null) {
                        characteristic = service.getCharacteristic(UUID.fromString(CHAR_UUID));
                        if (characteristic != null) {
                            // Enable notifications
                            if (ActivityCompat.checkSelfPermission(MainActivity.this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                                gatt.setCharacteristicNotification(characteristic, true);
                            }
                            
                            runOnUiThread(() -> {
                                isConnected = true;
                                updateStatus();
                                Toast.makeText(MainActivity.this, "Ready!", Toast.LENGTH_SHORT).show();
                                showMainMenu();
                            });
                        } else {
                            runOnUiThread(() -> Toast.makeText(MainActivity.this, "Characteristic not found", Toast.LENGTH_SHORT).show());
                        }
                    } else {
                        runOnUiThread(() -> Toast.makeText(MainActivity.this, "Service not found", Toast.LENGTH_SHORT).show());
                    }
                }
            }
            
            @Override
            public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                byte[] data = characteristic.getValue();
                String message = new String(data);
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "RX: " + message, Toast.LENGTH_LONG).show());
            }
        };
        
        gatt = device.connectGatt(this, false, gattCallback);
    }
    
    private void disconnect() {
        if (gatt != null) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                gatt.disconnect();
                gatt.close();
            }
            gatt = null;
            characteristic = null;
        }
        isConnected = false;
        updateStatus();
        showMainMenu();
    }
    
    private void sendCommand(String command) {
        if (characteristic == null || !isConnected) {
            Toast.makeText(this, "Not connected", Toast.LENGTH_SHORT).show();
            return;
        }
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        
        characteristic.setValue(command.getBytes());
        boolean success = gatt.writeCharacteristic(characteristic);
        Toast.makeText(this, success ? "Sent: " + command : "Send failed", Toast.LENGTH_SHORT).show();
    }
    
    private void showWifiConnectDialog() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 40);
        
        EditText ssidInput = new EditText(this);
        ssidInput.setHint("WiFi SSID");
        layout.addView(ssidInput);
        
        EditText passwordInput = new EditText(this);
        passwordInput.setHint("Password");
        passwordInput.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);
        layout.addView(passwordInput);
        
        new androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("Connect to WiFi")
            .setView(layout)
            .setPositiveButton("Connect", (dialog, which) -> {
                String ssid = ssidInput.getText().toString();
                String password = passwordInput.getText().toString();
                sendCommand("WIFI_CONNECT:" + ssid + ":" + password);
            })
            .setNegativeButton("Cancel", null)
            .show();
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isScanning && ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
            scanner.stopScan(scanCallback);
        }
        disconnect();
    }
}
