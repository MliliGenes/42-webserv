const clock = document.getElementById('clock');
const message = document.getElementById('message');

function pad(value) {
    return String(value).padStart(2, '0');
}

function tick() {
    const now = new Date();
    clock.textContent = [pad(now.getHours()), pad(now.getMinutes()), pad(now.getSeconds())].join(':');
}

async function loadCgiPreview() {
    try {
        const response = await fetch('/cgi-bin/hello.py', { cache: 'no-store' });
        const html = await response.text();
        message.textContent = response.ok
            ? `Python CGI responded with ${response.status}. Preview length: ${html.length} characters.`
            : `Python CGI returned ${response.status}.`;
    } catch (error) {
        message.textContent = `Failed to contact CGI endpoint: ${error.message}`;
    }
}

tick();
loadCgiPreview();
setInterval(tick, 1000);