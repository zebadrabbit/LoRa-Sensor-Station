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

// Execute immediately when script loads (no DOMContentLoaded needed with defer)
console.log('LoRa Settings page loaded');

async function loadConfig() {
        try {
            console.log('Fetching current config...');
            const response = await fetchWithTimeout('/api/lora/config');
            const config = await response.json();
            console.log('Config loaded:', config);
            
            // Update current values display
            document.getElementById('currentNetworkId').textContent = config.networkId || 'Not set';
            document.getElementById('currentRegion').textContent = config.region || 'US915';
            document.getElementById('currentFrequency').textContent = (config.frequency || 915000000) / 1000000 + ' MHz';
            document.getElementById('currentSF').textContent = 'SF' + (config.spreadingFactor || 10);
            document.getElementById('currentBW').textContent = (config.bandwidth || 125) + ' kHz';
            document.getElementById('currentTxPower').textContent = (config.txPower || 14) + ' dBm';
            
            // Update form fields
            document.getElementById('region').value = config.region || 'US915';
            document.getElementById('frequency').value = config.frequency || 915000000;
            document.getElementById('spreadingFactor').value = config.spreadingFactor || 10;
            document.getElementById('bandwidth').value = config.bandwidth || 125;
            document.getElementById('txPower').value = config.txPower || 14;
            document.getElementById('codingRate').value = config.codingRate || 1;
            document.getElementById('preambleLength').value = config.preambleLength || 8;
        } catch (error) {
            console.error('Error loading configuration:', error);
            showMessage('Failed to load configuration', 'error');
        }
    }
    
    document.getElementById('loraForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        console.log('Form submitted!');
        
        const formData = new FormData(e.target);
        const config = {
            region: formData.get('region'),
            frequency: parseInt(formData.get('frequency')),
            spreadingFactor: parseInt(formData.get('spreadingFactor')),
            bandwidth: parseInt(formData.get('bandwidth')),
            txPower: parseInt(formData.get('txPower')),
            codingRate: parseInt(formData.get('codingRate')),
            preambleLength: parseInt(formData.get('preambleLength'))
        };
        
        console.log('Sending config:', config);
        
        // Show status modal
        const modal = document.getElementById('statusModal');
        modal.style.display = 'flex';
        console.log('Modal should be visible now');
    
    try {
        // Step 1: Saving
        setStepActive('step1');
        await new Promise(resolve => setTimeout(resolve, 500));
        
        const response = await fetchWithTimeout('/api/lora/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        const result = await response.json();
        
        if (result.success) {
            setStepComplete('step1');
            
            // Step 2: Broadcasting
            setStepActive('step2');
            await new Promise(resolve => setTimeout(resolve, 800));
            setStepComplete('step2');
            
            // Show sensor info
            document.getElementById('sensorInfo').style.display = 'block';
            document.getElementById('sensorCount').textContent = result.sensorsFound || 0;
            document.getElementById('commandCount').textContent = result.commandsSent || 0;
            
            // Step 3: Waiting for ACKs - REAL-TIME MONITORING
            setStepActive('step3');
            
            // Poll for ACK status
            let allAcked = false;
            let timedOut = false;
            const maxPolls = 40; // 20 seconds (500ms intervals)
            let pollCount = 0;
            
            const ackPollInterval = setInterval(async () => {
                try {
                    const statusResponse = await fetchWithTimeout('/api/lora/reboot-status');
                    const status = await statusResponse.json();
                    
                    // Update display with ACK progress
                    const step3 = document.getElementById('step3');
                    if (status.trackingActive) {
                        step3.textContent = `⏳ Waiting for sensor ACKs... (${status.ackedCount}/${status.totalSensors})`;
                    }
                    
                    // Check if all sensors ACKed
                    if (status.allAcked || !status.trackingActive) {
                        allAcked = true;
                        clearInterval(ackPollInterval);
                        setStepComplete('step3');
                        
                        // Move to step 4
                        setStepActive('step4');
                        await new Promise(resolve => setTimeout(resolve, 5000));
                        setStepComplete('step4');
                        
                        // Move to step 5
                        startBaseStationRebootMonitoring();
                    }
                    
                    // Check for timeout
                    if (status.timedOut) {
                        timedOut = true;
                        clearInterval(ackPollInterval);
                        step3.textContent = `⚠️ Partial ACKs received (${status.ackedCount}/${status.totalSensors}) - proceeding anyway`;
                        setStepComplete('step3');
                        
                        // Still proceed to reboot
                        setStepActive('step4');
                        await new Promise(resolve => setTimeout(resolve, 3000));
                        setStepComplete('step4');
                        
                        startBaseStationRebootMonitoring();
                    }
                    
                    pollCount++;
                    if (pollCount >= maxPolls && !allAcked && !timedOut) {
                        // Safety timeout
                        clearInterval(ackPollInterval);
                        setStepComplete('step3');
                        startBaseStationRebootMonitoring();
                    }
                } catch (error) {
                    console.error('Error polling ACK status:', error);
                }
            }, 500);  // Poll every 500ms
            
            // Function to monitor base station reboot
            async function startBaseStationRebootMonitoring() {
                setStepActive('step5');
                document.getElementById('rebootTimer').style.display = 'block';
                
                let countdown = 30;
                const countdownInterval = setInterval(async () => {
                    countdown--;
                    document.getElementById('countdown').textContent = countdown;
                    
                    // Try to ping the server
                    if (countdown % 2 === 0) {
                        try {
                            const pingResponse = await fetchWithTimeout('/api/status', { 
                                method: 'GET',
                                cache: 'no-cache'
                            });
                            if (pingResponse.ok) {
                                // Server is back online
                                clearInterval(countdownInterval);
                                setStepComplete('step5');
                                document.getElementById('rebootTimer').innerHTML = '<strong style="color: #4CAF50;">✅ Base station is back online! Redirecting...</strong>';
                                setTimeout(() => {
                                    window.location.href = '/';
                                }, 1500);
                            }
                        } catch (e) {
                            // Server not ready yet
                        }
                    }
                    
                    if (countdown <= 0) {
                        clearInterval(countdownInterval);
                        setStepComplete('step5');
                        document.getElementById('rebootTimer').innerHTML = '<strong style="color: #4CAF50;">✅ Redirecting to dashboard...</strong>';
                        setTimeout(() => {
                            window.location.href = '/';
                        }, 1000);
                    }
                }, 1000);
            }
            
        } else {
            modal.style.display = 'none';
            showMessage('Failed to save configuration: ' + (result.error || 'Unknown error'), 'error');
        }
    } catch (error) {
        console.error('Error saving configuration:', error);
        modal.style.display = 'none';
        showMessage('Failed to save configuration', 'error');
    }
});

function setStepActive(stepId) {
    const step = document.getElementById(stepId);
    step.style.display = 'block';
    step.classList.add('active');
}

function setStepComplete(stepId) {
    const step = document.getElementById(stepId);
    step.classList.remove('active');
    step.classList.add('complete');
    step.textContent = step.textContent.replace('⏳', '✅');
}

function showMessage(text, type) {
    const message = document.getElementById('messageBox');
    message.textContent = text;
    message.className = 'message ' + type;
    message.classList.remove('hidden');
    
    if (type === 'success') {
        setTimeout(() => {
            message.classList.add('hidden');
        }, 5000);
    }
}

// Auto-update frequency when region changes
document.getElementById('region').addEventListener('change', (e) => {
    const freqMap = {
        'US915': 915000000,
        'EU868': 868100000,
        'AU915': 915000000,
        'AS923': 923000000,
        'CN470': 470000000,
        'IN865': 865000000
    };
    document.getElementById('frequency').value = freqMap[e.target.value] || 915000000;
});

// Small delay to let AsyncWebServer free up the connection used to serve this HTML
setTimeout(() => {
    loadConfig();
}, 250);
