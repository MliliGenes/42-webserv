function testGet() {
    fetch("/cgi-bin/echo.py")
        .then(res => res.text())
        .then(data => document.getElementById("result").innerHTML = data);
}

function testPost() {
    fetch("/cgi-bin/echo.py", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: "name=webserv&test=ok"
    })
    .then(res => res.text())
    .then(data => document.getElementById("result").innerHTML = data);
}