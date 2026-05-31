#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
import json
import re
import shutil
import unicodedata
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import quote, unquote, urljoin, urlparse

import mirror_live


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "github-wiki"


class DivExtractor(HTMLParser):
    def __init__(self, target_id: str) -> None:
        super().__init__(convert_charrefs=False)
        self.target_id = target_id
        self.collecting = False
        self.depth = 0
        self.parts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attr_map = dict(attrs)
        if not self.collecting and tag.lower() == "div" and attr_map.get("id") == self.target_id:
            self.collecting = True
            self.depth = 1
            return
        if self.collecting:
            self.depth += 1 if tag.lower() == "div" else 0
            self.parts.append(self.get_starttag_text() or "")

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if self.collecting:
            self.parts.append(self.get_starttag_text() or "")

    def handle_endtag(self, tag: str) -> None:
        if not self.collecting:
            return
        if tag.lower() == "div":
            self.depth -= 1
            if self.depth == 0:
                self.collecting = False
                return
        self.parts.append(f"</{tag}>")

    def handle_data(self, data: str) -> None:
        if self.collecting:
            self.parts.append(data)

    def handle_entityref(self, name: str) -> None:
        if self.collecting:
            self.parts.append(f"&{name};")

    def handle_charref(self, name: str) -> None:
        if self.collecting:
            self.parts.append(f"&#{name};")

    def handle_comment(self, data: str) -> None:
        pass


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def page_title_from_html(source: Path, fallback: str) -> str:
    text = source.read_text(encoding="utf-8", errors="ignore")
    match = re.search(r'<h1[^>]*id="firstHeading"[^>]*>(.*?)</h1>', text, re.I | re.S)
    if match:
        return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", html.unescape(match.group(1)))).strip() or fallback
    return fallback


def extract_content(source: Path) -> str:
    text = source.read_text(encoding="utf-8", errors="ignore")
    parser = DivExtractor("mw-content-text")
    parser.feed(text)
    content = "".join(parser.parts).strip()
    if not content:
        body = re.search(r"<body[^>]*>(.*?)</body>", text, re.I | re.S)
        content = body.group(1).strip() if body else text
    content = re.sub(r"<script\b.*?</script>", "", content, flags=re.I | re.S)
    content = re.sub(r"<style\b.*?</style>", "", content, flags=re.I | re.S)
    return content.strip()


def github_slug(title: str, used: dict[str, str]) -> str:
    if title == "Main Page":
        base = "Home"
    else:
        normalized = unicodedata.normalize("NFKC", title).strip()
        base = re.sub(r'[\\/:*?"<>|#%\[\]`{}]+', "-", normalized)
        base = re.sub(r"\s+", "-", base)
        base = re.sub(r"-+", "-", base).strip("-. ")
        base = base or "Page"
    slug = base
    if slug.lower() == "home" and title != "Main Page":
        slug = f"{slug}-page"
    key = slug.lower()
    if key in used and used[key] != title:
        suffix = 2
        while f"{slug}-{suffix}".lower() in used:
            suffix += 1
        slug = f"{slug}-{suffix}"
        key = slug.lower()
    used[key] = title
    return slug


def build_page_map() -> tuple[dict[str, dict[str, str]], dict[str, str], dict[str, str]]:
    live_pages = load_json(ROOT / "_inventory" / "live_pages.json")
    by_title: dict[str, dict[str, str]] = {}
    used: dict[str, str] = {}
    title_to_slug: dict[str, str] = {}
    source_path_to_title: dict[str, str] = {}
    for page in live_pages:  # type: ignore[assignment]
        title = str(page["title"]).replace(" ", "_")
        source = mirror_live.title_to_path(title)
        if not source.exists():
            continue
        display_title = title.replace("_", " ")
        slug = github_slug(display_title, used)
        if title == "Main_Page":
            slug = "Home"
        by_title[title.lower()] = {
            "title": title,
            "display_title": display_title,
            "slug": slug,
            "source": str(source),
            "output": f"{slug}.md",
        }
        title_to_slug[title.lower()] = slug
        source_path_to_title[str(source.resolve()).lower()] = title
    return by_title, title_to_slug, source_path_to_title


def asset_output_path(source: Path) -> Path:
    relative = source.resolve().relative_to((ROOT / "assets").resolve())
    return OUT / "assets" / relative


def raw_asset_url(repo: str, source: Path) -> str:
    relative = asset_output_path(source).relative_to(OUT).as_posix()
    return f"https://raw.githubusercontent.com/wiki/{repo}/{quote(relative, safe='/._~!$&()*+,;=:@')}"


def wiki_page_url(repo: str, slug: str) -> str:
    return f"https://github.com/{repo}/wiki/{quote(slug, safe='-._~')}"


def title_from_href(value: str, current_source: Path) -> str | None:
    if value.startswith("#"):
        return None
    title = mirror_live.url_to_title(value, base=mirror_live.LIVE_BASE)
    if title:
        return title
    parsed = urlparse(value)
    if parsed.scheme or parsed.netloc:
        return None
    clean = unquote(parsed.path)
    if clean.endswith(".html"):
        candidate = (current_source.parent / clean).resolve()
        if candidate.exists():
            for page_file in (ROOT / "pages").rglob("*.html"):
                if page_file.resolve() == candidate:
                    return page_file.stem
        # Fall back to the URL's stem. This works for most flat wiki page links.
        return clean[:-5]
    return None


def resolve_asset(value: str, current_source: Path) -> Path | None:
    parsed = urlparse(value)
    if value.startswith("../assets/"):
        candidate = (current_source.parent / value).resolve()
        if candidate.exists():
            return candidate
    if value.startswith("assets/"):
        candidate = (ROOT / value).resolve()
        if candidate.exists():
            return candidate
    normalized = mirror_live.normalize_live_url(value, base=mirror_live.LIVE_BASE)
    if normalized:
        candidate = mirror_live.asset_path_for_url(normalized)
        if candidate.exists():
            return candidate
        original = mirror_live.original_for_thumbnail(normalized)
        if original:
            original_candidate = mirror_live.asset_path_for_url(original)
            if original_candidate.exists():
                return original_candidate
    if not parsed.scheme and not parsed.netloc:
        candidate = (current_source.parent / value).resolve()
        if candidate.exists():
            return candidate
    return None


def rewrite_content(content: str, source: Path, repo: str, title_to_slug: dict[str, str]) -> tuple[str, set[Path], set[str]]:
    assets: set[Path] = set()
    unresolved: set[str] = set()

    def rewrite_attr(match: re.Match[str]) -> str:
        attr = match.group("attr").lower()
        value = html.unescape(match.group("value"))
        if value.startswith("#"):
            return match.group(0)
        if attr in ("src", "poster"):
            asset = resolve_asset(value, source)
            if asset:
                assets.add(asset)
                return f'{match.group("attr")}="{raw_asset_url(repo, asset)}"'
            unresolved.add(value)
            return match.group(0)
        if attr == "href":
            asset = resolve_asset(value, source)
            if asset and asset.is_file() and not asset.suffix.lower() == ".html":
                assets.add(asset)
                return f'{match.group("attr")}="{raw_asset_url(repo, asset)}"'
            title = title_from_href(value, source)
            if title:
                normalized_title = title.replace(" ", "_").lower()
                slug = title_to_slug.get(normalized_title)
                if slug:
                    fragment = ""
                    if "#" in value:
                        fragment = "#" + value.split("#", 1)[1]
                    return f'{match.group("attr")}="{wiki_page_url(repo, slug)}{fragment}"'
            if value.startswith(("/wiki/", "/mw/")):
                return f'{match.group("attr")}="{urljoin(mirror_live.LIVE_BASE, value)}"'
        return match.group(0)

    content = re.sub(r'(?P<attr>href|src|poster)="(?P<value>[^"]*)"', rewrite_attr, content, flags=re.I)

    def rewrite_srcset(match: re.Match[str]) -> str:
        rewritten: list[str] = []
        for candidate in match.group("value").split(","):
            pieces = candidate.strip().split()
            if pieces:
                asset = resolve_asset(html.unescape(pieces[0]), source)
                if asset:
                    assets.add(asset)
                    pieces[0] = raw_asset_url(repo, asset)
                else:
                    unresolved.add(pieces[0])
            rewritten.append(" ".join(pieces))
        return f'srcset="{", ".join(rewritten)}"'

    content = re.sub(r'srcset="(?P<value>[^"]*)"', rewrite_srcset, content, flags=re.I)
    return content, assets, unresolved


def export(repo: str, clean: bool) -> None:
    if clean and OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "assets").mkdir(exist_ok=True)

    by_title, title_to_slug, _source_map = build_page_map()
    copied_assets: set[Path] = set()
    unresolved: dict[str, list[str]] = {}
    exported_pages: list[dict[str, str]] = []

    for page in sorted(by_title.values(), key=lambda item: (item["slug"].lower() != "home", item["slug"].lower())):
        source = Path(page["source"])
        title = page_title_from_html(source, page["display_title"])
        content = extract_content(source)
        content, assets, unresolved_links = rewrite_content(content, source, repo, title_to_slug)
        copied_assets.update(assets)
        if unresolved_links:
            unresolved[page["title"]] = sorted(unresolved_links)
        output = OUT / page["output"]
        output.write_text(
            f"# {title}\n\n"
            f"<!-- Exported from the local Dragon Age Toolset Wiki mirror. GitHub's native theme provides dark mode. -->\n\n"
            f"{content}\n",
            encoding="utf-8",
        )
        exported_pages.append({"title": page["title"], "slug": page["slug"], "file": page["output"]})

    # Copy only assets actually referenced by exported page content. This keeps the wiki
    # smaller than the raw static mirror while preserving page rendering.
    for asset in sorted(copied_assets):
        if not asset.exists() or not asset.is_file():
            continue
        target = asset_output_path(asset)
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.stat().st_size != asset.stat().st_size:
            shutil.copy2(asset, target)

    sidebar_links = [
        ("Home", "Home"),
        ("Getting Started", title_to_slug.get("getting_started", "")),
        ("Tutorials", title_to_slug.get("tutorials", "")),
        ("Design", title_to_slug.get("design", "")),
        ("Art", title_to_slug.get("art", "")),
        ("Cinematography", title_to_slug.get("cinematography", "")),
        ("Sound and Music", title_to_slug.get("sound_and_music", "")),
        ("Script", title_to_slug.get("script", "")),
        ("Technical Information", title_to_slug.get("technical_information", "")),
    ]
    (OUT / "_Sidebar.md").write_text(
        "\n".join(f"* [{label}]({wiki_page_url(repo, slug)})" for label, slug in sidebar_links if slug) + "\n",
        encoding="utf-8",
    )

    report = {
        "repo": repo,
        "wiki_url": f"https://github.com/{repo}/wiki",
        "raw_asset_base": f"https://raw.githubusercontent.com/wiki/{repo}/assets",
        "exported_pages": len(exported_pages),
        "copied_assets": len(copied_assets),
        "total_files": sum(1 for path in OUT.rglob("*") if path.is_file()),
        "github_wiki_soft_limit": 5000,
        "soft_limit_note": "GitHub documents a soft limit of 5,000 total wiki files; this full export exceeds it.",
        "dark_mode_note": "GitHub wikis do not support custom CSS; exported pages avoid the old static MediaWiki CSS and rely on GitHub's native dark theme.",
        "unresolved_pages": len(unresolved),
    }
    write_json(OUT / "_export-report.json", report)
    write_json(OUT / "_page-map.json", exported_pages)
    if unresolved:
        write_json(OUT / "_unresolved-links.json", unresolved)
    (OUT / "README.md").write_text(
        "# Dragon Age Toolset Wiki Export\n\n"
        "This folder is a generated GitHub Wiki export.\n\n"
        "Push the contents of this folder to the wiki repository, for example:\n\n"
        "```powershell\n"
        "git clone https://github.com/adarec1994/Haven-Tools.wiki.git Haven-Tools.wiki\n"
        "Copy-Item -Recurse -Force Wiki\\github-wiki\\* Haven-Tools.wiki\\\n"
        "cd Haven-Tools.wiki\n"
        "git add .\n"
        "git commit -m \"Import Dragon Age Toolset wiki mirror\"\n"
        "git push\n"
        "```\n\n"
        "GitHub wikis have a documented soft limit of 5,000 total files. This full export may exceed that limit.\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Export the local mirror as a GitHub Wiki repository.")
    parser.add_argument("--repo", default="adarec1994/Haven-Tools", help="GitHub owner/repo for generated wiki links.")
    parser.add_argument("--clean", action="store_true", help="Regenerate the export folder from scratch.")
    args = parser.parse_args()
    export(args.repo, args.clean)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
