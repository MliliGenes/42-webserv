// ── Utility ──────────────────────────────────────────────────────────────────

function fmtSize(bytes) {
    if (bytes < 1024)       return bytes + ' B';
    if (bytes < 1048576)    return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
}

function setResult(el, text, isError) {
    el.textContent = text;
    el.className = 'result' + (isError ? ' error' : '');
}

// ── Upload page ───────────────────────────────────────────────────────────────

function initUploadPage() {
    var dropZone   = document.getElementById('drop-zone');
    var fileInput  = document.getElementById('file-input');
    var fileList   = document.getElementById('file-list');
    var uploadBtn  = document.getElementById('upload-btn');
    var result     = document.getElementById('upload-result');
    var gallery    = document.getElementById('gallery');

    if (!dropZone) return;

    var pendingFiles = [];

    // drag-and-drop
    dropZone.addEventListener('dragover', function(e) {
        e.preventDefault();
        dropZone.classList.add('dragover');
    });
    dropZone.addEventListener('dragleave', function() {
        dropZone.classList.remove('dragover');
    });
    dropZone.addEventListener('drop', function(e) {
        e.preventDefault();
        dropZone.classList.remove('dragover');
        addFiles(e.dataTransfer.files);
    });
    dropZone.addEventListener('click', function() { fileInput.click(); });
    fileInput.addEventListener('change', function() { addFiles(fileInput.files); });

    function addFiles(files) {
        for (var i = 0; i < files.length; i++) {
            pendingFiles.push(files[i]);
        }
        renderFileList();
    }

    function renderFileList() {
        fileList.innerHTML = '';
        pendingFiles.forEach(function(f, idx) {
            var div = document.createElement('div');
            div.className = 'file-item';
            div.innerHTML =
                '<span class="name">' + f.name + '</span>' +
                '<span class="size">' + fmtSize(f.size) + '</span>' +
                '<button class="del-btn" data-idx="' + idx + '">✕</button>';
            fileList.appendChild(div);
        });
        fileList.querySelectorAll('.del-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                pendingFiles.splice(parseInt(btn.dataset.idx), 1);
                renderFileList();
            });
        });
    }

    // upload all pending files one by one
    uploadBtn.addEventListener('click', function() {
        if (pendingFiles.length === 0) {
            setResult(result, 'No files selected.', true);
            return;
        }
        var log = [];
        var done = 0;

        pendingFiles.forEach(function(file) {
            var fd = new FormData();
            fd.append('file', file, file.name);

            fetch('/uploads', { method: 'POST', body: fd })
                .then(function(r) { return r.text().then(function(t) { return { ok: r.ok, status: r.status, text: t }; }); })
                .then(function(r) {
                    log.push((r.ok ? '✓' : '✗') + ' ' + file.name + ' → ' + r.status);
                    if (r.ok && isImage(file.name)) addPreview(file);
                    if (r.ok && isVideo(file.name)) addVideoPreview(file);
                    done++;
                    if (done === pendingFiles.length) {
                        setResult(result, log.join('\n'));
                        pendingFiles = [];
                        renderFileList();
                        refreshUploaded();
                    }
                })
                .catch(function(e) {
                    log.push('✗ ' + file.name + ' → ' + e.message);
                    done++;
                    if (done === pendingFiles.length) setResult(result, log.join('\n'), true);
                });
        });
    });

    function isImage(name) { return /\.(jpg|jpeg|png|gif|webp|svg)$/i.test(name); }
    function isVideo(name) { return /\.(mp4|webm|ogg|mov)$/i.test(name); }

    function addPreview(file) {
        var img = document.createElement('img');
        img.className = 'preview';
        img.src = URL.createObjectURL(file);
        img.alt = file.name;
        gallery.appendChild(img);
    }

    function addVideoPreview(file) {
        var vid = document.createElement('video');
        vid.className = 'preview';
        vid.controls = true;
        vid.src = URL.createObjectURL(file);
        gallery.appendChild(vid);
    }

    // show already-uploaded files with delete buttons
    function refreshUploaded() {
        fetch('/uploads')
            .then(function(r) { return r.text(); })
            .then(function(html) {
                var parser = new DOMParser();
                var doc = parser.parseFromString(html, 'text/html');
                var links = doc.querySelectorAll('a');
                var ul = document.getElementById('uploaded-list');
                if (!ul) return;
                ul.innerHTML = '';
                links.forEach(function(a) {
                    var name = a.textContent.trim();
                    if (name === '..') return;
                    var li = document.createElement('div');
                    li.className = 'file-item';
                    li.innerHTML =
                        '<a class="name" href="' + a.href + '" target="_blank">' + name + '</a>' +
                        '<button class="del-btn" data-name="' + name + '">Delete</button>';
                    ul.appendChild(li);
                });
                ul.querySelectorAll('.del-btn').forEach(function(btn) {
                    btn.addEventListener('click', function() {
                        deleteFile(btn.dataset.name);
                    });
                });
            });
    }

    function deleteFile(name) {
        fetch('/uploads/' + name, { method: 'DELETE' })
            .then(function(r) {
                setResult(result, (r.ok ? '✓ Deleted: ' : '✗ Failed: ') + name, !r.ok);
                refreshUploaded();
            });
    }

    refreshUploaded();
}

// ── Form / Echo page ──────────────────────────────────────────────────────────

function initFormPage() {
    var form   = document.getElementById('echo-form');
    var result = document.getElementById('echo-result');
    if (!form) return;

    form.addEventListener('submit', function(e) {
        e.preventDefault();
        var data = new FormData(form);
        var body = new URLSearchParams(data).toString();
        fetch('/cgi-bin/echo.py', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body
        })
        .then(function(r) { return r.text(); })
        .then(function(t) { result.innerHTML = t; })
        .catch(function(e) { setResult(result, 'Error: ' + e.message, true); });
    });
}

// ── Session page ──────────────────────────────────────────────────────────────

function initSessionPage() {
    var btn    = document.getElementById('session-btn');
    var result = document.getElementById('session-result');
    if (!btn) return;

    btn.addEventListener('click', function() {
        fetch('/cgi-bin/session.py', { method: 'GET', credentials: 'include' })
            .then(function(r) { return r.text(); })
            .then(function(t) { result.innerHTML = t; })
            .catch(function(e) { setResult(result, 'Error: ' + e.message, true); });
    });
}

// ── Delete page ───────────────────────────────────────────────────────────────

function initDeletePage() {
    var btn    = document.getElementById('delete-btn');
    var input  = document.getElementById('delete-name');
    var result = document.getElementById('delete-result');
    if (!btn) return;

    btn.addEventListener('click', function() {
        var name = input.value.trim();
        if (!name) { setResult(result, 'Enter a filename.', true); return; }
        fetch('/uploads/' + name, { method: 'DELETE' })
            .then(function(r) {
                return r.text().then(function(t) {
                    return { ok: r.ok, status: r.status, text: t };
                });
            })
            .then(function(r) {
                setResult(result, r.status + ' ' + (r.ok ? '— deleted successfully' : '— ' + r.text), !r.ok);
            })
            .catch(function(e) { setResult(result, 'Error: ' + e.message, true); });
    });
}

// ── Init ──────────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', function() {
    initUploadPage();
    initFormPage();
    initSessionPage();
    initDeletePage();
});
