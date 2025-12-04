// OpenPonyLogger Web UI - Application Logic
// Mock data generator for prototyping

class OpenPonyLogger {
    constructor() {
        this.isRecording = false;
        this.currentSession = null;
        this.sessions = this.loadMockSessions();
        this.gauges = {};
        this.animationFrames = {};
        
        this.init();
    }

    init() {
        this.setupTabs();
        this.setupGauges();
        this.setupGForceDisplay();
        this.setupGPSDisplay();
        this.setupSessions();
        this.setupConfig();
        this.startDataSimulation();
        this.updateConnectionStatus();
    }

    // Tab Navigation
    setupTabs() {
        const tabButtons = document.querySelectorAll('.tab-button');
        const tabContents = document.querySelectorAll('.tab-content');

        tabButtons.forEach(button => {
            button.addEventListener('click', () => {
                const tabName = button.dataset.tab;
                
                // Remove active class from all
                tabButtons.forEach(btn => btn.classList.remove('active'));
                tabContents.forEach(content => content.classList.remove('active'));
                
                // Add active class to clicked tab
                button.classList.add('active');
                document.getElementById(tabName).classList.add('active');
                
                // Trigger specific tab initialization
                this.onTabChange(tabName);
            });
        });
    }

    onTabChange(tabName) {
        switch(tabName) {
            case 'gauges':
                this.updateGauges();
                break;
            case 'gforce':
                this.drawGForce();
                break;
            case 'gps':
                this.drawGPS();
                break;
            case 'sessions':
                this.renderSessions();
                break;
        }
    }

    // Canvas-Gauges Setup
    setupGauges() {
        // Speed Gauge
        this.gauges.speed = new RadialGauge({
            renderTo: 'speedGauge',
            width: 250,
            height: 250,
            units: 'MPH',
            minValue: 0,
            maxValue: 180,
            majorTicks: ['0', '20', '40', '60', '80', '100', '120', '140', '160', '180'],
            minorTicks: 2,
            strokeTicks: true,
            highlights: [
                { from: 0, to: 80, color: 'rgba(76, 175, 80, .3)' },
                { from: 80, to: 120, color: 'rgba(255, 193, 7, .3)' },
                { from: 120, to: 180, color: 'rgba(244, 67, 54, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#ff6b35',
            colorNeedleEnd: '#ff6b35',
            needleCircleSize: 10,
            needleCircleOuter: false,
            animationDuration: 500,
            animationRule: 'linear',
            fontNumbersSize: 20,
            borders: false
        }).draw();

        // RPM Gauge
        this.gauges.rpm = new RadialGauge({
            renderTo: 'rpmGauge',
            width: 250,
            height: 250,
            units: 'x1000',
            minValue: 0,
            maxValue: 8,
            majorTicks: ['0', '1', '2', '3', '4', '5', '6', '7', '8'],
            minorTicks: 5,
            strokeTicks: true,
            highlights: [
                { from: 0, to: 5, color: 'rgba(76, 175, 80, .3)' },
                { from: 5, to: 6.5, color: 'rgba(255, 193, 7, .3)' },
                { from: 6.5, to: 8, color: 'rgba(244, 67, 54, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#ff6b35',
            colorNeedleEnd: '#ff6b35',
            needleCircleSize: 10,
            animationDuration: 500,
            animationRule: 'linear',
            fontNumbersSize: 20,
            borders: false
        }).draw();

        // Temperature Gauge
        this.gauges.temp = new RadialGauge({
            renderTo: 'tempGauge',
            width: 250,
            height: 250,
            units: '°F',
            minValue: 100,
            maxValue: 250,
            majorTicks: ['100', '125', '150', '175', '200', '225', '250'],
            minorTicks: 5,
            strokeTicks: true,
            highlights: [
                { from: 100, to: 195, color: 'rgba(76, 175, 80, .3)' },
                { from: 195, to: 220, color: 'rgba(255, 193, 7, .3)' },
                { from: 220, to: 250, color: 'rgba(244, 67, 54, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#2196f3',
            colorNeedleEnd: '#2196f3',
            needleCircleSize: 10,
            animationDuration: 1000,
            fontNumbersSize: 18,
            borders: false
        }).draw();

        // Oil Pressure Gauge
        this.gauges.oilPressure = new RadialGauge({
            renderTo: 'oilPressureGauge',
            width: 250,
            height: 250,
            units: 'PSI',
            minValue: 0,
            maxValue: 100,
            majorTicks: ['0', '20', '40', '60', '80', '100'],
            minorTicks: 4,
            strokeTicks: true,
            highlights: [
                { from: 0, to: 20, color: 'rgba(244, 67, 54, .3)' },
                { from: 20, to: 80, color: 'rgba(76, 175, 80, .3)' },
                { from: 80, to: 100, color: 'rgba(255, 193, 7, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#4caf50',
            colorNeedleEnd: '#4caf50',
            needleCircleSize: 10,
            animationDuration: 1000,
            fontNumbersSize: 20,
            borders: false
        }).draw();

        // Boost Gauge
        this.gauges.boost = new RadialGauge({
            renderTo: 'boostGauge',
            width: 250,
            height: 250,
            units: 'PSI',
            minValue: -5,
            maxValue: 20,
            majorTicks: ['-5', '0', '5', '10', '15', '20'],
            minorTicks: 5,
            strokeTicks: true,
            highlights: [
                { from: -5, to: 0, color: 'rgba(33, 150, 243, .3)' },
                { from: 0, to: 12, color: 'rgba(76, 175, 80, .3)' },
                { from: 12, to: 20, color: 'rgba(255, 193, 7, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#f7931e',
            colorNeedleEnd: '#f7931e',
            needleCircleSize: 10,
            animationDuration: 500,
            fontNumbersSize: 20,
            borders: false
        }).draw();

        // Throttle Position Gauge
        this.gauges.throttle = new RadialGauge({
            renderTo: 'throttleGauge',
            width: 250,
            height: 250,
            units: '%',
            minValue: 0,
            maxValue: 100,
            majorTicks: ['0', '20', '40', '60', '80', '100'],
            minorTicks: 4,
            strokeTicks: true,
            highlights: [
                { from: 0, to: 100, color: 'rgba(76, 175, 80, .3)' }
            ],
            colorPlate: '#1a1a1a',
            colorMajorTicks: '#ffffff',
            colorMinorTicks: '#808080',
            colorNumbers: '#ffffff',
            colorNeedle: '#ffc107',
            colorNeedleEnd: '#ffc107',
            needleCircleSize: 10,
            animationDuration: 300,
            fontNumbersSize: 20,
            borders: false
        }).draw();
    }

    updateGauges() {
        // Simulate realistic driving data
        const speed = 45 + Math.random() * 30;
        const rpm = 2.5 + Math.random() * 2;
        const temp = 185 + Math.random() * 10;
        const oilPressure = 40 + Math.random() * 20;
        const boost = -2 + Math.random() * 8;
        const throttle = Math.random() * 100;

        this.gauges.speed.value = speed;
        this.gauges.rpm.value = rpm;
        this.gauges.temp.value = temp;
        this.gauges.oilPressure.value = oilPressure;
        this.gauges.boost.value = boost;
        this.gauges.throttle.value = throttle;
    }

    // G-Force Display
    setupGForceDisplay() {
        const canvas = document.getElementById('gforceCanvas');
        canvas.width = 600;
        canvas.height = 600;
        this.gforceCtx = canvas.getContext('2d');
        this.gforceData = { lat: 0, long: 0, vert: 1.0 };
        this.gforcePeaks = { maxAccel: 0, maxBrake: 0, maxCorner: 0 };
    }

    drawGForce() {
        const ctx = this.gforceCtx;
        const canvas = ctx.canvas;
        const centerX = canvas.width / 2;
        const centerY = canvas.height / 2;
        const radius = Math.min(centerX, centerY) - 40;

        // Clear canvas
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Draw concentric circles (0.5g increments)
        ctx.strokeStyle = '#404040';
        ctx.lineWidth = 1;
        for (let i = 1; i <= 4; i++) {
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius * i / 4, 0, Math.PI * 2);
            ctx.stroke();
        }

        // Draw crosshairs
        ctx.strokeStyle = '#808080';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(centerX, centerY - radius);
        ctx.lineTo(centerX, centerY + radius);
        ctx.moveTo(centerX - radius, centerY);
        ctx.lineTo(centerX + radius, centerY);
        ctx.stroke();

        // Draw labels
        ctx.fillStyle = '#b0b0b0';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('1.0g', centerX, centerY - radius - 10);
        ctx.fillText('1.0g', centerX, centerY + radius + 25);
        ctx.textAlign = 'right';
        ctx.fillText('1.0g', centerX - radius - 10, centerY + 5);
        ctx.textAlign = 'left';
        ctx.fillText('1.0g', centerX + radius + 10, centerY + 5);

        // Draw current G-force position
        const gX = this.gforceData.lat * radius;
        const gY = -this.gforceData.long * radius; // Negative for forward=up

        // Trail effect
        ctx.strokeStyle = 'rgba(255, 107, 53, 0.3)';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(centerX + gX, centerY + gY);
        ctx.stroke();

        // Current position
        ctx.fillStyle = '#ff6b35';
        ctx.beginPath();
        ctx.arc(centerX + gX, centerY + gY, 12, 0, Math.PI * 2);
        ctx.fill();

        // Outer ring
        ctx.strokeStyle = '#ff6b35';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.arc(centerX + gX, centerY + gY, 18, 0, Math.PI * 2);
        ctx.stroke();

        // Request next frame
        requestAnimationFrame(() => this.drawGForce());
    }

    updateGForceData() {
        // Simulate realistic G-forces
        this.gforceData.lat = (Math.random() - 0.5) * 0.6;
        this.gforceData.long = (Math.random() - 0.3) * 0.8;
        this.gforceData.vert = 0.95 + Math.random() * 0.1;

        // Update display values
        document.getElementById('gforceLat').textContent = this.gforceData.lat.toFixed(2) + 'g';
        document.getElementById('gforceLong').textContent = this.gforceData.long.toFixed(2) + 'g';
        document.getElementById('gforceVert').textContent = this.gforceData.vert.toFixed(2) + 'g';

        // Update peaks
        if (this.gforceData.long > this.gforcePeaks.maxAccel) {
            this.gforcePeaks.maxAccel = this.gforceData.long;
            document.getElementById('maxAccel').textContent = this.gforcePeaks.maxAccel.toFixed(2) + 'g';
        }
        if (this.gforceData.long < -this.gforcePeaks.maxBrake) {
            this.gforcePeaks.maxBrake = -this.gforceData.long;
            document.getElementById('maxBrake').textContent = this.gforcePeaks.maxBrake.toFixed(2) + 'g';
        }
        const cornerG = Math.abs(this.gforceData.lat);
        if (cornerG > this.gforcePeaks.maxCorner) {
            this.gforcePeaks.maxCorner = cornerG;
            document.getElementById('maxCorner').textContent = this.gforcePeaks.maxCorner.toFixed(2) + 'g';
        }
    }

    // GPS Display
    setupGPSDisplay() {
        const canvas = document.getElementById('gpsCanvas');
        canvas.width = 800;
        canvas.height = 600;
        this.gpsCtx = canvas.getContext('2d');
        this.gpsData = {
            lat: 42.2793,
            lon: -71.4162,
            alt: 525,
            heading: 0,
            speed: 0,
            maxSpeed: 0,
            satellites: this.generateMockSatellites()
        };
    }

    drawGPS() {
        const ctx = this.gpsCtx;
        const canvas = ctx.canvas;
        const centerX = canvas.width / 2;
        const centerY = canvas.height / 2;
        const radius = Math.min(centerX, centerY) - 40;

        // Clear canvas
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Draw sky circle
        ctx.fillStyle = '#1a1a1a';
        ctx.beginPath();
        ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
        ctx.fill();

        // Draw compass rings
        ctx.strokeStyle = '#404040';
        ctx.lineWidth = 1;
        for (let i = 1; i <= 3; i++) {
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius * i / 3, 0, Math.PI * 2);
            ctx.stroke();
        }

        // Draw cardinal directions
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 24px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('N', centerX, centerY - radius - 15);
        ctx.fillText('S', centerX, centerY + radius + 30);
        ctx.textAlign = 'right';
        ctx.fillText('W', centerX - radius - 15, centerY + 8);
        ctx.textAlign = 'left';
        ctx.fillText('E', centerX + radius + 15, centerY + 8);

        // Draw satellites
        this.gpsData.satellites.forEach(sat => {
            const angle = (sat.azimuth - 90) * Math.PI / 180;
            const distance = radius * (1 - sat.elevation / 90);
            const x = centerX + distance * Math.cos(angle);
            const y = centerY + distance * Math.sin(angle);

            // Satellite color based on SNR
            let color;
            if (sat.snr > 35) color = '#4caf50';
            else if (sat.snr > 25) color = '#ffc107';
            else color = '#f44336';

            ctx.fillStyle = color;
            ctx.beginPath();
            ctx.arc(x, y, 8, 0, Math.PI * 2);
            ctx.fill();

            // Satellite ID
            ctx.fillStyle = '#ffffff';
            ctx.font = '12px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText(sat.id, x, y - 12);
        });

        // Draw heading indicator
        const headingAngle = (this.gpsData.heading - 90) * Math.PI / 180;
        ctx.strokeStyle = '#ff6b35';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(
            centerX + radius * 0.7 * Math.cos(headingAngle),
            centerY + radius * 0.7 * Math.sin(headingAngle)
        );
        ctx.stroke();

        requestAnimationFrame(() => this.drawGPS());
    }

    generateMockSatellites() {
        const satellites = [];
        for (let i = 1; i <= 12; i++) {
            satellites.push({
                id: i,
                azimuth: Math.random() * 360,
                elevation: 15 + Math.random() * 75,
                snr: 20 + Math.random() * 30
            });
        }
        return satellites;
    }

    updateGPSData() {
        this.gpsData.heading = (this.gpsData.heading + 1) % 360;
        this.gpsData.speed = 45 + Math.random() * 20;
        if (this.gpsData.speed > this.gpsData.maxSpeed) {
            this.gpsData.maxSpeed = this.gpsData.speed;
        }

        // Update display
        document.getElementById('gpsLat').textContent = this.gpsData.lat.toFixed(6) + '°';
        document.getElementById('gpsLon').textContent = this.gpsData.lon.toFixed(6) + '°';
        document.getElementById('gpsAlt').textContent = this.gpsData.alt.toFixed(0) + ' ft';
        document.getElementById('gpsHeading').textContent = this.gpsData.heading.toFixed(0) + '°';
        document.getElementById('gpsSpeed').textContent = this.gpsData.speed.toFixed(1) + ' MPH';
        document.getElementById('gpsMaxSpeed').textContent = this.gpsData.maxSpeed.toFixed(1) + ' MPH';

        // Update satellite list
        const satList = document.getElementById('satList');
        satList.innerHTML = this.gpsData.satellites.slice(0, 8).map(sat => `
            <div class="satellite-item">
                <span class="sat-id">PRN ${sat.id}</span>
                <span class="sat-snr">${sat.snr.toFixed(0)} dB</span>
            </div>
        `).join('');
    }

    // Sessions Management
    loadMockSessions() {
        return [
            {
                id: 1,
                name: 'Morning Commute',
                date: '2024-12-02',
                time: '08:15 AM',
                duration: '25:42',
                distance: '18.3 mi',
                maxSpeed: '68 MPH',
                maxGForce: '0.52g',
                avgSpeed: '42 MPH'
            },
            {
                id: 2,
                name: 'Track Day - Session 1',
                date: '2024-12-01',
                time: '10:30 AM',
                duration: '15:20',
                distance: '12.8 mi',
                maxSpeed: '128 MPH',
                maxGForce: '1.24g',
                avgSpeed: '75 MPH'
            },
            {
                id: 3,
                name: 'Evening Drive',
                date: '2024-11-30',
                time: '06:45 PM',
                duration: '42:15',
                distance: '31.2 mi',
                maxSpeed: '75 MPH',
                maxGForce: '0.68g',
                avgSpeed: '48 MPH'
            }
        ];
    }

    setupSessions() {
        const startButton = document.getElementById('startSession');
        const exportButton = document.getElementById('exportSessions');

        startButton.addEventListener('click', () => {
            this.toggleRecording();
        });

        exportButton.addEventListener('click', () => {
            this.exportSessions();
        });

        this.renderSessions();
    }

    toggleRecording() {
        this.isRecording = !this.isRecording;
        const button = document.getElementById('startSession');
        const buttonText = document.getElementById('sessionButtonText');

        if (this.isRecording) {
            button.classList.add('recording');
            buttonText.textContent = '⏹ Stop Recording';
            this.currentSession = {
                startTime: new Date(),
                name: `Session ${this.sessions.length + 1}`
            };
        } else {
            button.classList.remove('recording');
            buttonText.textContent = 'Start Recording';
            if (this.currentSession) {
                // Save session
                const duration = Math.floor((new Date() - this.currentSession.startTime) / 1000);
                this.sessions.unshift({
                    id: this.sessions.length + 1,
                    name: this.currentSession.name,
                    date: new Date().toISOString().split('T')[0],
                    time: new Date().toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
                    duration: this.formatDuration(duration),
                    distance: (Math.random() * 30 + 5).toFixed(1) + ' mi',
                    maxSpeed: (Math.random() * 50 + 60).toFixed(0) + ' MPH',
                    maxGForce: (Math.random() * 0.8 + 0.3).toFixed(2) + 'g',
                    avgSpeed: (Math.random() * 30 + 35).toFixed(0) + ' MPH'
                });
                this.renderSessions();
            }
        }
    }

    formatDuration(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }

    renderSessions() {
        const sessionsList = document.getElementById('sessionsList');
        
        if (this.sessions.length === 0) {
            sessionsList.innerHTML = '<p style="color: var(--text-secondary); text-align: center; padding: 2rem;">No sessions recorded yet.</p>';
            return;
        }

        sessionsList.innerHTML = this.sessions.map(session => `
            <div class="session-card">
                <div class="session-info">
                    <h3>${session.name}</h3>
                    <div class="session-meta">
                        <span>📅 ${session.date}</span>
                        <span>🕐 ${session.time}</span>
                        <span>⏱️ ${session.duration}</span>
                    </div>
                    <div class="session-stats">
                        <div class="stat">
                            <div class="stat-value">${session.distance}</div>
                            <div class="stat-label">Distance</div>
                        </div>
                        <div class="stat">
                            <div class="stat-value">${session.maxSpeed}</div>
                            <div class="stat-label">Max Speed</div>
                        </div>
                        <div class="stat">
                            <div class="stat-value">${session.maxGForce}</div>
                            <div class="stat-label">Max G-Force</div>
                        </div>
                        <div class="stat">
                            <div class="stat-value">${session.avgSpeed}</div>
                            <div class="stat-label">Avg Speed</div>
                        </div>
                    </div>
                </div>
                <div class="session-actions">
                    <button class="btn btn-secondary btn-sm" onclick="app.viewSession(${session.id})">View</button>
                    <button class="btn btn-secondary btn-sm" onclick="app.exportSession(${session.id})">Export</button>
                    <button class="btn btn-danger btn-sm" onclick="app.deleteSession(${session.id})">Delete</button>
                </div>
            </div>
        `).join('');
    }

    viewSession(id) {
        alert(`Viewing session ${id} (functionality to be implemented)`);
    }

    exportSession(id) {
        alert(`Exporting session ${id} to CSV (functionality to be implemented)`);
    }

    deleteSession(id) {
        if (confirm('Are you sure you want to delete this session?')) {
            this.sessions = this.sessions.filter(s => s.id !== id);
            this.renderSessions();
        }
    }

    exportSessions() {
        alert('Exporting all sessions to CSV (functionality to be implemented)');
    }

    // Configuration
    setupConfig() {
        const saveButton = document.getElementById('saveConfig');
        const resetButton = document.getElementById('resetConfig');
        const factoryResetButton = document.getElementById('factoryReset');
        const brightnessSlider = document.getElementById('brightness');
        const brightnessValue = document.getElementById('brightnessValue');

        brightnessSlider.addEventListener('input', (e) => {
            brightnessValue.textContent = e.target.value + '%';
        });

        saveButton.addEventListener('click', () => {
            this.saveConfiguration();
        });

        resetButton.addEventListener('click', () => {
            if (confirm('Reset all settings to defaults?')) {
                this.resetConfiguration();
            }
        });

        factoryResetButton.addEventListener('click', () => {
            if (confirm('WARNING: This will delete all data and reset to factory settings. Continue?')) {
                this.factoryReset();
            }
        });
    }

    saveConfiguration() {
        // Collect all configuration values
        const config = {
            deviceName: document.getElementById('deviceName').value,
            timezone: document.getElementById('timezone').value,
            units: document.getElementById('units').value,
            sampleRate: document.getElementById('sampleRate').value,
            autoRecord: document.getElementById('autoRecord').checked,
            recordGPS: document.getElementById('recordGPS').checked,
            recordAccel: document.getElementById('recordAccel').checked,
            wifiMode: document.getElementById('wifiMode').value,
            wifiSSID: document.getElementById('wifiSSID').value,
            obdProtocol: document.getElementById('obdProtocol').value,
            obdTimeout: document.getElementById('obdTimeout').value,
            darkMode: document.getElementById('darkMode').checked,
            brightness: document.getElementById('brightness').value
        };

        console.log('Saving configuration:', config);
        alert('Configuration saved successfully!');
    }

    resetConfiguration() {
        document.getElementById('deviceName').value = 'OpenPonyLogger-01';
        document.getElementById('timezone').value = 'America/New_York';
        document.getElementById('units').value = 'imperial';
        document.getElementById('sampleRate').value = '100';
        document.getElementById('autoRecord').checked = true;
        document.getElementById('recordGPS').checked = true;
        document.getElementById('recordAccel').checked = true;
        document.getElementById('brightness').value = '80';
        document.getElementById('brightnessValue').textContent = '80%';
        alert('Configuration reset to defaults');
    }

    factoryReset() {
        this.sessions = [];
        this.resetConfiguration();
        this.renderSessions();
        alert('Factory reset complete');
    }

    // Status Updates
    updateStatus() {
        // System status
        document.getElementById('cpuTemp').textContent = (40 + Math.random() * 15).toFixed(1) + '°C';
        document.getElementById('memUsed').textContent = (50 + Math.random() * 30).toFixed(0) + '%';
        document.getElementById('uptime').textContent = '3h 42m';
        document.getElementById('wifiSignal').textContent = '-' + (40 + Math.random() * 20).toFixed(0) + ' dBm';

        // OBD-II status
        const obdConnected = Math.random() > 0.3;
        const obdStatus = document.getElementById('obdStatus');
        obdStatus.textContent = obdConnected ? 'Connected' : 'Disconnected';
        obdStatus.className = 'status-badge ' + (obdConnected ? 'connected' : 'disconnected');
        
        if (obdConnected) {
            document.getElementById('obdProtocol').textContent = 'CAN (ISO 15765-4)';
            document.getElementById('obdVin').textContent = '1ZVBP8AM5E5******';
            document.getElementById('obdDataRate').textContent = (80 + Math.random() * 20).toFixed(0) + ' Hz';
        }

        // GPS status
        const gpsConnected = Math.random() > 0.2;
        const gpsStatus = document.getElementById('gpsStatus');
        gpsStatus.textContent = gpsConnected ? '3D Fix' : 'No Fix';
        gpsStatus.className = 'status-badge ' + (gpsConnected ? 'connected' : 'disconnected');
        
        if (gpsConnected) {
            document.getElementById('gpsSats').textContent = (8 + Math.floor(Math.random() * 5)).toString();
            document.getElementById('gpsFixQuality').textContent = 'GPS + GLONASS';
            document.getElementById('gpsHdop').textContent = (0.8 + Math.random() * 0.5).toFixed(1);
        }

        // Storage
        const storagePercent = 35 + Math.random() * 5;
        document.getElementById('storageBar').style.width = storagePercent + '%';
    }

    updateConnectionStatus() {
        const statusDot = document.getElementById('connectionStatus');
        const statusText = document.getElementById('connectionText');
        
        // Simulate connection after 2 seconds
        setTimeout(() => {
            statusDot.classList.add('connected');
            statusText.textContent = 'Connected';
        }, 2000);
    }

    // Data Simulation
    startDataSimulation() {
        // Update gauges every 500ms
        setInterval(() => {
            if (document.querySelector('.tab-button.active').dataset.tab === 'gauges') {
                this.updateGauges();
            }
        }, 500);

        // Update G-force data every 100ms
        setInterval(() => {
            this.updateGForceData();
        }, 100);

        // Update GPS data every 1000ms
        setInterval(() => {
            this.updateGPSData();
        }, 1000);

        // Update status every 2000ms
        setInterval(() => {
            if (document.querySelector('.tab-button.active').dataset.tab === 'status') {
                this.updateStatus();
            }
        }, 2000);

        // Initial status update
        this.updateStatus();
    }
}

// Initialize the application
let app;
document.addEventListener('DOMContentLoaded', () => {
    app = new OpenPonyLogger();
});
