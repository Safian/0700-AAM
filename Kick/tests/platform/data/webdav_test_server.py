#!/usr/bin/env python3
# Minimal WebDAV test server for WebDavSourceTests: PROPFIND (Depth 0/1), GET with Range, Basic auth (user "kick" / pass "s3cret"),
# same-origin redirect (/redir/... -> /dav/...), cross-origin redirect (/evil -> http://127.0.0.2:1/x).
# Usage: webdav_test_server.py <root-dir> ; prints "PORT <n> PID <pid>" on stdout when ready.
import base64, os, sys, urllib.parse, email.utils
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT = os.path.abspath(sys.argv[1])
AUTH = "Basic " + base64.b64encode(b"kick:s3cret").decode()

class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def _auth(self):
        if self.headers.get("Authorization") != AUTH:
            self.send_response(401); self.send_header("WWW-Authenticate", 'Basic realm="t"'); self.send_header("Content-Length", "0"); self.end_headers(); return False
        return True
    def _fs(self, path):
        p = urllib.parse.unquote(path)
        if not p.startswith("/dav"): return None
        rel = p[len("/dav"):].lstrip("/")
        full = os.path.abspath(os.path.join(ROOT, rel))
        return full if full == ROOT or full.startswith(ROOT + os.sep) else None
    def _entry(self, href, full):
        st = os.stat(full)
        mod = email.utils.formatdate(st.st_mtime, usegmt=True)
        if os.path.isdir(full):
            return f'<D:response><D:href>{urllib.parse.quote(href)}</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype><D:getlastmodified>{mod}</D:getlastmodified></D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>'
        etag = '"%x-%x"' % (int(st.st_mtime), st.st_size)
        return f'<D:response><D:href>{urllib.parse.quote(href)}</D:href><D:propstat><D:prop><D:resourcetype/><D:getcontentlength>{st.st_size}</D:getcontentlength><D:getlastmodified>{mod}</D:getlastmodified><D:getetag>{etag}</D:getetag></D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>'
    def do_PROPFIND(self):
        n = int(self.headers.get("Content-Length", "0")); self.rfile.read(n)
        if not self._auth(): return
        full = self._fs(self.path)
        if not full or not os.path.exists(full): self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers(); return
        href = self.path if self.path.endswith("/") or not os.path.isdir(full) else self.path + "/"
        body = '<?xml version="1.0" encoding="utf-8"?><D:multistatus xmlns:D="DAV:">' + self._entry(urllib.parse.unquote(href), full)
        if os.path.isdir(full) and self.headers.get("Depth", "1") != "0":
            for name in sorted(os.listdir(full)):
                c = os.path.join(full, name)
                h = urllib.parse.unquote(href) + name + ("/" if os.path.isdir(c) else "")
                body += self._entry(h, c)
        body += "</D:multistatus>"; b = body.encode()
        self.send_response(207); self.send_header("Content-Type", "application/xml; charset=utf-8"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.path.startswith("/evil"):
            self.send_response(302); self.send_header("Location", "http://127.0.0.2:1/x"); self.send_header("Content-Length", "0"); self.end_headers(); return
        if self.path.startswith("/redir/"):
            self.send_response(302); self.send_header("Location", "/dav/" + self.path[len("/redir/"):]); self.send_header("Content-Length", "0"); self.end_headers(); return
        if not self._auth(): return
        full = self._fs(self.path)
        if not full or not os.path.isfile(full): self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers(); return
        data = open(full, "rb").read(); total = len(data)
        rng = self.headers.get("Range")
        if rng and rng.startswith("bytes=") and os.environ.get("IGNORE_RANGE") != "1":
            start = int(rng[6:].split("-")[0])
            if start >= total:
                self.send_response(416); self.send_header("Content-Range", f"bytes */{total}"); self.send_header("Content-Length", "0"); self.end_headers(); return
            chunk = data[start:]
            self.send_response(206); self.send_header("Content-Range", f"bytes {start}-{total-1}/{total}")
        else:
            chunk = data; self.send_response(200)
        self.send_header("Content-Length", str(len(chunk))); self.send_header("Content-Type", "application/octet-stream"); self.end_headers(); self.wfile.write(chunk)

srv = HTTPServer(("127.0.0.1", 0), H)
print("PORT", srv.server_address[1], "PID", os.getpid(), flush=True)
srv.serve_forever()
