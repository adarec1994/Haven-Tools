#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from hashlib import sha1
from html import unescape
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode, urljoin, urlparse, urlunparse
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "_inventory"
PAGES = ROOT / "pages"
ASSETS = ROOT / "assets"

LIVE_BASE = "https://www.datoolset.net"
LIVE_API = f"{LIVE_BASE}/mw/api.php"
ARCHIVE_BASE = "http://www.datoolset.net"
WAYBACK = "https://web.archive.org"

ARCHIVE_YEAR = "2025"
FROM_TS = "20250101000000"
TO_TS = "20251231235959"

USER_AGENT = "Haven-Tools wiki archival mirror/1.0 (+local preservation)"


def log(message: str) -> None:
    print(message, flush=True)


def ensure_dirs() -> None:
    for path in (INVENTORY, PAGES, ASSETS):
        path.mkdir(parents=True, exist_ok=True)


def fetch_bytes(url: str, retries: int = 4, timeout: int = 60) -> tuple[int, str, str, bytes]:
    last_error: Exception | None = None
    for attempt in range(1, retries + 1):
        try:
            request = Request(url, headers={"User-Agent": USER_AGENT})
            with urlopen(request, timeout=timeout) as response:
                data = response.read()
                status = getattr(response, "status", 200)
                content_type = response.headers.get("Content-Type", "")
                return status, response.geturl(), content_type, data
        except HTTPError as exc:
            last_error = exc
            if exc.code in (429, 500, 502, 503, 504) and attempt < retries:
                time.sleep(min(30, 2 * attempt))
                continue
            raise
        except (URLError, TimeoutError, ConnectionError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(min(30, 2 * attempt))
                continue
            raise
    raise RuntimeError(f"Unable to fetch {url}: {last_error}")


def fetch_text(url: str, retries: int = 4, timeout: int = 60) -> tuple[int, str, str, str]:
    status, final_url, content_type, data = fetch_bytes(url, retries=retries, timeout=timeout)
    charset = "utf-8"
    match = re.search(r"charset=([^;\s]+)", content_type, re.I)
    if match:
        charset = match.group(1)
    return status, final_url, content_type, data.decode(charset, errors="replace")


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_namespace_options(html: str) -> list[dict[str, object]]:
    namespaces: list[dict[str, object]] = []
    for match in re.finditer(r'<option value="(?P<id>-?\d+)"[^>]*>(?P<name>[^<]*)</option>', html):
        ns_id = int(match.group("id"))
        if ns_id < 0:
            continue
        namespaces.append({"id": ns_id, "name": unescape(match.group("name")).strip()})
    if namespaces:
        return namespaces
    fallback = [
        (0, "(Main)"),
        (1, "Talk"),
        (2, "User"),
        (3, "User talk"),
        (4, "Dragon Age Toolset Wiki"),
        (5, "Dragon Age Toolset Wiki talk"),
        (6, "File"),
        (7, "File talk"),
        (8, "MediaWiki"),
        (9, "MediaWiki talk"),
        (10, "Template"),
        (11, "Template talk"),
        (12, "Help"),
        (13, "Help talk"),
        (14, "Category"),
        (15, "Category talk"),
        (800, "Portal"),
        (801, "Portal talk"),
        (802, "Community"),
        (803, "Community talk"),
        (804, "Contest"),
        (805, "Contest talk"),
        (806, "Script"),
        (807, "Script talk"),
    ]
    return [{"id": ns_id, "name": name} for ns_id, name in fallback]


def api_get(params: dict[str, object]) -> dict[str, object]:
    query = urlencode(params)
    _status, _final, _ctype, text = fetch_text(f"{LIVE_API}?{query}", timeout=90)
    return json.loads(text)


def title_path(title: str) -> str:
    return quote(title.replace(" ", "_"), safe="/:")


def live_page_url(title: str) -> str:
    return f"{LIVE_BASE}/wiki/{title_path(title)}"


def old_page_url(title: str) -> str:
    return f"{ARCHIVE_BASE}/wiki/{title_path(title)}"


def enumerate_namespace(ns_id: int, ns_name: str) -> list[dict[str, object]]:
    pages: list[dict[str, object]] = []
    apcontinue: str | None = None
    while True:
        params: dict[str, object] = {
            "action": "query",
            "list": "allpages",
            "aplimit": 500,
            "apnamespace": ns_id,
            "format": "json",
        }
        if apcontinue:
            params["apcontinue"] = apcontinue
        data = api_get(params)
        batch = data.get("query", {}).get("allpages", [])  # type: ignore[union-attr]
        for page in batch:
            title = page["title"]
            pages.append(
                {
                    "pageid": page.get("pageid"),
                    "namespace_id": ns_id,
                    "namespace": ns_name,
                    "title": title,
                    "live_url": live_page_url(title),
                    "archive_url": old_page_url(title),
                }
            )
        old_continue = data.get("query-continue", {})  # type: ignore[assignment]
        apcontinue = None
        if isinstance(old_continue, dict):
            apcontinue = old_continue.get("allpages", {}).get("apcontinue")  # type: ignore[union-attr]
        new_continue = data.get("continue", {})  # type: ignore[assignment]
        if isinstance(new_continue, dict):
            apcontinue = new_continue.get("apcontinue", apcontinue)  # type: ignore[assignment]
        if not apcontinue:
            return pages


def scrape_live_media() -> list[dict[str, object]]:
    start = f"{LIVE_BASE}/mw/index.php?title=Special:ListFiles&limit=5000"
    seen_pages: set[str] = set()
    media: dict[str, dict[str, object]] = {}
    url: str | None = start
    while url and url not in seen_pages:
        seen_pages.add(url)
        _status, _final, _ctype, html = fetch_text(url, timeout=120)
        for href in re.findall(r'href="([^"]+)"', html):
            href = unescape(href)
            if href.startswith("/mw/images/"):
                absolute = urljoin(LIVE_BASE, href)
                media[absolute] = {
                    "live_url": absolute,
                    "path": href,
                    "filename": Path(urlparse(href).path).name,
                }
        next_url = None
        for match in re.finditer(r'<a href="(?P<href>[^"]+)">(?P<text>[^<]*Next page[^<]*)</a>', html):
            next_url = urljoin(LIVE_BASE, unescape(match.group("href")))
        url = next_url
    return sorted(media.values(), key=lambda item: str(item["live_url"]))


def inventory_live() -> None:
    ensure_dirs()
    log("Fetching live namespace list...")
    _status, _final, _ctype, allpages_html = fetch_text(f"{LIVE_BASE}/wiki/Special:AllPages")
    namespaces = parse_namespace_options(allpages_html)
    write_json(INVENTORY / "live_namespaces.json", namespaces)

    all_pages: list[dict[str, object]] = []
    for namespace in namespaces:
        ns_id = int(namespace["id"])
        ns_name = str(namespace["name"])
        pages = enumerate_namespace(ns_id, ns_name)
        log(f"  namespace {ns_id:>3} {ns_name}: {len(pages)} pages")
        all_pages.extend(pages)

    log("Scraping live Special:ListFiles for direct media URLs...")
    media = scrape_live_media()

    write_json(INVENTORY / "live_pages.json", all_pages)
    write_json(INVENTORY / "live_media.json", media)
    urls = [str(page["live_url"]) for page in all_pages] + [str(item["live_url"]) for item in media]
    (INVENTORY / "live_urls.txt").write_text("\n".join(sorted(urls)) + "\n", encoding="utf-8")
    summary = {
        "page_count": len(all_pages),
        "media_count": len(media),
        "namespace_count": len(namespaces),
        "source": LIVE_BASE,
        "wayback_source": ARCHIVE_BASE,
        "wayback_year": ARCHIVE_YEAR,
    }
    write_json(INVENTORY / "live_summary.json", summary)
    log(f"Live inventory complete: {len(all_pages)} wiki pages, {len(media)} media files.")


def cdx_page(prefix: str, resume_key: str | None = None, limit: int = 1000) -> tuple[list[dict[str, str]], str | None]:
    params = {
        "url": prefix,
        "from": FROM_TS,
        "to": TO_TS,
        "output": "json",
        "fl": "timestamp,original,statuscode,mimetype,digest",
        "filter": "statuscode:200",
        "limit": limit,
        "showResumeKey": "true",
    }
    if resume_key:
        params["resumeKey"] = resume_key
    url = f"{WAYBACK}/cdx?{urlencode(params)}"
    _status, _final, _ctype, text = fetch_text(url, timeout=180)
    if not text.strip():
        return [], None
    raw_rows = json.loads(text)
    rows: list[dict[str, str]] = []
    next_resume: str | None = None
    header: list[str] | None = None
    for row in raw_rows:
        if not row:
            continue
        if row == ["timestamp", "original", "statuscode", "mimetype", "digest"]:
            header = row
            continue
        if len(row) == 1:
            next_resume = row[0]
            continue
        if header and len(row) == len(header):
            rows.append(dict(zip(header, row)))
    return rows, next_resume


def normalize_original(url: str) -> str:
    parsed = urlparse(url)
    scheme = "http"
    netloc = parsed.netloc.lower()
    if netloc.endswith(":80"):
        netloc = netloc[:-3]
    if netloc == "www.datoolset.net":
        netloc = "datoolset.net"
    path = unescape(parsed.path)
    path = re.sub(r"/+", "/", path)
    query = unescape(parsed.query)
    if path.startswith("/wiki/"):
        title = path[len("/wiki/") :].replace(" ", "_")
        path = "/wiki/" + title
        query = ""
    return urlunparse((scheme, netloc, path, "", query, ""))


def capture_kind(url: str, mimetype: str) -> str:
    path = unescape(urlparse(url).path)
    if path.startswith("/mw/images/"):
        return "image"
    if path.startswith("/mw/skins/"):
        return "skin"
    if path.startswith("/mw/extensions/"):
        return "extension"
    if path in ("/", "/wiki", "/wiki/") or path.startswith("/wiki/"):
        if mimetype.startswith("text/html"):
            return "page"
    return "asset"


def add_live_inventory_targets(selected: dict[str, dict[str, str]]) -> dict[str, int]:
    added = {"pages": 0, "media": 0}
    live_pages_path = INVENTORY / "live_pages.json"
    if live_pages_path.exists():
        live_pages = read_json(live_pages_path)
        for page in live_pages:  # type: ignore[assignment]
            original = str(page.get("archive_url") or page.get("old_2015_url"))
            key = normalize_original(original)
            if key not in selected:
                selected[key] = {
                    "timestamp": TO_TS,
                    "original": original,
                    "statuscode": "closest",
                    "mimetype": "text/html",
                    "digest": "",
                    "selector": "live-page-target",
                    "normalized_url": key,
                    "kind": "page",
                }
                added["pages"] += 1
    live_media_path = INVENTORY / "live_media.json"
    if live_media_path.exists():
        live_media = read_json(live_media_path)
        for item in live_media:  # type: ignore[assignment]
            original = str(item["live_url"]).replace("https://", "http://", 1)
            key = normalize_original(original)
            if key not in selected:
                selected[key] = {
                    "timestamp": TO_TS,
                    "original": original,
                    "statuscode": "closest",
                    "mimetype": "image/*",
                    "digest": "",
                    "selector": "live-media-target",
                    "normalized_url": key,
                    "kind": "image",
                }
                added["media"] += 1
    return added


def collect_cdx() -> None:
    ensure_dirs()
    selectors = [
        ("root", "www.datoolset.net/"),
        ("favicon", "www.datoolset.net/favicon.ico"),
        ("pages", "www.datoolset.net/wiki/*"),
        ("skins", "www.datoolset.net/mw/skins/*"),
        ("resources", "www.datoolset.net/mw/resources/*"),
        ("load", "www.datoolset.net/mw/load.php*"),
    ]
    selectors.extend((f"images-{prefix}", f"www.datoolset.net/mw/images/{prefix}/*") for prefix in "0123456789abcdef")
    selectors.extend((f"thumbs-{prefix}", f"www.datoolset.net/mw/images/thumb/{prefix}/*") for prefix in "0123456789abcdef")
    all_rows: list[dict[str, str]] = []
    for label, selector in selectors:
        log(f"Collecting {ARCHIVE_YEAR} Wayback CDX rows for {label}...")
        resume_key = None
        selector_count = 0
        while True:
            rows, resume_key = cdx_page(selector, resume_key=resume_key)
            for row in rows:
                row["selector"] = label
            all_rows.extend(rows)
            selector_count += len(rows)
            log(f"  {label}: +{len(rows)} rows ({selector_count} total)")
            if not resume_key:
                break
            time.sleep(1)

    selected: dict[str, dict[str, str]] = {}
    for row in all_rows:
        key = normalize_original(row["original"])
        row = dict(row)
        row["normalized_url"] = key
        row["kind"] = capture_kind(row["original"], row["mimetype"])
        current = selected.get(key)
        if not current or row["timestamp"] > current["timestamp"]:
            selected[key] = row

    exact_selected_keys = set(selected)
    live_inventory_added = add_live_inventory_targets(selected)
    selected_rows = sorted(selected.values(), key=lambda item: (item["kind"], item["normalized_url"]))
    write_json(INVENTORY / f"wayback_{ARCHIVE_YEAR}_captures_all.json", all_rows)
    write_json(INVENTORY / f"wayback_{ARCHIVE_YEAR}_selected.json", selected_rows)

    coverage = {}
    live_pages_path = INVENTORY / "live_pages.json"
    if live_pages_path.exists():
        live_pages = read_json(live_pages_path)
        matched = 0
        missing: list[dict[str, object]] = []
        for page in live_pages:  # type: ignore[assignment]
            key = normalize_original(str(page.get("archive_url") or page.get("old_2015_url")))
            if key in exact_selected_keys:
                matched += 1
            else:
                missing.append(page)
        coverage = {
            "live_pages": len(live_pages),  # type: ignore[arg-type]
            f"live_pages_with_exact_{ARCHIVE_YEAR}_cdx_capture": matched,
            f"live_pages_without_exact_{ARCHIVE_YEAR}_cdx_capture": len(missing),
            "live_inventory_fallback_targets_added": live_inventory_added,
            "missing_manifest": f"_inventory/live_pages_missing_{ARCHIVE_YEAR}_capture.json",
        }
        write_json(INVENTORY / f"live_pages_missing_{ARCHIVE_YEAR}_capture.json", missing)

    kind_counts: dict[str, int] = {}
    for row in selected_rows:
        kind_counts[row["kind"]] = kind_counts.get(row["kind"], 0) + 1
    summary = {
        "from": FROM_TS,
        "to": TO_TS,
        "total_cdx_rows": len(all_rows),
        "selected_unique_urls": len(selected_rows),
        "selected_by_kind": kind_counts,
        "live_inventory_fallback_targets_added": live_inventory_added,
        "coverage": coverage,
    }
    write_json(INVENTORY / f"wayback_{ARCHIVE_YEAR}_summary.json", summary)
    log(f"CDX collection complete: {len(selected_rows)} unique {ARCHIVE_YEAR} URLs selected.")


def safe_segments(relative_path: str) -> list[str]:
    parts = [part for part in relative_path.split("/") if part]
    return [quote(part, safe="") for part in parts]


def local_path_for_original(url: str) -> Path:
    parsed = urlparse(normalize_original(url))
    path = parsed.path
    page_prefix = "/wiki/"
    if path in ("/", "/wiki", "/wiki/"):
        return PAGES / "Main_Page.html"
    if path.startswith(page_prefix):
        title = path[len(page_prefix) :] or "Main_Page"
        segments = safe_segments(title)
        if not segments:
            segments = ["Main_Page"]
        return PAGES.joinpath(*segments[:-1], f"{segments[-1]}.html")
    for prefix, base in (
        ("/mw/images/", ASSETS / "images"),
        ("/mw/skins/", ASSETS / "skins"),
        ("/mw/extensions/", ASSETS / "extensions"),
        ("/mw/resources/", ASSETS / "resources"),
    ):
        if path.startswith(prefix):
            segments = safe_segments(path[len(prefix) :])
            if not segments:
                segments = ["index"]
            return base.joinpath(*segments)
    if path.endswith("/mw/load.php"):
        if "only=styles" in parsed.query or "modules=site" in parsed.query:
            ext = ".css"
        elif "only=scripts" in parsed.query:
            ext = ".js"
        else:
            ext = ".bin"
        digest = sha1(normalize_original(url).encode("utf-8")).hexdigest()[:16]
        return ASSETS / "load" / f"load-{digest}{ext}"
    ext = Path(path).suffix
    if not ext and parsed.query:
        if "ctype=text/css" in parsed.query or "gen=css" in parsed.query:
            ext = ".css"
        elif "gen=js" in parsed.query:
            ext = ".js"
        else:
            ext = ".bin"
    digest = sha1(normalize_original(url).encode("utf-8")).hexdigest()[:16]
    name = quote(Path(path).name or "asset", safe="") + "-" + digest + ext
    return ASSETS / "misc" / name


def wayback_raw_url(timestamp: str, original: str) -> str:
    return f"{WAYBACK}/web/{timestamp}id_/{original}"


def extract_actual_timestamp(final_url: str) -> str | None:
    match = re.search(r"/web/(\d{14})(?:[a-z_]+)?/", final_url)
    return match.group(1) if match else None


def download_selected(force: bool = False, limit: int | None = None) -> None:
    ensure_dirs()
    selected_path = INVENTORY / f"wayback_{ARCHIVE_YEAR}_selected.json"
    if not selected_path.exists():
        collect_cdx()
    selected = read_json(selected_path)
    downloaded: list[dict[str, object]] = []
    errors: list[dict[str, object]] = []
    existing_manifest = INVENTORY / "download_manifest.json"
    if existing_manifest.exists() and not force:
        existing = read_json(existing_manifest)
        if isinstance(existing, list):
            downloaded.extend(existing)
    known = {str(item.get("normalized_url")) for item in downloaded if isinstance(item, dict)}

    rows = selected if limit is None else selected[:limit]  # type: ignore[index]
    total = len(rows)  # type: ignore[arg-type]
    for index, row in enumerate(rows, start=1):  # type: ignore[assignment]
        normalized_url = row["normalized_url"]
        local_path = local_path_for_original(row["original"])
        if not force and normalized_url in known and local_path.exists() and local_path.stat().st_size > 0:
            if index % 100 == 0:
                log(f"  skipped {index}/{total} (already downloaded)")
            continue
        if not force and local_path.exists() and local_path.stat().st_size > 0:
            known.add(normalized_url)
            continue
        local_path.parent.mkdir(parents=True, exist_ok=True)
        raw_url = wayback_raw_url(row["timestamp"], row["original"])
        try:
            status, final_url, content_type, data = fetch_bytes(raw_url, retries=5, timeout=120)
            local_path.write_bytes(data)
            record = dict(row)
            record.update(
                {
                    "status": status,
                    "content_type": content_type,
                    "wayback_url": raw_url,
                    "final_url": final_url,
                    "actual_timestamp": extract_actual_timestamp(final_url),
                    "local_path": str(local_path.relative_to(ROOT)).replace("\\", "/"),
                    "bytes": len(data),
                }
            )
            downloaded.append(record)
            known.add(normalized_url)
        except Exception as exc:
            errors.append({"row": row, "error": repr(exc)})
        if index % 25 == 0 or index == total:
            log(f"  downloaded pass {index}/{total}; ok={len(downloaded)} errors={len(errors)}")
            write_json(INVENTORY / "download_manifest.json", downloaded)
            write_json(INVENTORY / "download_errors.json", errors)
        time.sleep(0.15)
    write_json(INVENTORY / "download_manifest.json", downloaded)
    write_json(INVENTORY / "download_errors.json", errors)
    log(f"Download complete: {len(downloaded)} files recorded, {len(errors)} errors.")


def absolutize_archive_url(url: str) -> str | None:
    url = unescape(url.strip())
    if not url or url.startswith(("#", "mailto:", "javascript:")):
        return None
    if url.startswith("//"):
        url = "http:" + url
    if url.startswith(("/wiki/", "/mw/", "/favicon.ico")):
        return ARCHIVE_BASE + url
    parsed = urlparse(url)
    netloc = parsed.netloc.lower()
    if netloc.endswith(":80"):
        netloc = netloc[:-3]
    if netloc in ("datoolset.net", "www.datoolset.net"):
        return url.replace("https://", "http://", 1)
    return None


def local_ref_for_url(url: str, current_file: Path) -> str | None:
    anchor = ""
    if "#" in url:
        url, anchor = url.split("#", 1)
        anchor = "#" + anchor
    archive_url = absolutize_archive_url(url)
    if not archive_url:
        return None
    local_path = local_path_for_original(archive_url)
    if not local_path.exists():
        return None
    return os.path.relpath(local_path, current_file.parent).replace("\\", "/") + anchor


def rewrite_html() -> None:
    manifest_path = INVENTORY / "download_manifest.json"
    if not manifest_path.exists():
        log("No download manifest found; skipping rewrite.")
        return
    manifest = read_json(manifest_path)
    html_files = [
        ROOT / item["local_path"]
        for item in manifest  # type: ignore[assignment]
        if isinstance(item, dict) and item.get("kind") == "page" and str(item.get("local_path", "")).endswith(".html")
    ]
    unresolved: dict[str, int] = {}
    rewritten = 0
    attr_re = re.compile(r'(?P<attr>href|src)="(?P<url>[^"]*)"', re.I)
    for html_file in html_files:
        if not html_file.exists():
            continue
        text = html_file.read_text(encoding="utf-8", errors="replace")

        def replace(match: re.Match[str]) -> str:
            original = match.group("url")
            replacement = local_ref_for_url(original, html_file)
            if replacement:
                return f'{match.group("attr")}="{replacement}"'
            archive_url = absolutize_archive_url(original)
            if archive_url:
                unresolved[archive_url] = unresolved.get(archive_url, 0) + 1
            return match.group(0)

        updated = attr_re.sub(replace, text)
        if updated != text:
            html_file.write_text(updated, encoding="utf-8")
            rewritten += 1
    write_json(
        INVENTORY / "rewrite_unresolved_links.json",
        [{"url": url, "count": count} for url, count in sorted(unresolved.items(), key=lambda item: (-item[1], item[0]))],
    )
    log(f"Rewrite complete: {rewritten} HTML files updated; {len(unresolved)} archive-site refs unresolved.")


def verify() -> None:
    page_files = list(PAGES.rglob("*.html"))
    asset_files = [path for path in ASSETS.rglob("*") if path.is_file()]
    image_files = list((ASSETS / "images").rglob("*")) if (ASSETS / "images").exists() else []
    image_files = [path for path in image_files if path.is_file()]
    manifest = read_json(INVENTORY / "download_manifest.json") if (INVENTORY / "download_manifest.json").exists() else []
    errors = read_json(INVENTORY / "download_errors.json") if (INVENTORY / "download_errors.json").exists() else []
    external_refs = 0
    for html_file in page_files:
        text = html_file.read_text(encoding="utf-8", errors="ignore")
        external_refs += len(re.findall(r'(?:web\.archive\.org|datoolset\.net)/(?:wiki|mw)/', text))
    summary = {
        "pages_on_disk": len(page_files),
        "assets_on_disk": len(asset_files),
        "images_on_disk": len(image_files),
        "download_manifest_entries": len(manifest),  # type: ignore[arg-type]
        "download_errors": len(errors),  # type: ignore[arg-type]
        "archive_site_refs_remaining_in_html": external_refs,
        "main_page": str((PAGES / "Main_Page.html").relative_to(ROOT)).replace("\\", "/"),
        "main_page_exists": (PAGES / "Main_Page.html").exists(),
    }
    write_json(INVENTORY / "verification_summary.json", summary)
    log(json.dumps(summary, indent=2))


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Mirror the {ARCHIVE_YEAR} Dragon Age Toolset Wiki from Wayback.")
    parser.add_argument(
        "command",
        choices=("all", "inventory", "cdx", "download", "rewrite", "verify"),
        help="Phase to run.",
    )
    parser.add_argument("--force", action="store_true", help="Re-download existing files.")
    parser.add_argument("--limit", type=int, default=None, help="Limit downloads for a test run.")
    args = parser.parse_args()

    if args.command in ("all", "inventory"):
        inventory_live()
    if args.command in ("all", "cdx"):
        collect_cdx()
    if args.command in ("all", "download"):
        download_selected(force=args.force, limit=args.limit)
    if args.command in ("all", "rewrite"):
        rewrite_html()
    if args.command in ("all", "verify"):
        verify()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
