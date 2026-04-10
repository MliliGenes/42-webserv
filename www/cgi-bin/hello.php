<?php
header('Content-Type: text/html; charset=UTF-8');
echo "<!DOCTYPE html>\n";
echo "<html lang=\"en\">\n";
echo "<head><meta charset=\"UTF-8\"><title>PHP CGI</title></head>\n";
echo "<body style=\"font-family: sans-serif; background:#0b1020; color:#eaf0ff; padding:40px;\">";
echo "<h1>PHP CGI works</h1>";
echo "<p>Generated at " . date('c') . "</p>";
echo "<p>Request URI: " . htmlspecialchars($_SERVER['REQUEST_URI'] ?? '', ENT_QUOTES, 'UTF-8') . "</p>";
echo "</body></html>";