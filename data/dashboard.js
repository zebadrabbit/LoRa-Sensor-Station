let charts = {};
let currentTimeWindow = 'all';  // Match the default active button
let ws;

async function loadData() {
    try {
        // Load sensor data
        const sensorsResponse = await fetch('/api/sensors');
        const sensors = await sensorsResponse.json();
        console.log('Loaded sensors:', sensors);
        
        // Load stats data
        const statsResponse = await fetch('/api/stats');
        const stats = await statsResponse.json();
        console.log('Loaded stats:', stats);
        
        updateSensorList(sensors);
        updateSensorDropdown(sensors);
        updateStats(stats);
    } catch (error) {
        console.error('Error loading data:', error);
    }
}

function updateSensorList(sensors) {
    const sensorList = document.getElementById('sensorList');
    if (!sensors || sensors.length === 0) {
        sensorList.innerHTML = '<div class="empty-state"><p>No clients detected yet</p></div>';
        return;
    }
    
    // Update zone filter dropdown
    const zoneFilter = document.getElementById('zoneFilter');
    const currentZone = zoneFilter.value;
    const zones = new Set();
    sensors.forEach(s => { if (s.zone) zones.add(s.zone); });
    
    zoneFilter.innerHTML = '<option value="">All Zones</option>';
    Array.from(zones).sort().forEach(zone => {
        const opt = document.createElement('option');
        opt.value = zone;
        opt.textContent = zone;
        zoneFilter.appendChild(opt);
    });
    if (currentZone) zoneFilter.value = currentZone;
    
    // Filter sensors by zone
    const filteredSensors = currentZone ? 
        sensors.filter(s => s.zone === currentZone) : sensors;
    
    let html = '';
    filteredSensors.forEach(sensor => {
        const location = sensor.location || `Client ${sensor.id}`;
        const isOnline = sensor.ageSeconds < 300; // Online if seen in last 5 minutes
        const statusClass = isOnline ? 'status-online' : 'status-offline';
        
        // Format battery with power state and charging status
        const powerSource = sensor.battery === 0 ? 'On AC' : 'On Battery';
        const chargingState = sensor.battery === 0 ? 'N/A' : (sensor.charging ? 'Charging' : 'Discharging');
        const batteryDisplay = `${powerSource}/${sensor.battery}%/${chargingState}`;
        
        // Health score color coding
        const health = sensor.health ? sensor.health.overall : 0;
        let healthColor = '#e74c3c'; // Red
        let healthLabel = 'Poor';
        if (health >= 0.8) { healthColor = '#27ae60'; healthLabel = 'Good'; }
        else if (health >= 0.5) { healthColor = '#f39c12'; healthLabel = 'Fair'; }
        
        // Priority badge color
        let priorityColor = '#3498db'; // Medium = blue
        if (sensor.priorityLevel === 2) priorityColor = '#e74c3c'; // High = red
        else if (sensor.priorityLevel === 0) priorityColor = '#95a5a6'; // Low = gray
        
        html += `
            <div class="sensor-item sensor-item-tab" style="background: white; border: 1px solid #e0e6ed; border-left: 5px solid #667eea; border-radius: 8px; padding: 12px; padding-left: 16px; margin-bottom: 10px; box-shadow: 0 1px 3px rgba(0,0,0,0.1);">
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 8px;">
                    <span class="status-indicator ${statusClass}" style="width: 10px; height: 10px; border-radius: 50%; ${isOnline ? 'background: #27ae60;' : 'background: #e74c3c;'}"></span>
                    <strong style="font-size: 16px;">${location}</strong>
                    <span style="background: #ecf0f1; padding: 2px 8px; border-radius: 12px; font-size: 11px; color: #7f8c8d;">ID: ${sensor.id}</span>
                    ${sensor.zone ? `<span style="background: #ecf0f1; padding: 2px 8px; border-radius: 12px; font-size: 11px; color: #7f8c8d;">📍 ${sensor.zone}</span>` : ''}
                    <span style="background: ${priorityColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;">${sensor.priority || 'Medium'}</span>
                    <span style="background: ${healthColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;" title="Communication: ${(health * 100).toFixed(0)}%">❤️ ${healthLabel}</span>
                </div>
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 8px; font-size: 14px; color: #666;">
                    <div>🔋 <strong>Battery:</strong> ${batteryDisplay}</div>
                    <div>📶 <strong>RSSI:</strong> ${sensor.rssi} dBm</div>
                    <div>🕐 <strong>Last seen:</strong> ${sensor.age}</div>
                </div>
            </div>
        `;
    });
    sensorList.innerHTML = html;
    console.log('Updated sensor list with', filteredSensors.length, 'sensors (filtered from', sensors.length, ')');
}

function updateSensorDropdown(sensors) {
    const select = document.getElementById('sensorSelect');
    const currentValue = select.value;
    
    // Clear existing options except the first one
    select.innerHTML = '<option value="">-- Select --</option>';
    
    if (!sensors || sensors.length === 0) return;
    
    sensors.forEach(sensor => {
        const location = sensor.location || `Sensor ${sensor.id}`;
        const option = document.createElement('option');
        option.value = sensor.id;
        option.textContent = `${location} (ID: ${sensor.id})`;
        select.appendChild(option);
    });
    
    // Auto-select first sensor if nothing selected
    if (!currentValue && sensors.length > 0) {
        select.value = sensors[0].id;
        loadHistoricalData(); // Load data for auto-selected sensor
    } else if (currentValue) {
        select.value = currentValue;
    }
}

function updateStats(stats) {
    console.log('Updating stats:', stats);
    if (stats.activeSensors !== undefined) {
        document.getElementById('activeSensorCount').textContent = stats.activeSensors;
    }
    if (stats.totalRx !== undefined) {
        document.getElementById('totalPackets').textContent = stats.totalRx;
    }
    if (stats.successRate !== undefined) {
        document.getElementById('successRate').textContent = stats.successRate + '%';
    }
}

function initChart(canvasId, title, unit) {
    const ctx = document.getElementById(canvasId).getContext('2d');
    charts[canvasId] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: title,
                data: [],
                borderColor: '#667eea',
                backgroundColor: 'rgba(102, 126, 234, 0.1)',
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false }
            },
            scales: {
                y: {
                    beginAtZero: false,
                    title: { display: true, text: unit }
                }
            }
        }
    });
}

function updateCharts(data) {
    // Charts will be updated when historical data is loaded via sensor selection
}

function setTimeWindow(hours) {
    currentTimeWindow = hours;
    document.querySelectorAll('.time-btn').forEach(btn => btn.classList.remove('active'));
    event.target.classList.add('active');
    loadHistoricalData();
}

async function loadHistoricalData() {
    const sensorId = document.getElementById('sensorSelect').value;
    if (!sensorId) return;
    
    try {
        const response = await fetch(`/api/history?sensorId=${sensorId}&range=${currentTimeWindow}`);
        const result = await response.json();
        console.log('Historical data response:', result);
        
        // Check if we got an error or actual data
        if (result.error) {
            console.warn('No historical data:', result.error);
            clearCharts('No historical data available');
            return;
        }
        
        // The API returns {sensorId: X, data: [...]}
        const data = result.data || [];
        
        if (!Array.isArray(data) || data.length === 0) {
            console.warn('Historical data is empty or not an array:', data);
            clearCharts('No readings recorded yet. Historical data will appear once the sensor starts sending valid readings.');
            return;
        }
        
        // Update each chart with its specific metric
        const labels = data.map(d => new Date(d.t * 1000).toLocaleTimeString());
        
        // Check which data fields exist in the response
        // API only returns: t, batt, rssi, charging
        const hasBatt = data.some(d => d.batt !== undefined && d.batt !== null);
        const hasRssi = data.some(d => d.rssi !== undefined && d.rssi !== null);
        
        // Note: Historical data API currently only tracks battery and RSSI
        // Other metrics (temp, humidity, etc.) are not stored in history
        
        // Hide all charts that don't have data
        const chartFields = [
            { id: 'tempChart', field: 'temp', hasData: false },
            { id: 'humidityChart', field: 'humidity', hasData: false },
            { id: 'pressureChart', field: 'pressure', hasData: false },
            { id: 'gasChart', field: 'gas', hasData: false },
            { id: 'lightChart', field: 'light', hasData: false },
            { id: 'voltageChart', field: 'voltage', hasData: false },
            { id: 'currentChart', field: 'current', hasData: false },
            { id: 'powerChart', field: 'power', hasData: false },
            { id: 'battChart', field: 'batt', hasData: hasBatt },
            { id: 'rssiChart', field: 'rssi', hasData: hasRssi }
        ];
        
        chartFields.forEach(chart => {
            const canvas = document.getElementById(chart.id);
            const article = canvas?.closest('article');
            if (article) {
                if (chart.hasData) {
                    article.style.display = 'block';
                    if (charts[chart.id]) {
                        charts[chart.id].data.labels = labels;
                        charts[chart.id].data.datasets[0].data = data.map(d => d[chart.field]);
                        charts[chart.id].update();
                        console.log(`Updated ${chart.id} with ${data.length} points`);
                    }
                } else {
                    article.style.display = 'none';
                    console.log(`Hiding ${chart.id} - no data`);
                }
            }
        });
        
        console.log('Updated charts with', data.length, 'data points');
    } catch (error) {
        console.error('Error loading historical data:', error);
    }
}

function clearCharts(message) {
    for (let chartId in charts) {
        const chart = charts[chartId];
        chart.data.labels = [message];
        chart.data.datasets[0].data = [0];
        chart.update();
    }
}

async function exportData(format) {
    const response = await fetch(`/api/export?format=${format}&window=${currentTimeWindow}`);
    const blob = await response.blob();
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `sensor-data.${format}`;
    a.click();
}

function changeSensor(selectId, targetId) {
    const select = document.getElementById(selectId);
    const chartContainer = document.getElementById(targetId);
    // Update chart based on sensor selection
    loadData();
}

function connectWebSocket() {
    const wsUrl = `ws://${window.location.hostname}/ws`;
    console.log('Connecting to WebSocket:', wsUrl);
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log('✅ WebSocket connected');
        document.getElementById('wsStatus').textContent = '🟢 Connected';
    };
    
    ws.onmessage = (event) => {
        console.log('📨 WebSocket message received:', event.data);
        try {
            const data = JSON.parse(event.data);
            console.log('📊 Parsed data:', data);
            updateSensorList(data);
            updateSensorDropdown(data);            
            // Reload historical data for currently selected sensor to update charts
            const selectedSensorId = document.getElementById('sensorSelect').value;
            if (selectedSensorId) {
                loadHistoricalData();
            }        } catch (error) {
            console.error('❌ WebSocket message parse error:', error);
        }
    };
    
    ws.onerror = (error) => {
        console.error('❌ WebSocket error:', error);
        document.getElementById('wsStatus').textContent = '🔴 Error';
    };
    
    ws.onclose = () => {
        console.log('⚠️ WebSocket disconnected, will reconnect in 5s...');
        document.getElementById('wsStatus').textContent = '🟡 Reconnecting...';
        // Reconnect after 5 seconds
        setTimeout(connectWebSocket, 5000);
    };
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    initChart('tempChart', 'Temperature', '°C');
    initChart('humidityChart', 'Humidity', '%');
    initChart('pressureChart', 'Pressure', 'hPa');
    initChart('gasChart', 'Gas Resistance', 'kΩ');
    initChart('lightChart', 'Light', 'lux');
    initChart('voltageChart', 'Voltage', 'V');
    initChart('currentChart', 'Current', 'mA');
    initChart('powerChart', 'Power', 'mW');
    initChart('battChart', 'Battery', '%');
    initChart('rssiChart', 'RSSI', 'dBm');
    
    // Hide all charts initially until data is loaded
    ['tempChart', 'humidityChart', 'pressureChart', 'gasChart', 'lightChart', 'voltageChart', 'currentChart', 'powerChart', 'battChart', 'rssiChart'].forEach(chartId => {
        const canvas = document.getElementById(chartId);
        const article = canvas?.closest('article');
        if (article) article.style.display = 'none';
    });
    
    // Connect WebSocket
    connectWebSocket();
    
    // Initial data load
    loadData();
    
    // Periodic refresh (fallback if WebSocket fails)
    setInterval(loadData, 30000); // Update every 30 seconds
    
    // Time range buttons
    document.querySelectorAll('.time-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            currentTimeWindow = this.dataset.range;
            document.querySelectorAll('.time-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            loadHistoricalData();
        });
    });
    
    // Sensor selection
    document.getElementById('sensorSelect').addEventListener('change', loadHistoricalData);
    
    // Zone filter
    document.getElementById('zoneFilter').addEventListener('change', loadData);
});