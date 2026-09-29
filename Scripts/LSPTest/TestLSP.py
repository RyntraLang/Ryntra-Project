import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_EXE = REPO_ROOT / "cmake-build-debug" / "LSP" / "ryntra-lsp.exe"
EXE_PATH = Path(os.environ.get("RYNTRA_LSP_EXE", DEFAULT_EXE))

VALID_SOURCE = 'public void main() {\n    __builtin_print("Hello World");\n}\n'
INVALID_SOURCE = "public void main() {\n"
NO_MAIN_SOURCE = 'public void greet() {\n    __builtin_print("hi");\n}\n'
NO_MAIN_WITH_ERROR_SOURCE = "public void greet() {\n    __builtin_print(missing);\n}\n"
SYMBOL_SOURCE = (
    "public void greet() {\n"
    '    __builtin_print("hi");\n'
    "}\n"
    "public void main() {\n"
    "    int value = 42;\n"
    "    __builtin_print(value);\n"
    "    greet();\n"
    "}\n"
)
STRUCT_SOURCE = (
    "public struct Rectangle {\n"
    "    public int width;\n"
    "    public int height;\n"
    "\n"
    "    public int getArea() {\n"
    "        return self.width * self.height;\n"
    "    }\n"
    "}\n"
    "\n"
    "public void main() {\n"
    "    Rectangle rect = Rectangle(100, 200);\n"
    "    __builtin_print(rect.getArea());\n"
    "    rect.width = 5;\n"
    "}\n"
)
CONSTRUCTOR_SOURCE = (
    "public struct Rectangle {\n"
    "    public int width;\n"
    "    public int height;\n"
    "\n"
    "    public Rectangle(int width, int height) {\n"
    "        self.width = width;\n"
    "    }\n"
    "}\n"
    "\n"
    "public void main() {\n"
    "    Rectangle rect = Rectangle(100, 200);\n"
    "    __builtin_print(rect.width);\n"
    "}\n"
)

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

    def read(self):
        return read_message(self.process.stdout)

    def request(self, message):
        self.send(message)
        request_id = message.get("id")

        while True:
            incoming = self.read()
            if incoming is None:
                raise AssertionError(f"No response for request id {request_id}")
            if incoming.get("id") == request_id:
                return incoming

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
    response = server.request({"jsonrpc": "2.0", "id": 2, "method": "textDocument/references", "params": {}})
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


def did_open(uri, text, version=1):
    return {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {"textDocument": {"uri": uri, "languageId": "ryntra", "version": version, "text": text}},
    }


def request_message(request_id, method, params):
    return {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params}


def position_params(uri, line, character):
    return {"textDocument": {"uri": uri}, "position": {"line": line, "character": character}}


def error_diagnostics(notification):
    assert notification["method"] == "textDocument/publishDiagnostics", notification
    return [diagnostic for diagnostic in notification["params"]["diagnostics"] if diagnostic["severity"] == 1]


def test_diagnostics_for_invalid_source():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    server.send(did_open("file:///invalid.rynt", INVALID_SOURCE))
    notification = server.read()
    assert notification["params"]["uri"] == "file:///invalid.rynt"
    assert error_diagnostics(notification), "expected at least one error diagnostic"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_diagnostics_clean_for_valid_source():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    server.send(did_open("file:///valid.rynt", VALID_SOURCE))
    notification = server.read()
    assert not error_diagnostics(notification), error_diagnostics(notification)

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_diagnostics_update_on_change():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///change.rynt"
    server.send(did_open(uri, VALID_SOURCE))
    assert not error_diagnostics(server.read())

    server.send({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": INVALID_SOURCE}]},
    })
    assert error_diagnostics(server.read()), "expected errors after breaking the document"

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_diagnostics_cleared_on_close():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///close.rynt"
    server.send(did_open(uri, INVALID_SOURCE))
    assert error_diagnostics(server.read())

    server.send({"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {"textDocument": {"uri": uri}}})
    notification = server.read()
    assert notification["method"] == "textDocument/publishDiagnostics"
    assert notification["params"]["diagnostics"] == []

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_hover_and_definition():
    server = ServerSession()

    initialization = server.request(initialize_request())
    capabilities = initialization["result"]["capabilities"]
    assert capabilities.get("hoverProvider") is True, capabilities
    assert capabilities.get("definitionProvider") is True, capabilities

    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///symbols.rynt"
    server.send(did_open(uri, SYMBOL_SOURCE))

    hover = server.request(request_message(10, "textDocument/hover", position_params(uri, 5, 22)))
    value = hover["result"]["contents"]["value"]
    assert "value" in value and "int" in value, value

    definition = server.request(request_message(11, "textDocument/definition", position_params(uri, 5, 22)))
    declared = definition["result"]["range"]
    assert declared["start"]["line"] == 4, declared
    assert declared["start"]["character"] <= 8 <= declared["end"]["character"], declared

    function_hover = server.request(request_message(12, "textDocument/hover", position_params(uri, 6, 5)))
    assert "greet" in function_hover["result"]["contents"]["value"]

    function_definition = server.request(request_message(13, "textDocument/definition", position_params(uri, 6, 5)))
    assert function_definition["result"]["range"]["start"]["line"] == 0, function_definition

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_struct_member_hover_and_definition():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///struct.rynt"
    server.send(did_open(uri, STRUCT_SOURCE))

    member_hover = server.request(request_message(20, "textDocument/hover", position_params(uri, 12, 10)))
    member_text = member_hover["result"]["contents"]["value"]
    assert "width" in member_text and "int" in member_text, member_text

    member_definition = server.request(request_message(21, "textDocument/definition", position_params(uri, 12, 10)))
    assert member_definition["result"]["range"]["start"]["line"] == 1, member_definition

    method_hover = server.request(request_message(22, "textDocument/hover", position_params(uri, 11, 26)))
    method_text = method_hover["result"]["contents"]["value"]
    assert "getArea" in method_text, method_text

    method_definition = server.request(request_message(23, "textDocument/definition", position_params(uri, 11, 26)))
    assert method_definition["result"]["range"]["start"]["line"] == 4, method_definition

    self_definition = server.request(request_message(24, "textDocument/definition", position_params(uri, 5, 21)))
    assert self_definition["result"]["range"]["start"]["line"] == 1, self_definition

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_constructor_and_parameter_symbols():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    uri = "file:///constructor.rynt"
    server.send(did_open(uri, CONSTRUCTOR_SOURCE))

    call_hover = server.request(request_message(30, "textDocument/hover", position_params(uri, 10, 22)))
    call_text = call_hover["result"]["contents"]["value"]
    assert "(constructor)" in call_text, call_text

    call_definition = server.request(request_message(31, "textDocument/definition", position_params(uri, 10, 22)))
    assert call_definition["result"]["range"]["start"]["line"] == 4, call_definition

    declaration_hover = server.request(request_message(32, "textDocument/hover", position_params(uri, 4, 12)))
    assert "(constructor)" in declaration_hover["result"]["contents"]["value"], declaration_hover

    parameter_hover = server.request(request_message(33, "textDocument/hover", position_params(uri, 4, 26)))
    parameter_text = parameter_hover["result"]["contents"]["value"]
    assert "(parameter)" in parameter_text, parameter_text

    parameter_definition = server.request(request_message(34, "textDocument/definition", position_params(uri, 5, 22)))
    assert parameter_definition["result"]["range"]["start"]["line"] == 4, parameter_definition

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


def test_editor_mode_suppresses_main_check_only():
    server = ServerSession()

    server.request(initialize_request())
    server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    clean_uri = "file:///no-main.rynt"
    server.send(did_open(clean_uri, NO_MAIN_SOURCE))
    clean = server.read()
    assert not error_diagnostics(clean), error_diagnostics(clean)

    error_uri = "file:///no-main-error.rynt"
    server.send(did_open(error_uri, NO_MAIN_WITH_ERROR_SOURCE))
    broken = server.read()
    messages = [diagnostic["message"] for diagnostic in error_diagnostics(broken)]
    assert any("RCE014" in message for message in messages), messages

    server.send({"jsonrpc": "2.0", "method": "exit"})
    server.wait()


TESTS = [
    ("initialize/shutdown/exit", test_initialize_shutdown_exit),
    ("request before initialize", test_request_before_initialize),
    ("unknown method", test_unknown_method),
    ("exit without shutdown", test_exit_without_shutdown),
    ("document lifecycle", test_document_lifecycle),
    ("change without open", test_change_without_open),
    ("malformed didChange", test_malformed_did_change),
    ("diagnostics for invalid source", test_diagnostics_for_invalid_source),
    ("diagnostics clean for valid source", test_diagnostics_clean_for_valid_source),
    ("diagnostics update on change", test_diagnostics_update_on_change),
    ("diagnostics cleared on close", test_diagnostics_cleared_on_close),
    ("hover and definition", test_hover_and_definition),
    ("struct member hover and definition", test_struct_member_hover_and_definition),
    ("constructor and parameter symbols", test_constructor_and_parameter_symbols),
    ("editor mode suppresses main check only", test_editor_mode_suppresses_main_check_only),
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
