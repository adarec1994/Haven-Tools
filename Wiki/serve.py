#!/usr/bin/env python3
from __future__ import annotations

import argparse
import mimetypes
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "_tools"))

import mirror_live  # noqa: E402


class WikiHandler(BaseHTTPRequestHandler):
    server_version = "HavenWiki/1.0"

    def do_HEAD(self) -> None:
        self._serve(send_body=False)

    def do_GET(self) -> None:
        self._serve(send_body=True)

    def log_message(self, format: str, *args: object) -> None:
        sys.stderr.write("%s - - [%s] %s\n" % (self.client_address[0], self.log_date_time_string(), format % args))

    def _serve(self, send_body: bool) -> None:
        path = self._resolve_path()
        if not path or not path.exists() or not path.is_file():
            self.send_error(404, "Not found")
            return
        try:
            self._send_file(path, send_body)
        except OSError:
            self.send_error(500, "Unable to read file")

    def _resolve_path(self) -> Path | None:
        raw_path = urlsplit(self.path).path or "/"
        if raw_path in ("/", "/index.html"):
            return ROOT / "index.html"
        if raw_path == "/favicon.ico":
            mirror_live.write_placeholder_assets()
            favicon = ROOT / "assets" / "favicon.ico"
            if favicon.exists():
                return favicon
            return ROOT / "assets" / "mw" / "resources" / "assets" / "poweredby_mediawiki_88x31.png"
        if raw_path.startswith("/wiki/"):
            title = raw_path[len("/wiki/") :] or "Main_Page"
            if title.endswith(".html"):
                title = title[:-5]
            page = mirror_live.title_to_path(title)
            if page.exists():
                return page
            return self._safe_literal_path("pages/" + title + ".html")
        if raw_path.startswith("/mw/"):
            normalized = mirror_live.normalize_live_url(raw_path)
            if normalized:
                asset = mirror_live.asset_path_for_url(normalized)
                if asset.exists():
                    return asset
                original = mirror_live.original_for_thumbnail(normalized)
                if original:
                    original_asset = mirror_live.asset_path_for_url(original)
                    if original_asset.exists():
                        return original_asset
            return None
        return self._safe_literal_path(raw_path.lstrip("/"))

    def _safe_literal_path(self, relative: str) -> Path | None:
        if "\\" in relative:
            return None
        parts = [part for part in relative.split("/") if part]
        if any(part in (".", "..") for part in parts):
            return None
        candidate = (ROOT.joinpath(*parts)).resolve()
        try:
            candidate.relative_to(ROOT)
        except ValueError:
            return None
        return candidate

    def _send_file(self, path: Path, send_body: bool) -> None:
        content = path.read_bytes()
        content_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        if path.suffix.lower() in (".html", ".css", ".js", ".txt", ".json", ".md"):
            content_type += "; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if send_body:
            self.wfile.write(content)


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve the local Dragon Age Toolset Wiki mirror.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    mirror_live.write_placeholder_assets()
    httpd = ThreadingHTTPServer((args.host, args.port), WikiHandler)
    print(f"Serving {ROOT} at http://{args.host}:{httpd.server_port}/", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
