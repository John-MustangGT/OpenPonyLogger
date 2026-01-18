#pragma once

const char HTML_MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenPonyLogger</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: #e0e0e0; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        h1 { color: #4a9eff; margin-bottom: 20px; }
        
        /* Tab Navigation */
        .tabs { display: flex; gap: 10px; margin-bottom: 20px; border-bottom: 2px solid #333; }
        .tab { padding: 12px 24px; background: #2a2a2a; border: none; color: #aaa; cursor: pointer; 
               border-radius: 5px 5px 0 0; transition: all 0.3s; }
        .tab:hover { background: #333; color: #fff; }
        .tab.active { background: #4a9eff; color: #fff; }
        
        .tab-content { display: none; padding: 20px; background: #2a2a2a; border-radius: 5px; }
        .tab-content.active { display: block; }
        
        /* Dashboard */
        .sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-top: 20px; }
        .sensor-card { background: #333; padding: 15px; border-radius: 8px; border-left: 4px solid #4a9eff; }
        .sensor-label { font-size: 12px; color: #aaa; margin-bottom: 5px; }
        .sensor-value { font-size: 24px; font-weight: bold; color: #4a9eff; }
        .sensor-unit { font-size: 14px; color: #888; margin-left: 5px; }
        
        /* Configuration Form */
        .config-form { max-width: 800px; }
        .form-group { margin-bottom: 20px; }
        .form-group label { display: block; margin-bottom: 8px; color: #aaa; font-size: 14px; }
        .form-group input, .form-group select { width: 100%; padding: 10px; background: #333; border: 1px solid #444; 
                                                 color: #e0e0e0; border-radius: 5px; font-size: 14px; }
        .form-group input:focus, .form-group select:focus { outline: none; border-color: #4a9eff; }
        
        .config-section { margin-bottom: 30px; padding: 20px; background: #222; border-radius: 8px; }
        .config-section h3 { color: #4a9eff; margin-bottom: 15px; font-size: 18px; }
        
        /* PID Table */
        .pid-table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        .pid-table th { background: #333; padding: 10px; text-align: left; color: #aaa; font-size: 12px; border-bottom: 2px solid #444; }
        .pid-table td { padding: 10px; border-bottom: 1px solid #333; }
        .pid-table input[type="checkbox"] { width: auto; margin: 0; }
        .pid-table input[type="number"] { width: 80px; padding: 5px; }
        .pid-name { color: #e0e0e0; font-weight: bold; }
        .pid-id { color: #888; font-family: monospace; font-size: 11px; }
        .pid-category { font-size: 11px; color: #666; font-style: italic; }
        
        button { padding: 12px 24px; background: #4a9eff; color: #fff; border: none; border-radius: 5px; 
                 cursor: pointer; font-size: 14px; transition: background 0.3s; }
        button:hover { background: #3a8eef; }
        button:disabled { background: #555; cursor: not-allowed; }
        
        .status-message { padding: 12px; margin-top: 15px; border-radius: 5px; display: none; }
        .status-message.success { background: #2d5016; border: 1px solid #4a8520; color: #90ee90; }
        .status-message.error { background: #5a1616; border: 1px solid #a52020; color: #ff6b6b; }
        
        /* About Page */
        .info-section { margin-bottom: 30px; }
        .info-section h3 { color: #4a9eff; margin-bottom: 15px; }
        .info-row { display: flex; justify-content: space-between; padding: 10px; background: #333; 
                    margin-bottom: 5px; border-radius: 5px; }
        .info-label { color: #aaa; }
        .info-value { color: #e0e0e0; font-weight: bold; }
        
        .license-box { background: #333; padding: 15px; border-radius: 5px; font-family: monospace; 
                       font-size: 12px; line-height: 1.6; color: #aaa; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏁 OpenPonyLogger</h1>
        
        <div class="tabs">
            <button class="tab active" onclick="showTab('dashboard')">Dashboard</button>
            <button class="tab" onclick="showTab('configuration')">Configuration</button>
            <button class="tab" onclick="showTab('about')">About</button>
        </div>
        
        <!-- Dashboard Tab -->
        <div id="dashboard" class="tab-content active">
            <div class="sensor-grid">
                <div class="sensor-card">
                    <div class="sensor-label">GPS Status</div>
                    <div class="sensor-value" id="gps-status">Waiting...</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Latitude</div>
                    <div class="sensor-value" id="latitude">--</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Longitude</div>
                    <div class="sensor-value" id="longitude">--</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Speed</div>
                    <div class="sensor-value" id="speed">--<span class="sensor-unit">mph</span></div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Acceleration X</div>
                    <div class="sensor-value" id="accel-x">--<span class="sensor-unit">m/s²</span></div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Acceleration Y</div>
                    <div class="sensor-value" id="accel-y">--<span class="sensor-unit">m/s²</span></div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Acceleration Z</div>
                    <div class="sensor-value" id="accel-z">--<span class="sensor-unit">m/s²</span></div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Battery</div>
                    <div class="sensor-value" id="battery">--<span class="sensor-unit">%</span></div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Sample Count</div>
                    <div class="sensor-value" id="sample-count">0</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-label">Uptime</div>
                    <div class="sensor-value" id="uptime">--</div>
                </div>
            </div>
        </div>
        
        <!-- Configuration Tab -->
        <div id="configuration" class="tab-content">
            <form class="config-form" onsubmit="saveConfig(event)">
                <div class="config-section">
                    <h3>System Frequencies</h3>
                    <div class="form-group">
                        <label for="main-loop-hz">Main Loop Frequency (Hz)</label>
                        <select id="main-loop-hz" name="main_loop_hz" required>
                            <option value="5">5 Hz</option>
                            <option value="10" selected>10 Hz</option>
                            <option value="20">20 Hz</option>
                            <option value="50">50 Hz</option>
                            <option value="100">100 Hz</option>
                        </select>
                    </div>
                    
                    <div class="form-group">
                        <label for="gps-hz">GPS Update Frequency (Hz)</label>
                        <input type="number" id="gps-hz" name="gps_hz" min="1" max="100" value="10" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="imu-hz">IMU Update Frequency (Hz)</label>
                        <input type="number" id="imu-hz" name="imu_hz" min="1" max="100" value="10" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="obd-hz">OBD Maximum Frequency (Hz)</label>
                        <input type="number" id="obd-hz" name="obd_hz" min="1" max="100" value="10" required>
                    </div>
                </div>
                
                <div class="config-section">
                    <h3>Network Configuration</h3>
                    <p style="color: #aaa; font-size: 13px; margin-bottom: 15px;">Configure WiFi Access Point settings. Changes require device restart to take effect.</p>
                    
                    <div class="form-group">
                        <label for="net-ssid">WiFi SSID (Network Name)</label>
                        <input type="text" id="net-ssid" name="net_ssid" maxlength="31" placeholder="PonyLogger" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="net-password">WiFi Password (leave empty for open network)</label>
                        <input type="password" id="net-password" name="net_password" maxlength="63" placeholder="Optional - leave blank for open network">
                    </div>
                    
                    <div class="form-group">
                        <label for="net-ip">IP Address</label>
                        <input type="text" id="net-ip" name="net_ip" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="192.168.4.1" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="net-subnet">Subnet Mask</label>
                        <input type="text" id="net-subnet" name="net_subnet" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="255.255.255.0" required>
                    </div>
                </div>
                
                <div class="config-section">\n                    <h3>OBD-II PID Configuration</h3>
                    <p style="color: #aaa; font-size: 13px; margin-bottom: 15px;">Configure individual update rates for each OBD-II Parameter ID. Core PIDs are recommended for track logging.</p>
                    
                    <table class="pid-table">
                        <thead>
                            <tr>
                                <th>Enabled</th>
                                <th>PID</th>
                                <th>Parameter</th>
                                <th>Rate (Hz)</th>
                                <th>Category</th>
                            </tr>
                        </thead>
                        <tbody id="pid-table-body">
                            <!-- Core PIDs -->
                            <tr data-pid="0x0C">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x0C</td>
                                <td class="pid-name">Engine RPM</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="10"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x0D">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x0D</td>
                                <td class="pid-name">Vehicle Speed</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="10"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x11">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x11</td>
                                <td class="pid-name">Throttle Position</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="10"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x10">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x10</td>
                                <td class="pid-name">MAF Air Flow</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="5"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x05">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x05</td>
                                <td class="pid-name">Coolant Temperature</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x0F">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x0F</td>
                                <td class="pid-name">Intake Air Temperature</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x1F">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x1F</td>
                                <td class="pid-name">Run Time Since Start</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x2F">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x2F</td>
                                <td class="pid-name">Fuel Tank Level</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x33">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x33</td>
                                <td class="pid-name">Barometric Pressure</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <tr data-pid="0x21">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x21</td>
                                <td class="pid-name">Distance with MIL On</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Core</td>
                            </tr>
                            <!-- Mandatory PIDs -->
                            <tr data-pid="0x03">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x03</td>
                                <td class="pid-name">Fuel System Status</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="1"></td>
                                <td class="pid-category">Mandatory</td>
                            </tr>
                            <tr data-pid="0x04">
                                <td><input type="checkbox" class="pid-enabled" checked></td>
                                <td class="pid-id">0x04</td>
                                <td class="pid-name">Engine Load</td>
                                <td><input type="number" class="pid-rate" min="1" max="100" value="5"></td>
                                <td class="pid-category">Mandatory</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
                
                <button type="submit" id="save-btn">Save Configuration</button>
                <div id="config-status" class="status-message"></div>
            </form>
        </div>
        
        <!-- About Tab -->
        <div id="about" class="tab-content">
            <div class="info-section">
                <h3>Version Information</h3>
                <div class="info-row">
                    <span class="info-label">Git Commit SHA:</span>
                    <span class="info-value" id="git-sha">Loading...</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Project Version:</span>
                    <span class="info-value" id="project-version">Loading...</span>
                </div>
            </div>
            
            <div class="info-section">
                <h3>Device Status</h3>
                <div class="info-row">
                    <span class="info-label">GPS Module:</span>
                    <span class="info-value" id="device-gps">Checking...</span>
                </div>
                <div class="info-row">
                    <span class="info-label">IMU Module:</span>
                    <span class="info-value" id="device-imu">Checking...</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Battery Monitor:</span>
                    <span class="info-value" id="device-battery">Checking...</span>
                </div>
            </div>
            
            <div class="info-section">
                <h3>License</h3>
                <div class="license-box">
MIT License

Copyright (c) 2026 OpenPonyLogger Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
                </div>
            </div>
        </div>
    </div>
    
    <script>
        let ws;
        
        function showTab(tabName) {
            document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');
            
            if (tabName === 'configuration') {
                loadConfig();
            } else if (tabName === 'about') {
                loadAbout();
            }
        }
        
        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            
            ws.onopen = () => console.log('WebSocket connected');
            ws.onclose = () => setTimeout(connectWebSocket, 3000);
            ws.onerror = (e) => console.error('WebSocket error:', e);
            
            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === 'sensor') {
                        updateDashboard(data);
                    }
                } catch (e) {
                    console.error('Parse error:', e);
                }
            };
        }
        
        function updateDashboard(data) {
            document.getElementById('gps-status').textContent = data.gps_valid ? 'Valid' : 'Invalid';
            document.getElementById('latitude').textContent = data.latitude.toFixed(6);
            document.getElementById('longitude').textContent = data.longitude.toFixed(6);
            document.getElementById('speed').textContent = data.speed.toFixed(1);
            document.getElementById('accel-x').textContent = data.accel_x.toFixed(2);
            document.getElementById('accel-y').textContent = data.accel_y.toFixed(2);
            document.getElementById('accel-z').textContent = data.accel_z.toFixed(2);
            document.getElementById('battery').textContent = data.battery_soc.toFixed(1);
            document.getElementById('sample-count').textContent = data.sample_count;
            
            const uptimeSec = Math.floor(data.uptime_ms / 1000);
            const hours = Math.floor(uptimeSec / 3600);
            const minutes = Math.floor((uptimeSec % 3600) / 60);
            const seconds = uptimeSec % 60;
            document.getElementById('uptime').textContent = 
                `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        }
        
        async function loadConfig() {
            try {
                const response = await fetch('/api/config');
                const config = await response.json();
                document.getElementById('main-loop-hz').value = config.main_loop_hz;
                document.getElementById('gps-hz').value = config.gps_hz;
                document.getElementById('imu-hz').value = config.imu_hz;
                document.getElementById('obd-hz').value = config.obd_hz;
                
                // Load network configuration if present
                if (config.network) {
                    document.getElementById('net-ssid').value = config.network.ssid || 'PonyLogger';
                    document.getElementById('net-password').value = config.network.password || '';
                    document.getElementById('net-ip').value = config.network.ip || '192.168.4.1';
                    document.getElementById('net-subnet').value = config.network.subnet || '255.255.255.0';
                }
                
                // Load PID configurations if present
                if (config.pids) {
                    config.pids.forEach(pid => {
                        const row = document.querySelector(`tr[data-pid="${pid.pid}"]`);
                        if (row) {
                            row.querySelector('.pid-enabled').checked = pid.enabled;
                            row.querySelector('.pid-rate').value = pid.rate_hz;
                        }
                    });
                }
            } catch (e) {
                showStatus('config-status', 'Failed to load configuration', 'error');
            }
        }
        
        async function saveConfig(event) {
            event.preventDefault();
            const btn = document.getElementById('save-btn');
            btn.disabled = true;
            
            // Collect system configuration
            const config = {
                main_loop_hz: parseInt(document.getElementById('main-loop-hz').value),
                gps_hz: parseInt(document.getElementById('gps-hz').value),
                imu_hz: parseInt(document.getElementById('imu-hz').value),
                obd_hz: parseInt(document.getElementById('obd-hz').value),
                network: {
                    ssid: document.getElementById('net-ssid').value,
                    password: document.getElementById('net-password').value,
                    ip: document.getElementById('net-ip').value,
                    subnet: document.getElementById('net-subnet').value
                },
                pids: []
            };
            
            // Collect PID configurations
            document.querySelectorAll('#pid-table-body tr').forEach(row => {
                const pidHex = row.getAttribute('data-pid');
                const pidDec = parseInt(pidHex, 16);
                const enabled = row.querySelector('.pid-enabled').checked;
                const rate = parseInt(row.querySelector('.pid-rate').value);
                const name = row.querySelector('.pid-name').textContent;
                
                config.pids.push({
                    pid: pidHex,
                    pid_dec: pidDec,
                    enabled: enabled,
                    rate_hz: rate,
                    name: name
                });
            });
            
            try {
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(config)
                });
                
                const result = await response.json();
                if (result.success) {
                    showStatus('config-status', 'Configuration saved! Restart device to apply.', 'success');
                } else {
                    showStatus('config-status', result.error || 'Failed to save', 'error');
                }
            } catch (e) {
                showStatus('config-status', 'Network error', 'error');
            } finally {
                btn.disabled = false;
            }
        }
        
        async function loadAbout() {
            try {
                const response = await fetch('/api/about');
                const info = await response.json();
                document.getElementById('git-sha').textContent = info.git_sha;
                document.getElementById('project-version').textContent = info.version;
                document.getElementById('device-gps').textContent = info.devices.gps ? 'Connected' : 'Not Found';
                document.getElementById('device-imu').textContent = info.devices.imu ? 'Connected' : 'Not Found';
                document.getElementById('device-battery').textContent = info.devices.battery ? 'Connected' : 'Not Found';
            } catch (e) {
                console.error('Failed to load about info:', e);
            }
        }
        
        function showStatus(elementId, message, type) {
            const el = document.getElementById(elementId);
            el.textContent = message;
            el.className = `status-message ${type}`;
            el.style.display = 'block';
            setTimeout(() => { el.style.display = 'none'; }, 5000);
        }
        
        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";
