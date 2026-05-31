#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import time
from hashlib import sha1
from html import unescape
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, quote, unquote, urljoin, urlparse, urlunparse
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "_inventory"
PAGES = ROOT / "pages"
ASSETS = ROOT / "assets"
LIVE_BASE = "https://www.datoolset.net"
LIVE_HOSTS = {"datoolset.net", "www.datoolset.net"}
USER_AGENT = "Haven-Tools live wiki mirror/1.0"

PAGE_WORKERS = 10
ASSET_WORKERS = 10
PLACEHOLDER_PNG = (
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAFgwJ/"
    "l6pQGQAAAABJRU5ErkJggg=="
)
PLACEHOLDER_ASSETS = (
    "/favicon.ico",
    "/mw/resources/assets/poweredby_mediawiki_88x31.png",
    "/mw/resources/assets/poweredby_mediawiki_132x47.png",
    "/mw/resources/assets/poweredby_mediawiki_176x62.png",
    "/mw/resources/assets/file-type-icons/fileicon.png",
    "/mw/resources/assets/file-type-icons/fileicon-pdf.png",
)


def log(message: str) -> None:
    print(message, flush=True)


def read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def clean_output() -> None:
    for target in (PAGES, ASSETS):
        if target.exists():
            shutil.rmtree(target)
        target.mkdir(parents=True, exist_ok=True)
    for name in (
        "live_download_manifest.json",
        "live_download_errors.json",
        "live_asset_manifest.json",
        "live_asset_errors.json",
        "live_discovered_assets.json",
        "live_rewrite_unresolved.json",
        "live_verification_summary.json",
    ):
        path = INVENTORY / name
        if path.exists():
            path.unlink()


def fetch_bytes(url: str, retries: int = 3, timeout: int = 45) -> tuple[int, str, str, bytes]:
    last_error: Exception | None = None
    for attempt in range(1, retries + 1):
        try:
            request = Request(url, headers={"User-Agent": USER_AGENT})
            with urlopen(request, timeout=timeout) as response:
                return response.status, response.geturl(), response.headers.get("Content-Type", ""), response.read()
        except HTTPError as exc:
            last_error = exc
            if exc.code in (429, 500, 502, 503, 504) and attempt < retries:
                time.sleep(1.5 * attempt)
                continue
            raise
        except (URLError, TimeoutError, ConnectionError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(1.5 * attempt)
                continue
            raise
    raise RuntimeError(f"Unable to fetch {url}: {last_error}")


def decode_text(content_type: str, data: bytes) -> str:
    charset = "utf-8"
    match = re.search(r"charset=([^;\s]+)", content_type, re.I)
    if match:
        charset = match.group(1)
    return data.decode(charset, errors="replace")


def safe_segment(segment: str) -> str:
    encoded = quote(segment, safe="")
    while encoded.endswith("."):
        encoded = encoded[:-1] + "%2E"
    while encoded.endswith(" "):
        encoded = encoded[:-1] + "%20"
    if len(encoded) <= 120:
        return encoded or "_"
    digest = sha1(encoded.encode("utf-8")).hexdigest()[:12]
    return f"{encoded[:90]}-{digest}"


def title_to_path(title: str) -> Path:
    title = (title or "Main_Page").replace(" ", "_").strip("/")
    segments = [safe_segment(part) for part in title.split("/") if part]
    if not segments:
        segments = ["Main_Page"]
    return PAGES.joinpath(*segments[:-1], f"{segments[-1]}.html")


def title_to_live_url(title: str) -> str:
    title = (title or "Main_Page").replace(" ", "_").strip("/")
    return f"{LIVE_BASE}/wiki/{quote(title, safe='/:%')}"


def is_live_url(url: str) -> bool:
    parsed = urlparse(url)
    host = parsed.netloc.lower()
    if host.endswith(":80") or host.endswith(":443"):
        host = host.rsplit(":", 1)[0]
    return host in LIVE_HOSTS


def normalize_live_url(url: str, base: str | None = None) -> str | None:
    url = unescape(url.strip())
    if not url or url.startswith(("#", "mailto:", "javascript:", "data:")):
        return None
    absolute = urljoin(base or LIVE_BASE, url)
    parsed = urlparse(absolute)
    if not is_live_url(absolute):
        return None
    host = parsed.netloc.lower()
    if host.endswith(":80") or host.endswith(":443"):
        host = host.rsplit(":", 1)[0]
    if host == "www.datoolset.net":
        host = "datoolset.net"
    path = re.sub(r"/+", "/", unquote(parsed.path))
    query = unescape(parsed.query)
    if path.startswith("/wiki/"):
        path = "/wiki/" + path[len("/wiki/") :].replace(" ", "_")
        query = ""
    return urlunparse(("http", host, path, "", query, ""))


def fetchable_live_url(url: str) -> str:
    normalized = normalize_live_url(url) or url
    parsed = urlparse(normalized)
    path = quote(unquote(parsed.path), safe="/!$&'()*+,;=:@")
    query = quote(unquote(parsed.query), safe="=&,|:*+-._~%")
    return urlunparse(("https", "www.datoolset.net", path, "", query, ""))


def url_to_title(url: str, base: str | None = None) -> str | None:
    normalized = normalize_live_url(url, base=base)
    if not normalized:
        return None
    parsed = urlparse(normalized)
    path = parsed.path
    if path in ("/", "/wiki", "/wiki/"):
        return "Main_Page"
    if path.startswith("/wiki/"):
        return path[len("/wiki/") :]
    if path.endswith("/mw/index.php"):
        title = parse_qs(parsed.query).get("title", [None])[0]
        if title:
            return title.replace(" ", "_")
    return None


def asset_path_for_url(url: str) -> Path:
    normalized = normalize_live_url(url) or url
    parsed = urlparse(normalized)
    path = re.sub(r"/+", "/", unquote(parsed.path))
    for prefix, base in (
        ("/mw/images/", ASSETS / "mw" / "images"),
        ("/mw/resources/", ASSETS / "mw" / "resources"),
        ("/mw/skins/", ASSETS / "mw" / "skins"),
    ):
        if path.startswith(prefix):
            parts = [safe_segment(part) for part in path[len(prefix) :].split("/") if part]
            return base.joinpath(*parts)
    if path.endswith("/mw/load.php"):
        ext = ".bin"
        if "only=styles" in parsed.query:
            ext = ".css"
        elif "only=scripts" in parsed.query:
            ext = ".js"
        digest = sha1(normalized.encode("utf-8")).hexdigest()[:16]
        return ASSETS / "mw" / "load" / f"load-{digest}{ext}"
    if path == "/favicon.ico":
        return ASSETS / "favicon.ico"
    digest = sha1(normalized.encode("utf-8")).hexdigest()[:16]
    suffix = Path(path).suffix or ".bin"
    return ASSETS / "misc" / f"{safe_segment(Path(path).name or 'asset')}-{digest}{suffix}"


def is_asset_url(url: str, base: str | None = None) -> bool:
    normalized = normalize_live_url(url, base=base)
    if not normalized:
        return False
    path = urlparse(normalized).path
    return path.startswith(("/mw/images/", "/mw/resources/", "/mw/skins/")) or path.endswith("/mw/load.php") or path == "/favicon.ico"


def extract_assets(html: str, page_url: str) -> set[str]:
    found: set[str] = set()
    for match in re.finditer(r'\b(?:src|href)="([^"]+)"', html, re.I):
        value = match.group(1)
        if is_asset_url(value, base=page_url):
            normalized = normalize_live_url(value, base=page_url)
            if normalized:
                found.add(normalized)
    for match in re.finditer(r'\bsrcset="([^"]+)"', html, re.I):
        for candidate in match.group(1).split(","):
            url = candidate.strip().split(" ")[0]
            if is_asset_url(url, base=page_url):
                normalized = normalize_live_url(url, base=page_url)
                if normalized:
                    found.add(normalized)
    return found


def download_page(page: dict[str, object]) -> tuple[dict[str, object] | None, list[str], dict[str, object] | None]:
    title = str(page["title"])
    url = str(page.get("live_url") or title_to_live_url(title))
    local_path = title_to_path(title)
    try:
        status, final_url, content_type, data = fetch_bytes(url, timeout=60)
        html = decode_text(content_type, data)
        local_path.parent.mkdir(parents=True, exist_ok=True)
        local_path.write_text(html, encoding="utf-8")
        return (
            {
                "title": title,
                "url": url,
                "final_url": final_url,
                "status": status,
                "content_type": content_type,
                "local_path": str(local_path.relative_to(ROOT)).replace("\\", "/"),
                "bytes": len(data),
            },
            sorted(extract_assets(html, url)),
            None,
        )
    except Exception as exc:
        return None, [], {"title": title, "url": url, "error": repr(exc)}


def download_pages() -> set[str]:
    live_pages = read_json(INVENTORY / "live_pages.json")
    manifest: list[dict[str, object]] = []
    errors: list[dict[str, object]] = []
    assets: set[str] = set()
    total = len(live_pages)  # type: ignore[arg-type]
    log(f"Downloading {total} live wiki pages...")
    with concurrent.futures.ThreadPoolExecutor(max_workers=PAGE_WORKERS) as executor:
        futures = [executor.submit(download_page, page) for page in live_pages]  # type: ignore[arg-type]
        for index, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            record, discovered, error = future.result()
            if record:
                manifest.append(record)
                assets.update(discovered)
            if error:
                errors.append(error)
            if index % 100 == 0 or index == total:
                log(f"  pages {index}/{total}; ok={len(manifest)} errors={len(errors)} assets={len(assets)}")
                write_json(INVENTORY / "live_download_manifest.json", manifest)
                write_json(INVENTORY / "live_download_errors.json", errors)
                write_json(INVENTORY / "live_discovered_assets.json", sorted(assets))
    write_json(INVENTORY / "live_download_manifest.json", manifest)
    write_json(INVENTORY / "live_download_errors.json", errors)
    write_json(INVENTORY / "live_discovered_assets.json", sorted(assets))
    return assets


def download_asset(url: str) -> tuple[dict[str, object] | None, dict[str, object] | None]:
    local_path = asset_path_for_url(url)
    fetch_url = fetchable_live_url(url)
    temp_path = local_path.with_name(local_path.name + ".part")
    try:
        local_path.parent.mkdir(parents=True, exist_ok=True)
        if temp_path.exists():
            temp_path.unlink()
        result = subprocess.run(
            [
                "curl.exe",
                "--silent",
                "--show-error",
                "--location",
                "--fail",
                "--max-time",
                "25",
                "--connect-timeout",
                "8",
                "--retry",
                "1",
                "--retry-delay",
                "1",
                "--user-agent",
                USER_AGENT,
                "--output",
                str(temp_path),
                fetch_url,
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            timeout=35,
        )
        if result.returncode != 0:
            if temp_path.exists():
                temp_path.unlink()
            raise RuntimeError(result.stderr.strip() or f"curl exited {result.returncode}")
        temp_path.replace(local_path)
        return (
            {
                "url": url,
                "final_url": fetch_url,
                "status": 200,
                "content_type": "",
                "local_path": str(local_path.relative_to(ROOT)).replace("\\", "/"),
                "bytes": local_path.stat().st_size,
            },
            None,
        )
    except Exception as exc:
        return None, {"url": url, "local_path": str(local_path.relative_to(ROOT)).replace("\\", "/"), "error": repr(exc)}


def download_assets(discovered: set[str]) -> None:
    live_media = read_json(INVENTORY / "live_media.json")
    asset_urls = set(discovered)
    for item in live_media:  # type: ignore[assignment]
        asset_urls.add(normalize_live_url(str(item["live_url"])) or str(item["live_url"]))
    existing_manifest = INVENTORY / "live_asset_manifest.json"
    manifest_by_url: dict[str, dict[str, object]] = {}
    if existing_manifest.exists():
        for item in read_json(existing_manifest):  # type: ignore[assignment]
            if isinstance(item, dict) and item.get("url"):
                manifest_by_url[str(item["url"])] = item
    pending: list[str] = []
    for url in sorted(asset_urls):
        local_path = asset_path_for_url(url)
        if local_path.exists() and local_path.stat().st_size > 0:
            manifest_by_url.setdefault(
                url,
                {
                    "url": url,
                    "final_url": fetchable_live_url(url),
                    "status": "existing",
                    "content_type": "",
                    "local_path": str(local_path.relative_to(ROOT)).replace("\\", "/"),
                    "bytes": local_path.stat().st_size,
                },
            )
        else:
            pending.append(url)
    total = len(pending)
    manifest: list[dict[str, object]] = list(manifest_by_url.values())
    errors: list[dict[str, object]] = []
    log(f"Downloading {total} remaining live assets/images ({len(manifest)} already present)...")
    with concurrent.futures.ThreadPoolExecutor(max_workers=ASSET_WORKERS) as executor:
        futures = [executor.submit(download_asset, url) for url in pending]
        for index, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            record, error = future.result()
            if record:
                manifest_by_url[str(record["url"])] = record
                manifest = list(manifest_by_url.values())
            if error:
                errors.append(error)
            if index % 100 == 0 or index == total:
                log(f"  assets {index}/{total}; ok={len(manifest)} errors={len(errors)}")
                write_json(INVENTORY / "live_asset_manifest.json", manifest)
                write_json(INVENTORY / "live_asset_errors.json", errors)
    write_json(INVENTORY / "live_asset_manifest.json", manifest)
    write_json(INVENTORY / "live_asset_errors.json", errors)


def write_fallback_css() -> Path:
    css_path = ASSETS / "local" / "wiki-static.css"
    css_path.parent.mkdir(parents=True, exist_ok=True)
    css_path.write_text(
        """
body {
  margin: 0;
  color: #202122;
  background: #f8f9fa;
  font: 14px/1.55 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
a { color: #0645ad; text-decoration: none; }
a:hover { text-decoration: underline; }
#content, .mw-body {
  max-width: 1180px;
  margin: 0 auto;
  padding: 24px 28px;
  background: #fff;
  border-left: 1px solid #a7d7f9;
  border-right: 1px solid #a7d7f9;
  min-height: 100vh;
}
#mw-panel, #p-personal, #p-search, #footer { max-width: 1180px; margin: 0 auto; }
h1, h2, h3 { font-family: Georgia, "Times New Roman", serif; font-weight: 400; }
h1 { font-size: 1.9rem; border-bottom: 1px solid #a2a9b1; }
h2 { border-bottom: 1px solid #a2a9b1; }
img { max-width: 100%; height: auto; }
table { border-collapse: collapse; }
td, th { border: 1px solid #a2a9b1; padding: 0.25rem 0.45rem; }
pre, code { background: #f6f6f6; border: 1px solid #ddd; border-radius: 3px; }
pre { padding: 0.8rem; overflow: auto; }
.thumb { margin: 0.5rem 1rem 1rem 0; }
.thumbinner { border: 1px solid #c8ccd1; background: #f8f9fa; padding: 3px; }
.tright { float: right; clear: right; margin-left: 1rem; }
.tleft { float: left; clear: left; margin-right: 1rem; }
.toc { border: 1px solid #a2a9b1; background: #f8f9fa; padding: 0.75rem; display: inline-block; }
""".strip()
        + "\n",
        encoding="utf-8",
    )
    return css_path


def write_placeholder_assets() -> None:
    data = base64.b64decode(PLACEHOLDER_PNG)
    for url_path in PLACEHOLDER_ASSETS:
        url = normalize_live_url(url_path)
        if not url:
            continue
        path = asset_path_for_url(url)
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)


def page_index() -> dict[str, Path]:
    live_pages = read_json(INVENTORY / "live_pages.json")
    index: dict[str, Path] = {}
    for page in live_pages:  # type: ignore[assignment]
        title = str(page["title"]).replace(" ", "_")
        path = title_to_path(title)
        index[title.lower()] = path
    index["main_page"] = title_to_path("Main_Page")
    return index


def asset_index() -> dict[str, Path]:
    manifest_path = INVENTORY / "live_asset_manifest.json"
    index: dict[str, Path] = {}
    if not manifest_path.exists():
        return index
    for item in read_json(manifest_path):  # type: ignore[assignment]
        normalized = normalize_live_url(str(item["url"]))
        if normalized:
            index[normalized] = ROOT / str(item["local_path"])
    for url_path in PLACEHOLDER_ASSETS:
        normalized = normalize_live_url(url_path)
        if normalized:
            path = asset_path_for_url(normalized)
            if path.exists():
                index[normalized] = path
    return index


def original_for_thumbnail(url: str) -> str | None:
    normalized = normalize_live_url(url)
    if not normalized:
        return None
    parsed = urlparse(normalized)
    prefix = "/mw/images/thumb/"
    if not parsed.path.startswith(prefix):
        return None
    parts = parsed.path[len(prefix) :].split("/")
    if len(parts) < 4:
        return None
    if parts[0] == "archive" and len(parts) >= 5:
        first, second, archived_name = parts[1], parts[2], parts[3]
        filename = archived_name.split("!", 1)[-1]
    else:
        first, second, filename = parts[0], parts[1], parts[2]
    return normalize_live_url(f"{LIVE_BASE}/mw/images/{first}/{second}/{filename}")


def rel(target: Path, source: Path) -> str:
    return os.path.relpath(target, source.parent).replace("\\", "/")


def rewrite_url(value: str, source_file: Path, pages: dict[str, Path], assets: dict[str, Path]) -> str | None:
    anchor = ""
    if "#" in value:
        value, fragment = value.split("#", 1)
        anchor = "#" + fragment
    normalized = normalize_live_url(value, base=LIVE_BASE)
    if not normalized:
        return None
    asset = assets.get(normalized)
    if asset and asset.exists():
        return rel(asset, source_file) + anchor
    original = original_for_thumbnail(normalized)
    if original:
        original_asset = assets.get(original)
        if original_asset and original_asset.exists():
            return rel(original_asset, source_file) + anchor
    title = url_to_title(value, base=LIVE_BASE)
    if title:
        page = pages.get(title.lower())
        if page and page.exists():
            return rel(page, source_file) + anchor
    return None


def rewrite_srcset(value: str, source_file: Path, pages: dict[str, Path], assets: dict[str, Path]) -> str:
    rewritten: list[str] = []
    for candidate in value.split(","):
        pieces = candidate.strip().split()
        if pieces:
            replacement = rewrite_url(pieces[0], source_file, pages, assets)
            if replacement:
                pieces[0] = replacement
        rewritten.append(" ".join(pieces))
    return ", ".join(rewritten)


def rewrite_html() -> None:
    write_placeholder_assets()
    fallback_css = write_fallback_css()
    pages = page_index()
    assets = asset_index()
    unresolved: dict[str, int] = {}
    rewritten = 0
    html_files = list(PAGES.rglob("*.html"))
    attr_re = re.compile(r'(?P<attr>href|src)="(?P<url>[^"]*)"', re.I)
    srcset_re = re.compile(r'srcset="(?P<srcset>[^"]*)"', re.I)
    for html_file in html_files:
        html = html_file.read_text(encoding="utf-8", errors="replace")
        original = html
        # Drop MediaWiki loader tags that failed to download; local fallback CSS handles layout.
        html = re.sub(r'<script[^>]+src="[^"]*/mw/load\.php[^"]*"[^>]*></script>\s*', "", html, flags=re.I)
        html = re.sub(
            r'<link[^>]+href="[^"]*/mw/load\.php[^"]*"[^>]*>\s*',
            lambda match: match.group(0)
            if rewrite_url(re.search(r'href="([^"]+)"', match.group(0), re.I).group(1), html_file, pages, assets)  # type: ignore[union-attr]
            else "",
            html,
            flags=re.I,
        )

        def replace_attr(match: re.Match[str]) -> str:
            replacement = rewrite_url(match.group("url"), html_file, pages, assets)
            if replacement:
                return f'{match.group("attr")}="{replacement}"'
            normalized = normalize_live_url(match.group("url"), base=LIVE_BASE)
            if normalized and urlparse(normalized).path.startswith(("/wiki/", "/mw/")):
                unresolved[normalized] = unresolved.get(normalized, 0) + 1
            return match.group(0)

        html = attr_re.sub(replace_attr, html)
        html = srcset_re.sub(lambda match: f'srcset="{rewrite_srcset(match.group("srcset"), html_file, pages, assets)}"', html)
        fallback_href = rel(fallback_css, html_file)
        if fallback_href not in html:
            html = html.replace("</head>", f'<link rel="stylesheet" href="{fallback_href}" />\n</head>', 1)
        if html != original:
            html_file.write_text(html, encoding="utf-8")
            rewritten += 1
    write_json(
        INVENTORY / "live_rewrite_unresolved.json",
        [{"url": url, "count": count} for url, count in sorted(unresolved.items(), key=lambda item: (-item[1], item[0]))],
    )
    log(f"Rewrote {rewritten} HTML files; {len(unresolved)} unresolved internal live URLs remain.")


def verify() -> None:
    page_files = list(PAGES.rglob("*.html"))
    asset_files = [path for path in ASSETS.rglob("*") if path.is_file()]
    image_files = [path for path in (ASSETS / "mw" / "images").rglob("*") if path.is_file()] if (ASSETS / "mw" / "images").exists() else []
    page_errors = read_json(INVENTORY / "live_download_errors.json") if (INVENTORY / "live_download_errors.json").exists() else []
    asset_errors = read_json(INVENTORY / "live_asset_errors.json") if (INVENTORY / "live_asset_errors.json").exists() else []
    internal_live_refs = 0
    missing_local_images = 0
    for html_file in page_files:
        html = html_file.read_text(encoding="utf-8", errors="ignore")
        internal_live_refs += len(re.findall(r'https?://(?:www\.)?datoolset\.net/(?:wiki|mw)/', html))
        for match in re.finditer(r'<img[^>]+src="([^"]+)"', html, re.I):
            src = match.group(1)
            if src.startswith(("http://", "https://", "/")):
                missing_local_images += 1
                continue
            target = (html_file.parent / src.split("#", 1)[0]).resolve()
            if not target.exists():
                missing_local_images += 1
    summary = {
        "pages_on_disk": len(page_files),
        "assets_on_disk": len(asset_files),
        "images_on_disk": len(image_files),
        "page_download_errors": len(page_errors),  # type: ignore[arg-type]
        "asset_download_errors": len(asset_errors),  # type: ignore[arg-type]
        "internal_live_refs_remaining": internal_live_refs,
        "missing_local_img_srcs": missing_local_images,
        "main_page": str((PAGES / "Main_Page.html").relative_to(ROOT)).replace("\\", "/"),
        "main_page_exists": (PAGES / "Main_Page.html").exists(),
    }
    write_json(INVENTORY / "live_verification_summary.json", summary)
    log(json.dumps(summary, indent=2))


def main() -> int:
    parser = argparse.ArgumentParser(description="Mirror the live Dragon Age Toolset Wiki as static files.")
    parser.add_argument("command", choices=("all", "pages", "assets", "rewrite", "verify", "clean"))
    parser.add_argument("--clean", action="store_true", help="Clean pages/assets before running.")
    args = parser.parse_args()

    if args.clean or args.command == "clean":
        clean_output()
        if args.command == "clean":
            return 0

    if args.command in ("all", "pages"):
        discovered = download_pages()
    else:
        discovered_path = INVENTORY / "live_discovered_assets.json"
        discovered = set(read_json(discovered_path)) if discovered_path.exists() else set()
    if args.command in ("all", "assets"):
        download_assets(discovered)
    if args.command in ("all", "rewrite"):
        rewrite_html()
    if args.command in ("all", "verify"):
        verify()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
