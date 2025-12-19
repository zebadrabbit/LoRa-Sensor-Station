let charts = {};
let currentTimeWindow = 'all';  // Match the default active button
let ws;
let pollTimer = null;
let reconnectTimer = null;
let dataRefreshTimer = null;

function updateClock() {
    const now = new Date();
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    const timeString = `${hours}:${minutes}:${seconds}`;
    const timeElement = document.getElementById('currentTime');
    if (timeElement) {
        timeElement.textContent = timeString;
    }
}

function startPolling() {
    if (pollTimer) return;
    pollTimer = setInterval(() => {
        loadData();
        loadHistoricalData();
    }, 5000);
    console.log('📡 Fallback polling started (5s)');
    document.getElementById('wsStatus').textContent = '🟠 Polling';
}

function stopPolling() {
    if (pollTimer) {
        clearInterval(pollTimer);
        pollTimer = null;
        console.log('🛑 Fallback polling stopped');
    }
}

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
    
    // Update zone filter dropdown if it exists
    const zoneFilter = document.getElementById('zoneFilter');
    let currentZone = '';
    if (zoneFilter) {
        currentZone = zoneFilter.value;
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
    }
    
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
            <div class="col-md-4 mb-3 client-card">
                <div class="card" style="background: #353d52; border-left: 4px solid #8b9bff; height: 100%;">
                    <div class="card-body">
                        <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 12px;">
                            <span class="status-indicator ${statusClass}" style="width: 10px; height: 10px; border-radius: 50%; ${isOnline ? 'background: #27ae60;' : 'background: #e74c3c;'}"></span>
                            <strong style="font-size: 16px; color: #ffffff;">${location}</strong>
                            <span style="background: #353d52; padding: 2px 8px; border-radius: 12px; font-size: 11px; color: #9ca3af;">ID: ${sensor.id}</span>
                            ${sensor.zone ? `<span style="background: #353d52; padding: 2px 8px; border-radius: 12px; font-size: 11px; color: #9ca3af;">📍 ${sensor.zone}</span>` : ''}
                        </div>
                        <div style="display: flex; gap: 6px; margin-bottom: 12px; flex-wrap: wrap;">
                            <span style="background: ${priorityColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;">${sensor.priority || 'Medium'}</span>
                            <span style="background: ${healthColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;" title="Communication: ${(health * 100).toFixed(0)}%">❤️ ${healthLabel}</span>
                            <a href="/runtime-config?sensor=${sensor.id}" style="margin-left: auto; padding: 4px 10px; background: #8b9bff; color: white; text-decoration: none; border-radius: 6px; font-size: 13px; font-weight: 600;" title="Configure sensor">⚙️ Config</a>
                        </div>
                        <div style="display: grid; grid-template-columns: 1fr; gap: 8px; font-size: 14px; color: #d1d5db;">
                            <div>🔋 <strong>Battery:</strong> ${batteryDisplay}</div>
                            <div>📶 <strong>RSSI:</strong> ${sensor.rssi} dBm</div>
                            <div>🕐 <strong>Last seen:</strong> ${sensor.age}</div>
                        </div>
                    </div>
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
                borderColor: '#8b9bff',
                backgroundColor: 'rgba(139, 155, 255, 0.15)',
                borderWidth: 2.5,
                pointBackgroundColor: '#8b9bff',
                pointBorderColor: '#fff',
                pointBorderWidth: 2,
                pointRadius: 3,
                pointHoverRadius: 5,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { 
                    display: false 
                },
                tooltip: {
                    backgroundColor: 'rgba(45, 52, 70, 0.95)',
                    titleColor: '#ffffff',
                    bodyColor: '#e5e7eb',
                    borderColor: '#8b9bff',
                    borderWidth: 1
                }
            },
            scales: {
                x: {
                    grid: {
                        color: 'rgba(255, 255, 255, 0.08)',
                        lineWidth: 1
                    },
                    ticks: {
                        color: '#9ca3af'
                    }
                },
                y: {
                    beginAtZero: false,
                    grid: {
                        color: 'rgba(255, 255, 255, 0.08)',
                        lineWidth: 1
                    },
                    ticks: {
                        color: '#9ca3af'
                    },
                    title: { 
                        display: true, 
                        text: unit,
                        color: '#e5e7eb'
                    }
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
            const container = canvas?.closest('.card');
            if (container) {
                if (chart.hasData) {
                    container.style.display = 'block';
                    if (charts[chart.id]) {
                        charts[chart.id].data.labels = labels;
                        charts[chart.id].data.datasets[0].data = data.map(d => d[chart.field]);
                        charts[chart.id].update();
                        console.log(`Updated ${chart.id} with ${data.length} points`);
                    }
                } else {
                    container.style.display = 'none';
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
    // Clean up any existing WebSocket first
    if (ws) {
        console.log('🔄 Closing existing WebSocket before creating new one');
        ws.onclose = null; // Prevent reconnection
        ws.onerror = null;
        ws.onmessage = null;
        ws.onopen = null;
        if (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING) {
            ws.close();
        }
        ws = null;
    }
    
    const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const wsUrl = `${scheme}://${window.location.host}/ws`;
    console.log('Connecting to WebSocket:', wsUrl);
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log('✅ WebSocket connected');
        document.getElementById('wsStatus').textContent = '🟢 Connected';
        stopPolling();
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
        console.error('❌ WebSocket error:', error, 'readyState:', ws.readyState, 'url:', wsUrl);
        document.getElementById('wsStatus').textContent = '🔴 Error';
        startPolling();
    };
    
    ws.onclose = () => {
        console.log('⚠️ WebSocket disconnected, will reconnect in 5s...');
        document.getElementById('wsStatus').textContent = '🟡 Reconnecting...';
        startPolling();
        // Reconnect after 5 seconds
        if (reconnectTimer) clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(connectWebSocket, 5000);
    };
}

// Cleanup function to close connections when leaving the page
function cleanup() {
    console.log('🧹 Cleaning up dashboard resources...');
    
    // Stop all timers
    stopPolling();
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }
    if (dataRefreshTimer) {
        clearInterval(dataRefreshTimer);
        dataRefreshTimer = null;
    }
    
    // Close WebSocket
    if (ws) {
        ws.onclose = null; // Prevent reconnection
        ws.onerror = null;
        ws.onmessage = null;
        ws.onopen = null;
        try {
            if (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING) {
                ws.close();
            }
        } catch (e) {
            console.warn('Error closing WebSocket:', e);
        }
        ws = null;
        console.log('🔌 WebSocket closed');
    }
}

// Initialize
function initDashboard() {
    console.log('📱 Dashboard initializing...');
    
    // Clean up any existing resources first (in case page was soft-reloaded)
    cleanup();
    
    // Only initialize charts that exist in the HTML
    initChart('tempChart', 'Temperature', '°C');
    initChart('battChart', 'Battery', '%');
    initChart('rssiChart', 'RSSI', 'dBm');
    
    // Start clock update
    updateClock();
    setInterval(updateClock, 1000);
    
    // Connect WebSocket
    connectWebSocket();
    
    // Initial data load
    loadData();
    
    // Periodic refresh (fallback if WebSocket fails)
    dataRefreshTimer = setInterval(loadData, 30000); // Update every 30 seconds
    
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
    const sensorSelect = document.getElementById('sensorSelect');
    if (sensorSelect) {
        sensorSelect.addEventListener('change', loadHistoricalData);
    }
    
    // Zone filter
    const zoneFilter = document.getElementById('zoneFilter');
    if (zoneFilter) {
        zoneFilter.addEventListener('change', loadData);
    }
}

// Execute initialization when DOM is ready (handle both cases: already loaded or still loading)
function waitForChartAndInit() {
    if (typeof Chart === 'undefined') {
        console.log('Waiting for Chart.js to load...');
        setTimeout(waitForChartAndInit, 100);
        return;
    }
    console.log('Chart.js loaded, initializing dashboard');
    initDashboard();
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', waitForChartAndInit);
} else {
    // DOM already loaded, but check for Chart.js
    waitForChartAndInit();
}

// Clean up when user navigates away from the dashboard
window.addEventListener('beforeunload', cleanup);
window.addEventListener('pagehide', cleanup);

// Handle page visibility changes (tab switching or navigating away)
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        console.log('📴 Dashboard hidden, pausing updates');
        stopPolling();
        if (dataRefreshTimer) {
            clearInterval(dataRefreshTimer);
            dataRefreshTimer = null;
        }
    } else {
        console.log('📱 Dashboard visible, resuming updates');
        // Reconnect WebSocket if needed
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            connectWebSocket();
        }
        // Restart data refresh
        if (!dataRefreshTimer) {
            dataRefreshTimer = setInterval(loadData, 30000);
        }
        // Immediate refresh
        loadData();
    }
});