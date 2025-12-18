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

// Load current configuration
async function loadConfig() {
    try {
        const response = await fetchWithTimeout('/api/alerts/config');
        const config = await response.json();
        
        document.getElementById('teamsEnabled').checked = config.teamsEnabled;
        document.getElementById('teamsWebhook').value = config.teamsWebhook;
        document.getElementById('emailEnabled').checked = config.emailEnabled;
        document.getElementById('smtpServer').value = config.smtpServer;
        document.getElementById('smtpPort').value = config.smtpPort;
        document.getElementById('emailUser').value = config.emailUser;
        document.getElementById('emailPassword').value = config.emailPassword;
        document.getElementById('emailFrom').value = config.emailFrom;
        document.getElementById('emailTo').value = config.emailTo;
        document.getElementById('emailTLS').checked = config.emailTLS;
        document.getElementById('tempHigh').value = config.tempHigh;
        document.getElementById('tempLow').value = config.tempLow;
        document.getElementById('batteryLow').value = config.batteryLow;
        document.getElementById('batteryCritical').value = config.batteryCritical;
        document.getElementById('sensorTimeout').value = config.timeout;
        document.getElementById('alertTempHigh').checked = config.alertTempHigh;
        document.getElementById('alertTempLow').checked = config.alertTempLow;
        document.getElementById('alertBatteryLow').checked = config.alertBatteryLow;
        document.getElementById('alertBatteryCritical').checked = config.alertBatteryCritical;
        document.getElementById('alertSensorOffline').checked = config.alertSensorOffline;
        document.getElementById('alertSensorOnline').checked = config.alertSensorOnline;
    } catch (error) {
        showMessage('Failed to load configuration', 'error');
    }
}

// Save configuration
document.getElementById('alertsForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const config = {
        enabled: document.getElementById('teamsEnabled').checked,
        webhook: document.getElementById('teamsWebhook').value,
        emailEnabled: document.getElementById('emailEnabled').checked,
        smtpServer: document.getElementById('smtpServer').value,
        smtpPort: parseInt(document.getElementById('smtpPort').value),
        emailUser: document.getElementById('emailUser').value,
        emailPassword: document.getElementById('emailPassword').value,
        emailFrom: document.getElementById('emailFrom').value,
        emailTo: document.getElementById('emailTo').value,
        emailTLS: document.getElementById('emailTLS').checked,
        tempHigh: parseFloat(document.getElementById('tempHigh').value),
        tempLow: parseFloat(document.getElementById('tempLow').value),
        batteryLow: parseInt(document.getElementById('batteryLow').value),
        batteryCritical: parseInt(document.getElementById('batteryCritical').value),
        timeout: parseInt(document.getElementById('sensorTimeout').value),
        alertTempHigh: document.getElementById('alertTempHigh').checked,
        alertTempLow: document.getElementById('alertTempLow').checked,
        alertBatteryLow: document.getElementById('alertBatteryLow').checked,
        alertBatteryCritical: document.getElementById('alertBatteryCritical').checked,
        alertSensorOffline: document.getElementById('alertSensorOffline').checked,
        alertSensorOnline: document.getElementById('alertSensorOnline').checked
    };
    
    try {
        const response = await fetchWithTimeout('/api/alerts/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        const result = await response.json();
        if (result.success) {
            showMessage('Configuration saved successfully!', 'success');
        } else {
            showMessage('Failed to save configuration', 'error');
        }
    } catch (error) {
        showMessage('Error saving configuration: ' + error.message, 'error');
    }
});

// Test webhook
async function testWebhook() {
    const webhook = document.getElementById('teamsWebhook').value;
    if (!webhook) {
        showMessage('Please enter a webhook URL first', 'error');
        return;
    }
    
    showMessage('Test alert...', 'info');
    
    try {
        const response = await fetchWithTimeout('/api/alerts/test', { method: 'POST' });
        const result = await response.json();
        
        if (result.success) {
            showMessage('Test alert sent! Check your Teams channel.', 'success');
        } else {
            showMessage('Test alert: ' + result.message, 'error');
        }
    } catch (error) {
        showMessage('Test alert: ' + error.message, 'error');
    }
}

// Test email
async function testEmail() {
    const emailTo = document.getElementById('emailTo').value;
    if (!emailTo) {
        showMessage('Please enter a recipient email address first', 'error');
        return;
    }
    
    showMessage('Sending test email...', 'info');
    
    try {
        const response = await fetchWithTimeout('/api/alerts/test-email', { method: 'POST' });
        const result = await response.json();
        
        if (result.success) {
            showMessage('Test email sent! Check your inbox.', 'success');
        } else {
            showMessage('Failed to send test email: ' + result.message, 'error');
        }
    } catch (error) {
        showMessage('Sending test email: ' + error.message, 'error');
    }
}

// Show message
function showMessage(message, type) {
    const box = document.getElementById('messageBox');
    box.textContent = message;
    box.className = 'alert alert-' + type;
    box.classList.remove('hidden');
    
    if (type === 'success' || type === 'info') {
        setTimeout(() => box.classList.add('hidden'), 5000);
    }
}

// Load config on page load
document.addEventListener('DOMContentLoaded', () => {
    console.log('Alerts page loaded');
    // Small delay to let AsyncWebServer free up the connection used to serve this HTML
    setTimeout(() => {
        loadConfig();
    }, 250);
});
