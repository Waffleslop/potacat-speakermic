// ============================================================
// POTACAT Speakermic — UI Integration for ECHOCAT
// Adds speakermic controls to the ECHOCAT remote page
// ============================================================

const SpeakermicUI = (() => {
    let container = null;
    let statusIcon = null;
    let batteryEl = null;
    let connectBtn = null;

    /**
     * Initialize the speakermic UI.
     * Call after the ECHOCAT page has loaded.
     *
     * @param {HTMLElement} parentEl - Container to append speakermic controls to
     * @param {object} opts - Options:
     *   - pc: RTCPeerConnection instance
     *   - rxStream: Remote audio MediaStream
     *   - pttStart: Function to call when PTT starts
     *   - pttStop: Function to call when PTT stops
     */
    function init(parentEl, opts) {
        const { pc, rxStream, pttStart, pttStop } = opts;

        // Create UI container
        container = document.createElement('div');
        container.id = 'speakermic-controls';
        container.style.cssText = `
            display: flex; align-items: center; gap: 8px;
            padding: 6px 10px; background: #1a1a2e; border-radius: 8px;
            font-size: 13px; color: #ccc;
        `;

        // Status icon
        statusIcon = document.createElement('span');
        statusIcon.textContent = '\u{1F50A}';  // Speaker emoji
        statusIcon.style.cssText = 'font-size: 16px; opacity: 0.4;';
        container.appendChild(statusIcon);

        // Connect button
        connectBtn = document.createElement('button');
        connectBtn.textContent = 'Speakermic';
        connectBtn.style.cssText = `
            background: #2d2d4e; color: #aaa; border: 1px solid #444;
            border-radius: 6px; padding: 4px 10px; cursor: pointer;
            font-size: 12px;
        `;
        connectBtn.onclick = () => toggleConnection(pc, rxStream, pttStart, pttStop);
        container.appendChild(connectBtn);

        // Battery indicator
        batteryEl = document.createElement('span');
        batteryEl.style.cssText = 'font-size: 11px; color: #888; min-width: 36px;';
        batteryEl.textContent = '';
        container.appendChild(batteryEl);

        parentEl.appendChild(container);

        // Set up SPEAKERMIC event handlers
        SPEAKERMIC.onConnect = (name) => {
            statusIcon.style.opacity = '1';
            connectBtn.textContent = 'Disconnect';
            connectBtn.style.background = '#3d1a1a';
            connectBtn.style.color = '#f88';
            connectBtn.style.borderColor = '#833';
        };

        SPEAKERMIC.onDisconnect = () => {
            statusIcon.style.opacity = '0.4';
            connectBtn.textContent = 'Speakermic';
            connectBtn.style.background = '#2d2d4e';
            connectBtn.style.color = '#aaa';
            connectBtn.style.borderColor = '#444';
            batteryEl.textContent = '';
        };

        SPEAKERMIC.onBattery = (percent) => {
            batteryEl.textContent = percent + '%';
            if (percent <= 10) {
                batteryEl.style.color = '#f44';
            } else if (percent <= 25) {
                batteryEl.style.color = '#fa0';
            } else {
                batteryEl.style.color = '#888';
            }
        };

        SPEAKERMIC.onPtt = (active) => {
            if (active) {
                if (pttStart) pttStart();
            } else {
                if (pttStop) pttStop();
            }
        };
    }

    async function toggleConnection(pc, rxStream, pttStart, pttStop) {
        if (SPEAKERMIC.isConnected) {
            SPEAKERMIC.disconnect();
            return;
        }

        try {
            connectBtn.textContent = 'Connecting...';
            connectBtn.disabled = true;

            await SPEAKERMIC.connect();

            // Set up audio bridges
            if (pc) await SPEAKERMIC.setupTxBridge(pc);
            if (rxStream) await SPEAKERMIC.setupRxBridge(rxStream);

        } catch (err) {
            console.error('[SpeakermicUI] Connection failed:', err);
            connectBtn.textContent = 'Speakermic';
        } finally {
            connectBtn.disabled = false;
        }
    }

    function destroy() {
        SPEAKERMIC.disconnect();
        if (container && container.parentNode) {
            container.parentNode.removeChild(container);
        }
    }

    return { init, destroy };
})();
