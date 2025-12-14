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
    
    let html = '';
    sensors.forEach(sensor => {
        const location = sensor.location || `Client ${sensor.id}`;
        const isOnline = sensor.ageSeconds < 300; // Online if seen in last 5 minutes
        const statusClass = isOnline ? 'status-online' : 'status-offline';
        
        // Format battery with power state and charging status
        const powerSource = sensor.battery === 0 ? 'On AC' : 'On Battery';
        const chargingState = sensor.battery === 0 ? 'N/A' : (sensor.charging ? 'Charging' : 'Discharging');
        const batteryDisplay = `${powerSource}/${sensor.battery}%/${chargingState}`;
        
        html += `
            <div class="sensor-item">
                <span class="status-indicator ${statusClass}"></span>
                <strong>${location}</strong> (ID: ${sensor.id})<br>
                Battery: ${batteryDisplay}, 
                RSSI: ${sensor.rssi} dBm,
                Last seen: ${sensor.age}
            </div>
        `;
    });
    sensorList.innerHTML = html;
    console.log('Updated sensor list with', sensors.length, 'sensors');
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
        
        // Temperature chart - hide it (client telemetry doesn't include sensor readings)
        const tempChartCard = document.getElementById('tempChart')?.closest('.card');
        if (tempChartCard) {
            tempChartCard.style.display = 'none';
        }
        
        // Battery chart
        if (charts['battChart']) {
            const battChartCard = document.getElementById('battChart')?.closest('.card');
            if (battChartCard) {
                battChartCard.style.display = 'block';
            }
            charts['battChart'].data.labels = labels;
            charts['battChart'].data.datasets[0].data = data.map(d => d.batt);
            charts['battChart'].update();
        }
        
        // RSSI chart
        if (charts['rssiChart']) {
            const rssiChartCard = document.getElementById('rssiChart')?.closest('.card');
            if (rssiChartCard) {
                rssiChartCard.style.display = 'block';
            }
            charts['rssiChart'].data.labels = labels;
            charts['rssiChart'].data.datasets[0].data = data.map(d => d.rssi);
            charts['rssiChart'].update();
        }
        
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
    initChart('battChart', 'Battery', '%');
    initChart('rssiChart', 'RSSI', 'dBm');
    
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
});