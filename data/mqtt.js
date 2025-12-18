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

async function loadConfig() {
    try {
        const response = await fetchWithTimeout('/api/mqtt/config');
        const config = await response.json();
        
        document.getElementById('enabled').checked = config.enabled;
        document.getElementById('broker').value = config.broker;
        document.getElementById('port').value = config.port;
        document.getElementById('username').value = config.username;
        document.getElementById('password').value = config.password;
        document.getElementById('topicPrefix').value = config.topicPrefix;
        document.getElementById('haDiscovery').checked = config.haDiscovery;
        document.getElementById('qos').value = config.qos;
    } catch (error) {
        showMessage('Failed to load configuration', 'error');
    }
}

async function loadStats() {
    try {
        const response = await fetchWithTimeout('/api/mqtt/stats');
        const stats = await response.json();
        
        document.getElementById('mqttStatus').textContent = stats.connected ? '🟢 Connected' : '🔴 Disconnected';
        document.getElementById('mqttPublishes').textContent = stats.publishes;
        document.getElementById('mqttFailures').textContent = stats.failures;
        document.getElementById('mqttReconnects').textContent = stats.reconnects;
    } catch (error) {
        console.error('Failed to load stats:', error);
    }
}

document.getElementById('mqttForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const config = {
        enabled: document.getElementById('enabled').checked,
        broker: document.getElementById('broker').value,
        port: parseInt(document.getElementById('port').value),
        username: document.getElementById('username').value,
        password: document.getElementById('password').value,
        topicPrefix: document.getElementById('topicPrefix').value,
        haDiscovery: document.getElementById('haDiscovery').checked,
        qos: parseInt(document.getElementById('qos').value)
    };
    
    try {
        const response = await fetchWithTimeout('/api/mqtt/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        const result = await response.json();
        if (result.success) {
            showMessage('Configuration saved successfully!', 'success');
            setTimeout(loadStats, 1000);
        } else {
            showMessage('Failed to save configuration', 'error');
        }
    } catch (error) {
        showMessage('Error saving configuration: ' + error.message, 'error');
    }
});

async function testConnection() {
    showMessage('🔄 Testing MQTT connection...', 'info');
    
    try {
        const response = await fetchWithTimeout('/api/mqtt/test', { method: 'POST' });
        const result = await response.json();
        
        if (result.success) {
            showMessage('✅ MQTT connection successful!', 'success');
            setTimeout(loadStats, 500);
        } else {
            showMessage('❌ Connection failed: ' + result.message, 'error');
        }
    } catch (error) {
        showMessage('Error testing connection: ' + error.message, 'error');
    }
}

function showMessage(message, type) {
    const box = document.getElementById('messageBox');
    box.textContent = message;
    box.className = 'alert alert-' + type;
    box.classList.remove('hidden');
    
    if (type === 'success' || type === 'info') {
        setTimeout(() => box.classList.add('hidden'), 5000);
    }
}

loadConfig();
loadStats();
setInterval(loadStats, 5000);
