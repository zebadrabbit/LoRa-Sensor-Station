// Fetch with timeout wrapper
async function fetchWithTimeout(url, options = {}, timeout = 5000) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);
    
    try {
        const response = await fetch(url, {
            ...options,
            signal: controller.signal
        });
        clearTimeout(timeoutId);
        return response;
    } catch (error) {
        clearTimeout(timeoutId);
        if (error.name === 'AbortError') {
            throw new Error('Request timeout');
        }
        throw error;
    }
}

let sensors = [];
let selectedSensor = null;
let commandPending = false;

function disableAllButtons() {
    commandPending = true;
    document.getElementById('updateLocationBtn').disabled = true;
    document.getElementById('updateIntervalBtn').disabled = true;
    document.querySelectorAll('.preset-btn').forEach(btn => btn.disabled = true);
    document.querySelectorAll('.action-btn').forEach(btn => btn.disabled = true);
}

function enableAllButtons() {
    commandPending = false;
    document.getElementById('updateLocationBtn').disabled = false;
    document.getElementById('updateIntervalBtn').disabled = false;
    document.querySelectorAll('.preset-btn').forEach(btn => btn.disabled = false);
    document.querySelectorAll('.action-btn').forEach(btn => btn.disabled = false);
}

function setButtonLoading(buttonId, loading, originalText) {
    const btn = document.getElementById(buttonId);
    if (loading) {
        btn.dataset.originalText = originalText;
        btn.innerHTML = '⏳ Waiting for ACK...';
        btn.disabled = true;
    } else {
        btn.innerHTML = btn.dataset.originalText || originalText;
        btn.disabled = false;
    }
}

async function loadSensors() {
    try {
        const response = await fetchWithTimeout('/api/sensors');
        sensors = await response.json();
        
        // Check for sensor ID in URL parameter
        const urlParams = new URLSearchParams(window.location.search);
        const sensorIdParam = urlParams.get('sensor');
        
        if (sensorIdParam) {
            const sensorId = parseInt(sensorIdParam);
            selectedSensor = sensors.find(s => s.id == sensorId);
            
            if (selectedSensor) {
                updateSensorDisplay();
            } else {
                showMessage('Sensor not found', 'error');
                window.location.href = '/';
            }
        } else {
            // No sensor specified, redirect to dashboard
            showMessage('No sensor specified', 'error');
            setTimeout(() => window.location.href = '/', 1500);
        }
    } catch (error) {
        console.error('Error loading sensors:', error);
        showMessage('Failed to load sensors', 'error');
    }
}

function updateSensorDisplay() {
    if (selectedSensor) {
        document.getElementById('currentSensorId').textContent = selectedSensor.id;
        document.getElementById('currentLocation').textContent = selectedSensor.location || 'Not set';
        document.getElementById('lastSeen').textContent = selectedSensor.age || 'Unknown';
        document.getElementById('battery').textContent = selectedSensor.battery || '0';
        
        // Pre-fill location field
        document.getElementById('locationInput').value = selectedSensor.location || '';
    }
}

function setInterval(seconds) {
    document.getElementById('intervalInput').value = seconds;
}

// Execute immediately when script loads (no DOMContentLoaded needed with defer)
document.getElementById('locationForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        
        if (!selectedSensor) {
            showMessage('Please select a sensor first', 'error');
            return;
        }
        
        if (commandPending) {
            showMessage('Please wait for the previous command to complete', 'error');
            return;
        }
        
        const location = document.getElementById('locationInput').value.trim();
        if (!location) {
            showMessage('Location cannot be empty', 'error');
            return;
        }
        
        disableAllButtons();
        setButtonLoading('updateLocationBtn', true, '💾 Update Location');
        
        try {
            const response = await fetchWithTimeout('/api/remote-config/location', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    id: selectedSensor.id,
                    location: location
                })
            });
            
            const result = await response.json();
            
            if (result.success) {
                showMessage(`Location updated to "${location}". Waiting for sensor acknowledgment...`, 'success');
                
                // Wait 2 seconds, then refresh sensor data
                setTimeout(async () => {
                    await loadSensors();
                    updateSensorDisplay();
                    
                    enableAllButtons();
                    setButtonLoading('updateLocationBtn', false, '💾 Update Location');
                    showMessage(`Location updated successfully!`, 'success');
                }, 2000);
            } else {
                enableAllButtons();
                setButtonLoading('updateLocationBtn', false, '💾 Update Location');
                showMessage(`Failed: ${result.message || 'Unknown error'}`, 'error');
            }
        } catch (error) {
            console.error('Error:', error);
            enableAllButtons();
            setButtonLoading('updateLocationBtn', false, '💾 Update Location');
            showMessage('Failed to send command', 'error');
        }
    });
    
    document.getElementById('intervalForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        
        if (!selectedSensor) {
            showMessage('Please select a sensor first', 'error');
            return;
        }
        
        if (commandPending) {
            showMessage('Please wait for the previous command to complete', 'error');
            return;
        }
        
        const interval = parseInt(document.getElementById('intervalInput').value);
        if (interval < 10 || interval > 3600) {
            showMessage('Interval must be between 10 and 3600 seconds', 'error');
            return;
        }
        
        disableAllButtons();
        setButtonLoading('updateIntervalBtn', true, '💾 Update Interval');
        
        try {
            const response = await fetchWithTimeout('/api/remote-config/interval', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    id: selectedSensor.id,
                    interval: interval
                })
            });
            
            const result = await response.json();
            
            if (result.success) {
                showMessage(`Interval updated to ${interval}s. Waiting for sensor acknowledgment...`, 'success');
                
                // Wait 2 seconds, then refresh sensor data
                setTimeout(async () => {
                    await loadSensors();
                    const sensorSelect = document.getElementById('sensorSelect');
                    sensorSelect.value = selectedSensor.id;
                    onSensorSelected();
                    
                    enableAllButtons();
                    setButtonLoading('updateIntervalBtn', false, '💾 Update Interval');
                    showMessage(`Interval updated successfully!`, 'success');
                }, 2000);
            } else {
                enableAllButtons();
                setButtonLoading('updateIntervalBtn', false, '💾 Update Interval');
                showMessage(`Failed: ${result.message || 'Unknown error'}`, 'error');
            }
        } catch (error) {
            console.error('Error:', error);
            enableAllButtons();
            setButtonLoading('updateIntervalBtn', false, '💾 Update Interval');
            showMessage('Failed to send command', 'error');
        }
    });
    
    // Initialize - load sensor from URL parameter
    setTimeout(() => {
        loadSensors();
    }, 250);
    
// Auto-refresh sensor data every 30 seconds
setInterval(() => {
    if (selectedSensor) {
        loadSensors();
    }
}, 30000);

async function restartSensor() {
    if (!selectedSensor) {
        showMessage('Please select a sensor first', 'error');
        return;
    }
    
    if (commandPending) {
        showMessage('Please wait for the previous command to complete', 'error');
        return;
    }
    
    if (!confirm(`Are you sure you want to restart sensor ${selectedSensor.id}?`)) {
        return;
    }
    
    disableAllButtons();
    setButtonLoading('restartBtn', true, '🔄 Restart Sensor');
    
    try {
        const response = await fetchWithTimeout('/api/remote-config/restart', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                id: selectedSensor.id
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showMessage(`Restart command sent. Sensor will reboot in ~1 second...`, 'success');
            
            // Wait 3 seconds for sensor to reboot, then refresh
            setTimeout(async () => {
                await loadSensors();
                enableAllButtons();
                setButtonLoading('restartBtn', false, '🔄 Restart Sensor');
                showMessage(`Sensor restarted successfully!`, 'success');
            }, 3000);
        } else {
            enableAllButtons();
            setButtonLoading('restartBtn', false, '🔄 Restart Sensor');
            showMessage(`Failed: ${result.message || 'Unknown error'}`, 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        enableAllButtons();
        setButtonLoading('restartBtn', false, '🔄 Restart Sensor');
        showMessage('Failed to send command', 'error');
    }
}

async function pingSensor() {
    if (!selectedSensor) {
        showMessage('Please select a sensor first', 'error');
        return;
    }
    
    showMessage('Note: Ping functionality requires sensor to implement CMD_PING. Currently, sensors send data on their regular interval. Use "Update Interval" to adjust timing.', 'info');
}

function showMessage(text, type) {
    const message = document.getElementById('messageBox');
    message.textContent = text;
    message.className = 'message ' + type;
    message.classList.remove('hidden');
    
    if (type === 'success') {
        setTimeout(() => {
            message.classList.add('hidden');
        }, 10000);
    }
}
