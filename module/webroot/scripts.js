let actionRunning = false;
let initialPinchDistance = null;
let currentFontSize = 14;
const MIN_FONT_SIZE = 8;
const MAX_FONT_SIZE = 24;

// Execute shell commands with ksu.exec
async function execCommand(command) {
    const callbackName = `exec_callback_${Date.now()}`;
    return new Promise((resolve, reject) => {
        window[callbackName] = (errno, stdout, stderr) => {
            delete window[callbackName];
            errno === 0 ? resolve(stdout) : reject(stderr);
        };
        ksu.exec(command, "{}", callbackName);
    });
}

// Apply button event listeners
function applyButtonEventListeners() {
    const fetchButton = document.getElementById('fetch');
    const sdkVendingToggle = document.getElementById('sdk-vending-toggle-container');
    const clearButton = document.querySelector('.clear-terminal');

    fetchButton.addEventListener('click', runAction);
    sdkVendingToggle.addEventListener('click', async () => {
        try {
            const pifPath = await execCommand(`
                [ ! -f /data/adb/modules/playintegrityfix/pif.json ] || echo /data/adb/modules/playintegrityfix/pif.json
                [ ! -f /data/adb/pif.json ] || echo /data/adb/pif.json
            `);
            if (pifPath.trim() === "") {
                appendToOutput("[!] No pif.json found");
                return;
            }
            const isChecked = document.getElementById('toggle-sdk-vending').checked;
            const paths = pifPath.trim().split('\n');
            for (const path of paths) {
                if (path) {
                    await execCommand(`sed -i 's/"spoofVendingSdk": [01]/"spoofVendingSdk": ${isChecked ? 0 : 1}/' ${path}`);
                }
            }
            appendToOutput(`[+] Successfully changed spoofVendingSdk to ${isChecked ? 0 : 1}`);
            document.getElementById('toggle-sdk-vending').checked = !isChecked;
        } catch (error) {
            appendToOutput("[-] Failed to change spoofVendingSdk");
            console.error('Failed to toggle sdk vending:', error);
        }
    });
    clearButton.addEventListener('click', () => {
        const output = document.querySelector('.output-terminal-content');
        output.innerHTML = '';
        currentFontSize = 14;
        updateFontSize(currentFontSize);
    });

    const terminal = document.querySelector('.output-terminal-content');
    
    terminal.addEventListener('touchstart', (e) => {
        if (e.touches.length === 2) {
            e.preventDefault();
            initialPinchDistance = getDistance(e.touches[0], e.touches[1]);
        }
    }, { passive: false });

    terminal.addEventListener('touchmove', (e) => {
        if (e.touches.length === 2) {
            e.preventDefault();
            const currentDistance = getDistance(e.touches[0], e.touches[1]);
            
            if (initialPinchDistance === null) {
                initialPinchDistance = currentDistance;
                return;
            }

            const scale = currentDistance / initialPinchDistance;
            const newFontSize = currentFontSize * scale;
            updateFontSize(newFontSize);
            initialPinchDistance = currentDistance;
        }
    }, { passive: false });

    terminal.addEventListener('touchend', () => {
        initialPinchDistance = null;
    });
}

// Function to load the version from module.prop
async function loadVersionFromModuleProp() {
    const versionElement = document.getElementById('version-text');
    try {
        const version = await execCommand("grep '^version=' /data/adb/modules/playintegrityfix/module.prop | cut -d'=' -f2");
        versionElement.textContent = version.trim();
    } catch (error) {
        appendToOutput("[-] Failed to read version from module.prop");
        console.error("Failed to read version from module.prop:", error);
    }
}

// Function to load spoofVendingSdk config
async function loadSpoofVendingSdkConfig() {
    const sdkVendingToggle = document.getElementById('toggle-sdk-vending');
    const isChecked = await execCommand(`grep -o '"spoofVendingSdk": [01]' /data/adb/modules/playintegrityfix/pif.json | cut -d' ' -f2`);
    if (isChecked === '0') {
        sdkVendingToggle.checked = false;
    } else {
        sdkVendingToggle.checked = true;
    }
}

// Function to append element in output terminal
function appendToOutput(content) {
    const output = document.querySelector('.output-terminal-content');
    if (content.trim() === "") {
        const lineBreak = document.createElement('br');
        output.appendChild(lineBreak);
    } else {
        const line = document.createElement('p');
        line.className = 'output-content';
        line.textContent = content;
        output.appendChild(line);
    }
    output.scrollTop = output.scrollHeight;
}

// Function to run the script and display its output
async function runAction() {
    if (actionRunning) return;
    actionRunning = true;
    try {
        appendToOutput("[+] Fetching pif.json...");
        await new Promise(resolve => setTimeout(resolve, 200));
        const scriptOutput = await execCommand("sh /data/adb/modules/playintegrityfix/action.sh");
        const lines = scriptOutput.split('\n');
        lines.forEach(line => {
            appendToOutput(line)
        });
        appendToOutput("");
    } catch (error) {
        console.error('Script execution failed:', error);
        if (typeof ksu !== 'undefined' && ksu.mmrl) {
            appendToOutput("");
            appendToOutput("[!] Please allow permission in MMRL settings");
            appendToOutput("[-] Settings");
            appendToOutput("[-] Security");
            appendToOutput("[-] Allow JavaScript API");
            appendToOutput("[-] Play Integrity Fix");
            appendToOutput("[-] Enable Allow Advanced KernelSU API");
            appendToOutput("");
        } else {
            appendToOutput("[!] Error: Fail to execute action.sh");
            appendToOutput("");
        }
    }
    actionRunning = false;
}

// Function to apply ripple effect
function applyRippleEffect() {
    document.querySelectorAll('.ripple-element').forEach(element => {
        if (element.dataset.rippleListener !== "true") {
            element.addEventListener("pointerdown", function (event) {
                const ripple = document.createElement("span");
                ripple.classList.add("ripple");

                // Calculate ripple size and position
                const rect = element.getBoundingClientRect();
                const width = rect.width;
                const size = Math.max(rect.width, rect.height);
                const x = event.clientX - rect.left - size / 2;
                const y = event.clientY - rect.top - size / 2;

                // Determine animation duration
                let duration = 0.2 + (width / 800) * 0.4;
                duration = Math.min(0.8, Math.max(0.2, duration));

                // Set ripple styles
                ripple.style.width = ripple.style.height = `${size}px`;
                ripple.style.left = `${x}px`;
                ripple.style.top = `${y}px`;
                ripple.style.animationDuration = `${duration}s`;
                ripple.style.transition = `opacity ${duration}s ease`;

                // Adaptive color
                const computedStyle = window.getComputedStyle(element);
                const bgColor = computedStyle.backgroundColor || "rgba(0, 0, 0, 0)";
                const textColor = computedStyle.color;
                const isDarkColor = (color) => {
                    const rgb = color.match(/\d+/g);
                    if (!rgb) return false;
                    const [r, g, b] = rgb.map(Number);
                    return (r * 0.299 + g * 0.587 + b * 0.114) < 96; // Luma formula
                };
                ripple.style.backgroundColor = isDarkColor(bgColor) ? "rgba(255, 255, 255, 0.2)" : "";

                // Append ripple and handle cleanup
                element.appendChild(ripple);
                const handlePointerUp = () => {
                    ripple.classList.add("end");
                    setTimeout(() => {
                        ripple.classList.remove("end");
                        ripple.remove();
                    }, duration * 1000);
                    element.removeEventListener("pointerup", handlePointerUp);
                    element.removeEventListener("pointercancel", handlePointerUp);
                };
                element.addEventListener("pointerup", handlePointerUp);
                element.addEventListener("pointercancel", handlePointerUp);
            });
            element.dataset.rippleListener = "true";
        }
    });
}

// Function to check if running in MMRL
async function checkMMRL() {
    if (typeof ksu !== 'undefined' && ksu.mmrl) {
        // Set status bars theme based on device theme
        try {
            $playintegrityfix.setLightStatusBars(!window.matchMedia('(prefers-color-scheme: dark)').matches)
        } catch (error) {
            console.log("Error setting status bars theme:", error)
        }

        // Request API permission, supported version: 33045+
        try {
            $playintegrityfix.requestAdvancedKernelSUAPI();
        } catch (error) {
            console.log("Error requesting API:", error);
        }
        try {
            await execCommand("whoami");
        } catch (error) {
            appendToOutput("");
            appendToOutput("[!] Please allow permission in MMRL settings");
            appendToOutput("[-] Settings");
            appendToOutput("[-] Security");
            appendToOutput("[-] Allow JavaScript API");
            appendToOutput("[-] Play Integrity Fix");
            appendToOutput("[-] Enable Allow Advanced KernelSU API");
            appendToOutput("");
        }
    } else {
        console.log("Not running in MMRL environment.");
    }
}

function getDistance(touch1, touch2) {
    return Math.hypot(
        touch1.clientX - touch2.clientX,
        touch1.clientY - touch2.clientY
    );
}

function updateFontSize(newSize) {
    currentFontSize = Math.min(Math.max(newSize, MIN_FONT_SIZE), MAX_FONT_SIZE);
    const terminal = document.querySelector('.output-terminal-content');
    terminal.style.fontSize = `${currentFontSize}px`;
}

document.addEventListener('DOMContentLoaded', async () => {
    checkMMRL();
    loadVersionFromModuleProp();
    loadSpoofVendingSdkConfig();
    applyButtonEventListeners();
    applyRippleEffect();
});