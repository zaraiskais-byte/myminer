import http.server
import socketserver

TARGET = "https://czr-node.taila0a431.ts.net"

class Handler(http.server.BaseHTTPRequestHandler):
    def _redirect(self):
        self.send_response(302)
        self.send_header("Location", TARGET)
        self.send_header("Content-Length", "0")
        self.end_headers()
    do_GET = _redirect
    do_POST = _redirect
    do_HEAD = _redirect
    def log_message(self, fmt, *args):
        pass

if __name__ == "__main__":
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", 38543), Handler) as srv:
        print("listening 127.0.0.1:38543 -> " + TARGET)
        srv.serve_forever()
