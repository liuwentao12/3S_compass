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
            Text("🧭 星环智能导航仪 (3S Compass)", style = MaterialTheme.typography.titleLarge)
            Spacer(modifier = Modifier.height(16.dp))

            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("设备连接", style = MaterialTheme.typography.titleMedium)
                    Text("状态: $connectionState", color = MaterialTheme.colorScheme.primary)
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
                    Button(onClick = { bleManager.sendCommand("DEST:$lat,$lon") }, modifier = Modifier.fillMaxWidth()) {
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
