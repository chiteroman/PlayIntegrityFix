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

// Function to run the script and display its output
async function runAction() {
    const output = document.querySelector('.output');
    try {
        const scriptOutput = await execCommand("sh /data/adb/modules/playintegrityfix/action.sh");
        output.innerHTML = '';
        const lines = scriptOutput.split('\n');
        lines.forEach(line => {
            const lineElement = document.createElement('div');
            lineElement.style.whiteSpace = 'pre-wrap';
            lineElement.textContent = line;
            if (line === '') {
                lineElement.innerHTML = '&nbsp;';
            }
            output.appendChild(lineElement);
        });
    } catch (error) {
        console.error('Script execution failed:', error);
        if (typeof ksu !== 'undefined' && ksu.mmrl) {
            output.innerHTML = '[!] Please allow permission in MMRL settings<br><br>[-] Settings<br>[-] Security<br>[-] Allow JavaScript API<br>[-] Play Integrity Fix<br>[-] Enable Allow Advanced KernelSU API';
        } else {
            output.innerHTML = '[!] Error: Fail to execute action.sh';
        }
    }
}

// Function to check if running in MMRL
function checkMMRL() {
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
    } else {
        console.log("Not running in MMRL environment.");
    }
}

document.addEventListener('DOMContentLoaded', async () => {
    checkMMRL();
    setTimeout(runAction, 200);
});