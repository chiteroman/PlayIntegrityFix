let shellRunning = false;
let initialPinchDistance = null;
let currentFontSize = 14;
const MIN_FONT_SIZE = 8;
const MAX_FONT_SIZE = 24;

/**
 * Executes a shell command with KernelSU privileges
 * @param {string} command - The shell command to execute
 * @returns {Promise<string>} A promise that resolves with stdout content
 * @throws {Error} If command execution fails with:
 *   - Non-zero exit code (includes stderr in error message)
 */
function exec(command) {
    return new Promise((resolve, reject) => {
        const callbackFuncName = `exec_callback_${Date.now()}`;
        window[callbackFuncName] = (errno, stdout, stderr) => {
            delete window[callbackFuncName];
            if (errno !== 0) {
                reject(new Error(`Command failed with exit code ${errno}: ${stderr}`));
                return;
            }
            resolve(stdout);
        };
        try {
            ksu.exec(command, "{}", callbackFuncName);
        } catch (error) {
            delete window[callbackFuncName];
            reject(error);
        }
    });
}

/**
 * Spawns shell process with ksu spawn
 * @param {string} command - The command to execute
 * @param {string[]} [args=[]] - Array of arguments to pass to the command
 * @returns {Object} A child process object with:
 *   - stdout: Stream for standard output
 *   - stderr: Stream for standard error
 *   - stdin: Stream for standard input
 *   - on(event, listener): Attach event listener ('exit', 'error')
 *   - emit(event, ...args): Emit events internally
 */
function spawn(command, args = []) {
    const child = {
        listeners: {},
        stdout: { listeners: {} },
        stderr: { listeners: {} },
        stdin: { listeners: {} },
        on: function(event, listener) {
            if (!this.listeners[event]) this.listeners[event] = [];
            this.listeners[event].push(listener);
        },
        emit: function(event, ...args) {
            if (this.listeners[event]) {
                this.listeners[event].forEach(listener => listener(...args));
            }
        }
    };
    ['stdout', 'stderr', 'stdin'].forEach(io => {
        child[io].on = child.on.bind(child[io]);
        child[io].emit = child.emit.bind(child[io]);
    });
    const callbackName = `spawn_callback_${Date.now()}`;
    window[callbackName] = child;
    child.on("exit", () => delete window[callbackName]);
    try {
        ksu.spawn(command, JSON.stringify(args), "{}", callbackName);
    } catch (error) {
        child.emit("error", error);
        delete window[callbackName];
    }
    return child;
}

// Apply button event listeners
function applyButtonEventListeners() {
    const fetchButton = document.getElementById('fetch');
    const previewFpToggle = document.getElementById('preview-fp-toggle-container');
    const clearButton = document.querySelector('.clear-terminal');
    const terminal = document.querySelector('.output-terminal-content');

    fetchButton.addEventListener('click', runAction);
    previewFpToggle.addEventListener('click', async () => {
        if (shellRunning) return;
        shellRunning = true;
        try {
            const isChecked = document.getElementById('toggle-preview-fp').checked;
            await exec(`sed -i 's/^FORCE_PREVIEW=.*$/FORCE_PREVIEW=${isChecked ? 0 : 1}/' /data/adb/modules/playintegrityfix/action.sh`);
            appendToOutput(`[+] Switched fingerprint to ${isChecked ? 'beta' : 'preview'}`);
            loadPreviewFingerprintConfig();
        } catch (error) {
            appendToOutput("[!] Failed to switch fingerprint type");
            console.error('Failed to switch fingerprint type:', error);
        }
        shellRunning = false;
    });

    clearButton.addEventListener('click', () => {
        terminal.innerHTML = '';
        currentFontSize = 14;
        updateFontSize(currentFontSize);
    });
    
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
        const version = await exec("grep '^version=' /data/adb/modules/playintegrityfix/module.prop | cut -d'=' -f2");
        versionElement.textContent = version.trim();
    } catch (error) {
        appendToOutput("[!] Failed to read version from module.prop");
        console.error("Failed to read version from module.prop:", error);
    }
}

// Function to load preview fingerprint config
async function loadPreviewFingerprintConfig() {
    try {
        const previewFpToggle = document.getElementById('toggle-preview-fp');
        const isChecked = await exec(`grep -o 'FORCE_PREVIEW=[01]' /data/adb/modules/playintegrityfix/action.sh | cut -d'=' -f2`);
        if (isChecked === '0') {
            previewFpToggle.checked = false;
        } else {
            previewFpToggle.checked = true;
        }
    } catch (error) {
        appendToOutput("[!] Failed to load preview fingerprint config");
        console.error("Failed to load preview fingerprint config:", error);
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
        line.innerHTML = content.replace(/ /g, '&nbsp;');
        output.appendChild(line);
    }
    output.scrollTop = output.scrollHeight;
}

// Function to run the script and display its output
function runAction() {
    if (shellRunning) return;
    shellRunning = true;
    const scriptOutput = spawn("sh", ["/data/adb/modules/playintegrityfix/action.sh"]);
    scriptOutput.stdout.on('data', (data) => appendToOutput(data));
    scriptOutput.stderr.on('data', (data) => appendToOutput(data));
    scriptOutput.on('exit', () => {
        appendToOutput("");
        shellRunning = false;
    });
    scriptOutput.on('error', () => {
        appendToOutput("[!] Error: Fail to execute action.sh");
        appendToOutput("");
        shellRunning = false;
    });
}

/**
 * Simulate MD3 ripple animation
 * Usage: class="ripple-element" style="position: relative; overflow: hidden;"
 * Note: Require background-color to work properly
 * @return {void}
 */
function applyRippleEffect() {
    document.querySelectorAll('.ripple-element').forEach(element => {
        if (element.dataset.rippleListener !== "true") {
            element.addEventListener("pointerdown", async (event) => {
                // Pointer up event
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
                const isDarkColor = (color) => {
                    const rgb = color.match(/\d+/g);
                    if (!rgb) return false;
                    const [r, g, b] = rgb.map(Number);
                    return (r * 0.299 + g * 0.587 + b * 0.114) < 96; // Luma formula
                };
                ripple.style.backgroundColor = isDarkColor(bgColor) ? "rgba(255, 255, 255, 0.2)" : "";

                // Append ripple
                element.appendChild(ripple);
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
    loadPreviewFingerprintConfig();
    applyButtonEventListeners();
    applyRippleEffect();
});