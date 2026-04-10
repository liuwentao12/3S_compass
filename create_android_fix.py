import os

project_dir = r"d:\esp32_project\3S_compass\AndroidApp"

def write_file(path, content):
    full_path = os.path.join(project_dir, path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as f:
        f.write(content.strip() + "\n")

write_file("app/src/main/java/com/compass3s/app/BleManager.kt", '''
package com.compass3s.app

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Build
import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.UUID

@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {
    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var bluetoothGatt: BluetoothGatt? = null

    private val _connectionState = MutableStateFlow("未连接")
    val connectionState: StateFlow<String> = _connectionState

    private val _otaProgress = MutableStateFlow(0f)
    val otaProgress: StateFlow<Float> = _otaProgress

    private var cmdCharacteristic: BluetoothGattCharacteristic? = null
    private var otaCharacteristic: BluetoothGattCharacteristic? = null

    private val SERVICE_UUID = UUID.fromString("000000ff-0000-1000-8000-00805f9b34fb")
    private val CMD_CHAR_UUID = UUID.fromString("0000ff01-0000-1000-8000-00805f9b34fb")
    private val OTA_CHAR_UUID = UUID.fromString("0000ff02-0000-1000-8000-00805f9b34fb")

    private var isOtaInProgress = false
    private var otaData: ByteArray? = null
    private var otaIndex = 0
    private val CHUNK_SIZE = 200

    private val scope = CoroutineScope(Dispatchers.IO + Job())

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val deviceName = device.name ?: ""
            if (deviceName.contains("3S_Compass")) {
                _connectionState.value = "找到设备，正在连接..."
                bluetoothAdapter?.bluetoothLeScanner?.stopScan(this)
                connectToDevice(device)
            }
        }
        override fun onScanFailed(errorCode: Int) {
            _connectionState.value = "扫描失败: \"
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                _connectionState.value = "已连接，发现服务中..."
                bluetoothGatt = gatt
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                _connectionState.value = "已断开连接"
                bluetoothGatt?.close()
                bluetoothGatt = null
                cmdCharacteristic = null
                otaCharacteristic = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                if (service != null) {
                    cmdCharacteristic = service.getCharacteristic(CMD_CHAR_UUID)
                    otaCharacteristic = service.getCharacteristic(OTA_CHAR_UUID)
                    _connectionState.value = "设备就绪"
                    
                    gatt.requestMtu(256)
                } else {
                    _connectionState.value = "未找到对应服务"
                }
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (isOtaInProgress && characteristic?.uuid == OTA_CHAR_UUID) {
                    sendNextOtaChunk()
                }
            } else {
                if (isOtaInProgress) {
                    _connectionState.value = "OTA 写入失败"
                    isOtaInProgress = false
                }
            }
        }
    }

    fun startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
            _connectionState.value = "蓝牙未开启"
            return
        }
        _connectionState.value = "扫描中..."
        bluetoothAdapter.bluetoothLeScanner?.startScan(scanCallback)
        
        scope.launch {
            delay(10000)
            if (_connectionState.value == "扫描中...") {
                bluetoothAdapter.bluetoothLeScanner?.stopScan(scanCallback)
                _connectionState.value = "扫描超时"
            }
        }
    }

    fun disconnect() {
        bluetoothGatt?.disconnect()
    }

    private fun connectToDevice(device: BluetoothDevice) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(context, false, gattCallback)
        }
    }

    fun sendCommand(cmd: String) {
        val char = cmdCharacteristic ?: return
        char.value = cmd.toByteArray()
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        bluetoothGatt?.writeCharacteristic(char)
        _connectionState.value = "已发送: \"
    }

    fun startOta(data: ByteArray) {
        if (cmdCharacteristic == null || otaCharacteristic == null) {
            _connectionState.value = "OTA 服务不可用"
            return
        }
        isOtaInProgress = true
        otaData = data
        otaIndex = 0
        _otaProgress.value = 0f
        
        _connectionState.value = "发送 OTA:START..."
        val char = cmdCharacteristic!!
        char.value = "OTA:START".toByteArray()
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        bluetoothGatt?.writeCharacteristic(char)
        
        scope.launch {
            delay(1000)
            sendNextOtaChunk()
        }
    }

    private fun sendNextOtaChunk() {
        val data = otaData ?: return
        if (otaIndex >= data.size) {
            isOtaInProgress = false
            _otaProgress.value = 1f
            _connectionState.value = "传输完成，发送重启指令..."
            val char = cmdCharacteristic!!
            char.value = "OTA:END".toByteArray()
            bluetoothGatt?.writeCharacteristic(char)
            scope.launch {
                delay(1000)
                _connectionState.value = "? OTA 成功！设备正在重启"
            }
            return
        }

        val end = Math.min(otaIndex + CHUNK_SIZE, data.size)
        val chunk = data.copyOfRange(otaIndex, end)
        
        val char = otaCharacteristic!!
        char.value = chunk
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        bluetoothGatt?.writeCharacteristic(char)
        
        otaIndex = end
        _otaProgress.value = otaIndex.toFloat() / data.size.toFloat()
        _connectionState.value = "OTA 传输中... (\%)"
    }
}
''')

write_file("app/src/main/java/com/compass3s/app/MainActivity.kt", '''
package com.compass3s.app

import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.compass3s.app.ui.theme.Compass3STheme

class MainActivity : ComponentActivity() {

    private lateinit var bleManager: BleManager

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val granted = permissions.entries.all { it.value }
        if (granted) {
            bleManager.startScan()
        } else {
            Toast.makeText(this, "需要蓝牙和定位权限才能连接设备", Toast.LENGTH_LONG).show()
        }
    }

    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            val inputStream = contentResolver.openInputStream(it)
            val bytes = inputStream?.readBytes()
            inputStream?.close()
            if (bytes != null) {
                bleManager.startOta(bytes)
            } else {
                Toast.makeText(this, "无法读取文件", Toast.LENGTH_SHORT).show()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        bleManager = BleManager(this)

        setContent {
            Compass3STheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    AppContent()
                }
            }
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    fun AppContent() {
        val connectionState by bleManager.connectionState.collectAsState()
        val otaProgress by bleManager.otaProgress.collectAsState()

        var lat by remember { mutableStateOf("39.916345") }
        var lon by remember { mutableStateOf("116.397155") }
        
        val scrollState = rememberScrollState()

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
                .verticalScroll(scrollState),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("?? 星环智能导航仪 (3S Compass)", style = MaterialTheme.typography.titleLarge)
            Spacer(modifier = Modifier.height(16.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("设备连接", style = MaterialTheme.typography.titleMedium)
                    Text("状态: \", color = MaterialTheme.colorScheme.primary)
                    Spacer(modifier = Modifier.height(8.dp))
                    Row {
                        Button(onClick = { checkPermissionsAndScan() }, modifier = Modifier.weight(1f)) {
                            Text("搜索并连接")
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(onClick = { bleManager.disconnect() }, modifier = Modifier.weight(1f)) {
                            Text("断开")
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("目标导航", style = MaterialTheme.typography.titleMedium)
                    OutlinedTextField(
                        value = lat,
                        onValueChange = { lat = it },
                        label = { Text("纬度 (Lat)") },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = lon,
                        onValueChange = { lon = it },
                        label = { Text("经度 (Lon)") },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(onClick = { bleManager.sendCommand("DEST:\,\") }, modifier = Modifier.fillMaxWidth()) {
                        Text("发送目标并导航")
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("控制命令", style = MaterialTheme.typography.titleMedium)
                    Row(modifier = Modifier.fillMaxWidth()) {
                        Button(onClick = { bleManager.sendCommand("MODE:COMPASS") }, modifier = Modifier.weight(1f)) {
                            Text("指南针模式")
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(onClick = { bleManager.sendCommand("CALI:START") }, modifier = Modifier.weight(1f)) {
                            Text("罗盘校准")
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("OTA 无感升级", style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(onClick = { filePickerLauncher.launch("application/octet-stream") }, modifier = Modifier.fillMaxWidth()) {
                        Text("选择固件 (.bin) 并开始 OTA")
                    }
                    if (otaProgress > 0f) {
                        Spacer(modifier = Modifier.height(8.dp))
                        LinearProgressIndicator(progress = otaProgress, modifier = Modifier.fillMaxWidth())
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(32.dp))
        }
    }

    private fun checkPermissionsAndScan() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        }

        val allGranted = permissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

        if (allGranted) {
            bleManager.startScan()
        } else {
            requestPermissionLauncher.launch(permissions)
        }
    }
}
''')

print("Fix applied successfully!")
