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
        output.innerHTML = '[!] Error: Fail to execute action.sh';
        console.error('Script execution failed:', error);
    }
}

document.addEventListener('DOMContentLoaded', async () => {
    setTimeout(runAction, 200);
});