import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
EXE_PATH = REPO_ROOT / "cmake-build-debug" / "LSP" / "ryntls.exe"

def frame(message):
    body = json.dumps(message).encode("utf-8")
    return b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body

def read_message(stream):
    headers = {}
    while True:
        line = stream.readline()
        if not line:
            return None
        line = line.rstrip(b"\r\n")
        if line == b"":
            break
        if b":" in line:
            key, value = line.split(b":", 1)
            headers[key.strip().lower()] = value.strip()

    length = int(headers[b"content-length"])
    return json.loads(stream.read(length).decode("utf-8"))

class ServerSession:
    def __init__(self):
        self.process = subprocess.Popen(
            [str(EXE_PATH)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def send(self, message):
        self.process.stdin.write(frame(message))
        self.process.stdin.flush()

    def request(self, message):
        self.send(message)
        return read_message(self.process.stdout)

    def wait(self):
        try:
            self.process.stdin.close()
        except OSError:
            pass
        return self.process.wait(timeout=5)

    def stderr_text(self):
        return self.process.stderr.read().decode("utf-8", errors="replace")

def initialize_request(request_id=1):
    return {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": "initialize",
        "params": {
            "processId": None,
            "clientInfo": {"name": "lsp-test", "version": "1.0"},
            "rootUri": None,
            "capabilities": {},
        },
    }

def test_initialize_shutdown_exit():
    server = ServerSession()

    response = server.request(initialize_request())
    assert response is not None, "no response to initialize"
    assert response["id"] == 1, "initialize response id mismatch"
    assert "capabilities" in response["result"], "missing capabilities"
    assert response["result"]["serverInfo"]["name"] == "Ryntra Language Server"

    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    response = server.request({"jsonrpc": "2.0", "id": 2, "method": "shutdown"})
    assert response is not None and response["id"] == 2, "no response to shutdown"
    assert response["result"] is None, "shutdown result must be null"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    assert server.wait() == 0, "exit after shutdown must return 0"

def test_request_before_initialize():
    server = ServerSession()

    response = server.request({"jsonrpc": "2.0", "id": 1, "method": "shutdown"})
    assert response is not None, "no response to request before initialize"
    assert response["error"]["code"] == -32002, "expected ServerNotInitialized"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()

def test_unknown_method():
    server = ServerSession()

    server.request(initialize_request())
    response = server.request({"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover", "params": {}})
    assert response is not None, "no response to unknown method"
    assert response["error"]["code"] == -32601, "expected MethodNotFound"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()

def test_exit_without_shutdown():
    server = ServerSession()

    server.send({"jsonrpc": "2.0", "method": "exit"})
    assert server.wait() == 1, "exit without shutdown must return 1"


def test_document_lifecycle():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///test.rynt"
    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {"textDocument": {"uri": uri, "languageId": "ryntra", "version": 1, "text": "int a = 1\n"}},
    })
    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": "int a = 2\n"}]},
    })
    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {"uri": uri, "version": 3},
            "contentChanges": [{
                "range": {"start": {"line": 0, "character": 6}, "end": {"line": 0, "character": 7}},
                "text": "b",
            }],
        },
    })
    server.send({"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {"textDocument": {"uri": uri}}})

    response = server.request({"jsonrpc": "2.0", "id": 2, "method": "shutdown"})
    assert response["result"] is None, "shutdown after document events failed"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    assert server.wait() == 0, "exit after document lifecycle must return 0"

    errors = server.stderr_text()
    assert "Invalid notification" not in errors, errors
    assert "unopened document" not in errors, errors


def test_change_without_open():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {"textDocument": {"uri": "file:///missing.rynt", "version": 1}, "contentChanges": [{"text": "int a = 1\n"}]},
    })
    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()

    assert "unopened document" in server.stderr_text()


def test_malformed_did_change():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {"textDocument": {"uri": "file:///bad.rynt", "version": 1}},
    })

    response = server.request({"jsonrpc": "2.0", "id": 2, "method": "shutdown"})
    assert response["result"] is None, "server did not survive a malformed notification"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    assert server.wait() == 0

    assert "Invalid notification" in server.stderr_text()


TESTS = [
    ("initialize/shutdown/exit", test_initialize_shutdown_exit),
    ("request before initialize", test_request_before_initialize),
    ("unknown method", test_unknown_method),
    ("exit without shutdown", test_exit_without_shutdown),
    ("document lifecycle", test_document_lifecycle),
    ("change without open", test_change_without_open),
    ("malformed didChange", test_malformed_did_change),
]

def main():
    if not EXE_PATH.exists():
        print(f"Server not found: {EXE_PATH}")
        print("Build the 'RyntraLSP' target first.")
        sys.exit(1)

    passed = 0
    for name, test in TESTS:
        try:
            test()
            print(f"Pass: {name}")
            passed += 1
        except Exception as error:
            print(f"Fail: {name} -> {error}")

    print("\n---- Summary ----")
    print(f"Passed: {passed} / {len(TESTS)}")

    if passed != len(TESTS):
        sys.exit(1)

if __name__ == "__main__":
    main()
