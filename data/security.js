let currentConfig = {};

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

// Load configuration on page load - sequential loading to avoid overload
window.onload = async function() {
    try {
        await loadConfig();
        await new Promise(resolve => setTimeout(resolve, 100));
        await loadWhitelist();
    } catch (error) {
        console.error('Initialization error:', error);
        showAlert('Failed to initialize page', 'error');
    }
};

function showAlert(message, type) {
    const alertBox = document.getElementById('alertBox');
    alertBox.textContent = message;
    alertBox.className = 'alert alert-' + type;
    alertBox.style.display = 'block';
    setTimeout(() => {
        alertBox.style.display = 'none';
    }, 5000);
}

async function loadConfig() {
    try {
        const response = await fetchWithTimeout('/api/security/config');
        const config = await response.json();
        currentConfig = config;
        
        // Update UI
        document.getElementById('encryptionToggle').checked = config.encryptionEnabled;
        document.getElementById('whitelistToggle').checked = config.whitelistEnabled;
        
        // Update status cards
        updateStatusCards();
        
        // Load and display current encryption key
        loadKey();
    } catch (error) {
        showAlert('Failed to load configuration: ' + error.message, 'error');
    }
}

function updateStatusCards() {
    const encCard = document.getElementById('encryptionStatus');
    const encText = document.getElementById('encryptionStatusText');
    const wlCard = document.getElementById('whitelistStatus');
    const wlText = document.getElementById('whitelistStatusText');
    
    if (currentConfig.encryptionEnabled) {
        encCard.className = 'status-card status-enabled';
        encText.textContent = 'Enabled';
    } else {
        encCard.className = 'status-card status-disabled';
        encText.textContent = 'Disabled';
    }
    
    if (currentConfig.whitelistEnabled) {
        wlCard.className = 'status-card status-enabled';
        wlText.textContent = 'Enabled';
    } else {
        wlCard.className = 'status-card status-disabled';
        wlText.textContent = 'Disabled';
    }
}

async function updateEncryption() {
    const enabled = document.getElementById('encryptionToggle').checked;
    
    try {
        const response = await fetchWithTimeout('/api/security/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                encryptionEnabled: enabled,
                whitelistEnabled: currentConfig.whitelistEnabled
            })
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            currentConfig.encryptionEnabled = enabled;
            updateStatusCards();
            showAlert('Encryption ' + (enabled ? 'enabled' : 'disabled'), 'success');
        } else {
            throw new Error(result.message || 'Failed to update');
        }
    } catch (error) {
        showAlert('Failed to update encryption: ' + error.message, 'error');
        document.getElementById('encryptionToggle').checked = currentConfig.encryptionEnabled;
    }
}

async function updateWhitelist() {
    const enabled = document.getElementById('whitelistToggle').checked;
    
    try {
        const response = await fetchWithTimeout('/api/security/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                encryptionEnabled: currentConfig.encryptionEnabled,
                whitelistEnabled: enabled
            })
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            currentConfig.whitelistEnabled = enabled;
            updateStatusCards();
            showAlert('Whitelist ' + (enabled ? 'enabled' : 'disabled'), 'success');
        } else {
            throw new Error(result.message || 'Failed to update');
        }
    } catch (error) {
        showAlert('Failed to update whitelist: ' + error.message, 'error');
        document.getElementById('whitelistToggle').checked = currentConfig.whitelistEnabled;
    }
}

async function loadWhitelist() {
    try {
        const response = await fetchWithTimeout('/api/security/whitelist');
        const data = await response.json();
        
        document.getElementById('deviceCount').textContent = data.count;
        
        const container = document.getElementById('whitelistContainer');
        
        if (data.count === 0) {
            container.innerHTML = '<div class="empty-state">No devices whitelisted</div>';
        } else {
            container.innerHTML = '';
            data.devices.forEach(deviceId => {
                const div = document.createElement('div');
                div.className = 'whitelist-device';
                div.innerHTML = `
                    <span>Device ID: ${deviceId}</span>
                    <button onclick="removeDevice(${deviceId})" class="btn btn-danger btn-sm">Remove</button>
                `;
                container.appendChild(div);
            });
        }
    } catch (error) {
        showAlert('Failed to load whitelist: ' + error.message, 'error');
    }
}

async function addDevice() {
    const input = document.getElementById('deviceIdInput');
    const deviceId = parseInt(input.value);
    
    if (!deviceId || deviceId < 1 || deviceId > 255) {
        showAlert('Please enter a valid device ID (1-255)', 'error');
        return;
    }
    
    try {
        const response = await fetchWithTimeout('/api/security/whitelist', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({deviceId: deviceId})
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            showAlert('Device ' + deviceId + ' added to whitelist', 'success');
            input.value = '';
            loadWhitelist();
        } else {
            throw new Error(result.message || 'Failed to add device');
        }
    } catch (error) {
        showAlert('Failed to add device: ' + error.message, 'error');
    }
}

async function removeDevice(deviceId) {
    if (!confirm('Remove device ' + deviceId + ' from whitelist?')) {
        return;
    }
    
    try {
        const response = await fetchWithTimeout('/api/security/whitelist/' + deviceId, {
            method: 'DELETE'
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            showAlert('Device ' + deviceId + ' removed from whitelist', 'success');
            loadWhitelist();
        } else {
            throw new Error(result.message || 'Failed to remove device');
        }
    } catch (error) {
        showAlert('Failed to remove device: ' + error.message, 'error');
    }
}

async function clearWhitelist() {
    if (!confirm('Clear ALL devices from whitelist? This cannot be undone!')) {
        return;
    }
    
    try {
        const response = await fetchWithTimeout('/api/security/whitelist', {
            method: 'DELETE'
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            showAlert('Whitelist cleared', 'success');
            loadWhitelist();
        } else {
            throw new Error(result.message || 'Failed to clear whitelist');
        }
    } catch (error) {
        showAlert('Failed to clear whitelist: ' + error.message, 'error');
    }
}

async function generateNewKey() {
    if (!confirm('Generate new encryption key? ALL SENSORS WILL NEED TO BE RECONFIGURED!')) {
        return;
    }
    
    try {
        const response = await fetchWithTimeout('/api/security/generate-key', {
            method: 'POST'
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            showAlert('New encryption key generated! Copy the key below to configure sensors.', 'success');
            // Display the new key
            if (result.key) {
                document.getElementById('keyDisplay').value = result.key;
            }
        } else {
            throw new Error(result.message || 'Failed to generate key');
        }
    } catch (error) {
        showAlert('Failed to generate key: ' + error.message, 'error');
    }
}

async function loadKey() {
    try {
        const response = await fetchWithTimeout('/api/security/key');
        const result = await response.json();
        if (result.key) {
            document.getElementById('keyDisplay').value = result.key;
        }
    } catch (error) {
        console.error('Failed to load key:', error);
    }
}

function copyKey() {
    const keyDisplay = document.getElementById('keyDisplay');
    keyDisplay.select();
    document.execCommand('copy');
    showAlert('Encryption key copied to clipboard!', 'success');
}

async function setCustomKey() {
    const keyInput = document.getElementById('keyInput');
    const key = keyInput.value.toUpperCase().trim();
    
    // Validate key format (32 hex characters)
    if (!/^[0-9A-F]{32}$/.test(key)) {
        showAlert('Invalid key format. Must be exactly 32 hex characters (0-9, A-F)', 'error');
        return;
    }
    
    if (!confirm('Set this custom encryption key? Make sure you have copied it - you will need it for all sensors!')) {
        return;
    }
    
    try {
        const response = await fetch('/api/security/key', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ key: key })
        });
        
        const result = await response.json();
        if (result.status === 'success') {
            showAlert('Encryption key updated! Configure all sensors with this key.', 'success');
            document.getElementById('keyDisplay').value = key;
            keyInput.value = '';
        } else {
            throw new Error(result.message || 'Failed to set key');
        }
    } catch (error) {
        showAlert('Failed to set key: ' + error.message, 'error');
    }
}
