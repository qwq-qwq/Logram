#!/usr/bin/env python3
"""Print one version's CHANGELOG.md section as HTML for the Sparkle update window.

Usage: changelog_html.py CHANGELOG.md 1.2.16

Fails (exit 1) when the version has no section or the section has no entries,
so a tag without a changelog entry stops the release before the long build.
Handles the subset CHANGELOG.md uses: "### " headings, "- " list items (with
indented continuation lines), paragraphs, **bold**, `code` and [text](url).
"""
import html
import re
import sys


def section(text, version):
    m = re.search(
        rf"^## \[{re.escape(version)}\][^\n]*\n(.*?)(?=^## |^\[[^\]]+\]: |\Z)",
        text, re.S | re.M)
    return m.group(1).strip() if m else None


def inline(s):
    out = []
    for part in re.split(r"(`[^`]+`)", s):
        if part.startswith("`") and part.endswith("`") and len(part) > 1:
            out.append("<code>" + html.escape(part[1:-1]) + "</code>")
            continue
        part = html.escape(part, quote=False)
        part = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", part)
        part = re.sub(r"\[([^\]]+)\]\((https?://[^)\s\"]+)\)", r'<a href="\2">\1</a>', part)
        out.append(part)
    return "".join(out)


def to_html(md):
    out, items = [], None

    def close_list():
        nonlocal items
        if items is not None:
            out.append("<ul>" + "".join(f"<li>{i}</li>" for i in items) + "</ul>")
            items = None

    for line in md.splitlines():
        line = line.rstrip()
        if line.startswith("- "):
            if items is None:
                items = []
            items.append(inline(line[2:]))
        elif items and line.startswith("  ") and line.strip():
            items[-1] += " " + inline(line.strip())
        else:
            close_list()
            if line.startswith("### "):
                out.append(f"<h3>{inline(line[4:])}</h3>")
            elif line:
                out.append(f"<p>{inline(line)}</p>")
    close_list()
    return "\n".join(out)


# Sparkle adds its own stylesheet (system font, dark mode colors); this only
# tightens the layout for a short list of changes.
STYLE = """<style>
h3 { font-size: 1em; margin: 0.9em 0 0.2em; }
h3:first-child { margin-top: 0; }
ul { margin: 0; padding-left: 1.3em; }
li { margin: 0.15em 0; }
code { font-family: ui-monospace, Menlo, monospace; font-size: 0.92em; }
</style>"""


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    path, version = sys.argv[1:]
    with open(path, encoding="utf-8") as f:
        body = section(f.read(), version)
    if body is None:
        sys.exit(f"{path}: no '## [{version}]' section; add the release notes before tagging")
    if not re.search(r"^- ", body, re.M):
        sys.exit(f"{path}: section [{version}] has no entries")
    sys.stdout.write(STYLE + "\n" + to_html(body) + "\n")


if __name__ == "__main__":
    main()
