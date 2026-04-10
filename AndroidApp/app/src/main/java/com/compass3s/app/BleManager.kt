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
            _connectionState.value = "扫描失败: $errorCode"
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
        _connectionState.value = "已发送: $cmd"
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
                _connectionState.value = "✅ OTA 成功！设备正在重启"
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
        _connectionState.value = "OTA 传输中... (${(otaProgress.value * 100).toInt()}%)"
    }
}
