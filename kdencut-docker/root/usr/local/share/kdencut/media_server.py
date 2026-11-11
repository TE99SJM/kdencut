"""Browser uploads and read-only SMB imports for the local KdenCut session.

Nginx exposes the loopback service under the editor's existing login. Every API
request requires a non-simple header, and no cross-origin permissions are granted.
Only completed files are published to the native editor through atomic JSON IPC.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from contextlib import closing
from pathlib import Path, PurePosixPath
from urllib.parse import parse_qs, urlsplit
import json
import os
import re
import shutil
import threading
import time
import uuid

MEDIA_EXTENSIONS = set("mp4 mov m4v mkv avi webm mpg mpeg mts m2ts ts mxf vob ogv 3gp mp3 wav flac aac m4a ogg opus aiff aif wma jpg jpeg png webp gif bmp tif tiff heic heif avif svg".split())
MAX_FILE = 100 * 1024**3


def atomic_json(path, value):
    """Publish a complete private document without exposing a partially written file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        with open(temporary, "x", encoding="utf-8") as stream:
            os.chmod(temporary, 0o600)
            json.dump(value, stream)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def relative_path(value):
    value = str(value)
    if "\\" in value or ":" in value or "\x00" in value or value.startswith("/") or ".." in value.split("/"):
        raise ValueError("Invalid folder path")
    return str(PurePosixPath(value)) if value else ""


def media_name(value):
    if not isinstance(value, str) or not value or len(value.encode("utf-8")) > 240:
        raise ValueError("Invalid filename")
    if any(char in value for char in '/\\:\x00') or value.startswith("."):
        raise ValueError("Invalid filename")
    if value.rsplit(".", 1)[-1].lower() not in MEDIA_EXTENSIONS:
        raise ValueError("Choose a video, photo, or audio file")
    return value


class MediaStore:
    def __init__(self, bridge, imports, roots):
        self.bridge = Path(bridge)
        self.imports = Path(imports)
        self.roots = {key: Path(value).resolve() for key, value in roots.items()}
        self.bridge.mkdir(parents=True, exist_ok=True)
        self.imports.mkdir(parents=True, exist_ok=True)
        os.chmod(self.bridge, 0o700)
        self.lock = threading.RLock()
        self.files = {}
        self.jobs = {}
        self.connections_file = self.bridge / "connections.json"
        self.connections = json.loads(self.connections_file.read_text()) if self.connections_file.exists() else {}

    def request(self):
        try:
            request = json.loads((self.bridge / "request.json").read_text())
            uuid.UUID(request["id"])
            if time.time() * 1000 - request["created"] > 24 * 3600 * 1000:
                return None
            return request
        except (OSError, ValueError, KeyError, TypeError):
            return None

    def active(self, request_id):
        request = self.request()
        if not request or request["id"] != request_id or (self.bridge / (request_id + ".json")).exists():
            raise ValueError("This import is closed. Use Add Media again.")

    def status(self):
        request = self.request()
        with self.lock:
            # Cancelled native dialogs can disappear without an HTTP cancel.
            for token, item in list(self.files.items()):
                if not request or item["request"] != request["id"]:
                    shutil.rmtree(Path(item["path"]).parent, ignore_errors=True)
                    del self.files[token]
            for key in list(self.jobs):
                if not request or key != request["id"]:
                    del self.jobs[key]
            job = dict(self.jobs.get(request["id"], {})) if request else {}
        if request and (self.bridge / (request["id"] + ".json")).exists():
            job["state"] = "done"
        return {"request": request, "job": job}

    def sources(self):
        with self.lock:
            connections = [{k: v for k, v in connection.items() if k != "password"}
                           for connection in self.connections.values()]
        return {"roots": [{"id": key, "name": "Imported media" if key == "imports" else "Server media", "path": str(path)}
                          for key, path in self.roots.items() if path.is_dir()],
                "connections": connections}

    def connect(self, fields):
        server = str(fields.get("server", "")).strip()
        share = str(fields.get("share", "")).strip()
        port = int(fields.get("port") or 445)
        if not 1 <= port <= 65535:
            raise ValueError("Enter a port between 1 and 65535")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,252}", server):
            raise ValueError("Enter a NAS hostname or IPv4 address without smb://")
        if not share or len(share) > 200 or any(c in share for c in '/\\:\x00') or share in (".", ".."):
            raise ValueError("Enter one share name, such as MNAS")
        connection = {"id": uuid.uuid4().hex, "name": str(fields.get("name") or share)[:100],
                      "server": server, "share": share, "port": port, "username": str(fields.get("username", ""))[:256],
                      "password": str(fields.get("password", ""))[:1024]}
        self.smb_list(connection, "")  # Verify credentials/share before saving.
        with self.lock:
            self.connections[connection["id"]] = connection
            atomic_json(self.connections_file, self.connections)
        return {"id": connection["id"]}

    def forget(self, connection_id):
        with self.lock:
            self.connections.pop(connection_id, None)
            atomic_json(self.connections_file, self.connections)

    def smb(self, connection):
        import smbclient
        cache = {}
        smbclient.register_session(connection["server"], username=connection["username"] or None,
                                   password=connection["password"] or None, connection_timeout=15,
                                   port=connection.get("port", 445), connection_cache=cache)
        return smbclient, cache

    @staticmethod
    def unc(connection, path):
        path = relative_path(path)
        return "\\\\" + connection["server"] + "\\" + connection["share"] + ("\\" + path.replace("/", "\\") if path and path != "." else "")

    def smb_list(self, connection, path):
        smb, cache = self.smb(connection)
        try:
            result = []
            with closing(smb.scandir(self.unc(connection, path), port=connection.get("port", 445), connection_cache=cache)) as entries:
                for entry in entries:
                    if entry.is_symlink():
                        continue
                    directory = entry.is_dir(follow_symlinks=False)
                    if directory or entry.name.rsplit(".", 1)[-1].lower() in MEDIA_EXTENSIONS:
                        result.append({"name": entry.name, "directory": directory,
                                       "size": 0 if directory else smb.stat(entry.path, follow_symlinks=False,
                                           port=connection.get("port", 445), connection_cache=cache).st_size})
                    if len(result) >= 5000:
                        raise ValueError("This folder is too large. Choose a more specific NAS share.")
            return sorted(result, key=lambda row: (not row["directory"], row["name"].lower()))
        finally:
            smb.reset_connection_cache(connection_cache=cache)

    def local_path(self, source, path):
        root = self.roots[source]
        result = (root / relative_path(path)).resolve()
        if result != root and root not in result.parents:
            raise ValueError("That path is outside the selected storage")
        return result

    def listing(self, source, path):
        path = relative_path(path)
        if source in self.connections:
            return self.smb_list(self.connections[source], path)
        folder = self.local_path(source, path)
        rows = []
        for item in folder.iterdir():
            if item.is_symlink() or item.name.startswith("."):
                continue
            directory = item.is_dir()
            if directory or item.suffix.lstrip(".").lower() in MEDIA_EXTENSIONS:
                rows.append({"name": item.name, "directory": directory, "size": 0 if directory else item.stat().st_size})
            if len(rows) >= 5000:
                raise ValueError("This folder is too large. Use the server file picker to browse it.")
        return sorted(rows, key=lambda row: (not row["directory"], row["name"].lower()))

    def allocate(self, request_id, name):
        self.active(request_id)
        name = media_name(name)
        token = uuid.uuid4().hex
        folder = self.imports / token
        folder.mkdir(mode=0o700)
        return token, folder / name

    def receive(self, request_id, name, size, stream):
        if size <= 0 or size > MAX_FILE:
            raise ValueError("File size must be between 1 byte and 100 GB")
        if shutil.disk_usage(self.imports).free < size + 64 * 1024**2:
            raise ValueError("Not enough free server storage for this file")
        token, destination = self.allocate(request_id, name)
        partial = destination.with_name(destination.name + ".part")
        try:
            with open(partial, "xb") as output:
                remaining = size
                while remaining:
                    self.active(request_id)
                    chunk = stream.read(min(1024**2, remaining))
                    if not chunk:
                        raise ValueError("Transfer interrupted. Please try again.")
                    output.write(chunk)
                    remaining -= len(chunk)
                output.flush()
                os.fsync(output.fileno())
            self.active(request_id)
            os.replace(partial, destination)
            with self.lock:
                self.files[token] = {"request": request_id, "path": str(destination)}
            return token
        except Exception:
            shutil.rmtree(destination.parent, ignore_errors=True)
            raise

    def finish(self, request_id, tokens):
        self.active(request_id)
        if not isinstance(tokens, list) or not tokens or len(tokens) > 500:
            raise ValueError("Select at least one media file")
        with self.lock:
            files = [self.files[token] for token in tokens]
            if any(item["request"] != request_id for item in files):
                raise ValueError("Files belong to another import")
            paths = [item["path"] for item in files]
            atomic_json(self.bridge / (request_id + ".json"), {"id": request_id, "status": "ready", "paths": paths})
            for token, item in list(self.files.items()):
                if item["request"] == request_id:
                    if token not in tokens:
                        shutil.rmtree(Path(item["path"]).parent, ignore_errors=True)
                    del self.files[token]

    def cancel(self, request_id, status="cancelled"):
        self.active(request_id)
        atomic_json(self.bridge / (request_id + ".json"), {"id": request_id, "status": status})
        with self.lock:
            for token, item in list(self.files.items()):
                if item["request"] == request_id:
                    shutil.rmtree(Path(item["path"]).parent, ignore_errors=True)
                    del self.files[token]

    def select(self, request_id, source, paths):
        self.active(request_id)
        if not isinstance(paths, list) or not paths or len(paths) > 500:
            raise ValueError("Choose between 1 and 500 files")
        paths = [relative_path(path) for path in paths]
        if source not in self.connections:
            files = [self.local_path(source, path) for path in paths]
            for file in files:
                media_name(file.name)
                if not file.is_file():
                    raise ValueError("Choose files, not folders")
            self.active(request_id)
            atomic_json(self.bridge / (request_id + ".json"), {"id": request_id, "status": "ready", "paths": [str(p) for p in files]})
            return
        with self.lock:
            if self.jobs.get(request_id, {}).get("state") == "copying":
                raise ValueError("A download is already in progress")
            self.jobs[request_id] = {"state": "copying", "done": 0, "total": 0, "file": "Connecting to NAS"}
        connection = dict(self.connections[source])
        threading.Thread(target=self.copy_nas, args=(request_id, connection, paths), daemon=True).start()

    def copy_nas(self, request_id, connection, paths):
        smb = None
        cache = None
        tokens = []
        try:
            smb, cache = self.smb(connection)
            total = sum(smb.stat(self.unc(connection, path), port=connection.get("port", 445), connection_cache=cache).st_size for path in paths)
            if shutil.disk_usage(self.imports).free < total + 64 * 1024**2:
                raise ValueError("Not enough free server storage for these files")
            done = 0
            for path in paths:
                self.active(request_id)
                remote = self.unc(connection, path)
                size = smb.stat(remote, port=connection.get("port", 445), connection_cache=cache).st_size
                name = PurePosixPath(path).name
                store = self

                class ProgressReader:
                    def read(self, count):
                        nonlocal done
                        chunk = stream.read(count)
                        done += len(chunk)
                        with store.lock:
                            store.jobs[request_id] = {"state": "copying", "done": done, "total": total, "file": name}
                        return chunk

                with smb.open_file(remote, mode="rb", port=connection.get("port", 445), connection_cache=cache) as stream:
                    tokens.append(self.receive(request_id, name, size, ProgressReader()))
            self.finish(request_id, tokens)
            with self.lock:
                self.jobs[request_id] = {"state": "done"}
        except Exception as error:
            # A failed batch never imports a partial selection. Completed members
            # of that failed batch are removed; the NAS originals are untouched.
            with self.lock:
                for token in tokens:
                    item = self.files.pop(token, None)
                    if item:
                        shutil.rmtree(Path(item["path"]).parent, ignore_errors=True)
                self.jobs[request_id] = {"state": "error", "error": safe_error(error)}
        finally:
            if smb is not None:
                smb.reset_connection_cache(connection_cache=cache)


def safe_error(error):
    if isinstance(error, ValueError):
        return str(error)
    if isinstance(error, PermissionError):
        return "Access denied. Check the NAS username, password, and share permissions."
    if isinstance(error, FileNotFoundError):
        return "The file or share could not be found."
    if isinstance(error, KeyError):
        return "The selected storage or file is no longer available."
    return "Could not access storage. Check the NAS address, share name, credentials, and network connection."


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass  # Filenames and NAS credentials must not enter access logs.

    def json_response(self, value, status=200):
        payload = json.dumps(value).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def route(self, method):
        try:
            if self.headers.get("X-KdenCut-Media") != "1" or self.headers.get("Sec-Fetch-Site") == "cross-site":
                self.json_response({"error": "Use the KdenCut media chooser"}, 403)
                return
            origin = self.headers.get("Origin")
            if origin and urlsplit(origin).netloc != self.headers.get("Host"):
                self.json_response({"error": "Invalid origin"}, 403)
                return
            parsed = urlsplit(self.path)
            query = {key: values[0] for key, values in parse_qs(parsed.query).items()}
            store = self.server.store
            if method == "GET" and parsed.path == "/status":
                result = store.status()
            elif method == "GET" and parsed.path == "/sources":
                result = store.sources()
            elif method == "GET" and parsed.path == "/list":
                result = {"entries": store.listing(query["source"], query.get("path", ""))}
            elif method == "PUT" and parsed.path == "/upload":
                self.connection.settimeout(120)
                result = {"token": store.receive(query["request"], query["name"], int(self.headers.get("Content-Length", "0")), self.rfile)}
            elif method == "POST":
                size = int(self.headers.get("Content-Length", "0"))
                if not 0 < size <= 128 * 1024:
                    raise ValueError("Invalid request size")
                body = json.loads(self.rfile.read(size))
                if parsed.path == "/connections":
                    result = store.connect(body)
                elif parsed.path == "/forget":
                    store.forget(body["id"])
                    result = {}
                elif parsed.path == "/finish":
                    store.finish(body["request"], body["files"])
                    result = {}
                elif parsed.path in ("/cancel", "/server"):
                    store.cancel(body["request"], "server" if parsed.path == "/server" else "cancelled")
                    result = {}
                elif parsed.path == "/select":
                    store.select(body["request"], body["source"], body["paths"])
                    result = {}
                else:
                    self.json_response({"error": "Not found"}, 404)
                    return
            else:
                self.json_response({"error": "Not found"}, 404)
                return
            self.json_response(result)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as error:
            self.close_connection = True
            self.json_response({"error": safe_error(error)}, 400)

    def do_GET(self):
        self.route("GET")

    def do_POST(self):
        self.route("POST")

    def do_PUT(self):
        self.route("PUT")


def make_server(store, port=8766):
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    server.store = store
    return server


if __name__ == "__main__":
    os.umask(0o077)
    imports = Path("/config/KdenCut/Imports")
    store = MediaStore(os.environ.get("KDENCUT_MEDIA_BRIDGE", "/config/.local/share/kdencut/media-bridge"), imports,
                       {"media": "/media", "imports": imports})
    make_server(store).serve_forever()
