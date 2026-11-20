"""Build the Hillside tutorial as one PDF.

Part 1 (layout-config-guide.md), Part 2 (operating-conventions-tutorial.md)
and, as an appendix, the Operating Data Conventions they cite
(operating-conventions.md), with the screenshots. The Markdown files stay
the source; the PDF is built by CI (ci-mcp.yml) and attached to the MCP
releases, never committed.

    uv run --no-project --with pypandoc_binary --with weasyprint \\
        python mcp/docs/build_tutorial_pdf.py OUT.pdf [--link-base URL] [--version V]

--link-base: where links to files outside the PDF point (the schema, the
README), e.g. <server>/<repo>/blob/<commit>/mcp/docs. Left out, they stay
relative.
"""

from __future__ import annotations

import argparse
import datetime
import re
from pathlib import Path

DOCS = Path(__file__).resolve().parent
PARTS = [  # file, id, heading prefix
    ("layout-config-guide.md", "part-1", "Part 1: "),
    ("operating-conventions-tutorial.md", "part-2", "Part 2: "),
    ("operating-conventions.md", "appendix", "Appendix: "),
]
INSIDE = {file: f"#{ident}" for file, ident, _ in PARTS}

CSS = """
@page { size: Letter; margin: 2cm 1.8cm;
        @bottom-center { content: counter(page); font-size: 9pt; color: #666; } }
body { font-family: "DejaVu Sans", sans-serif; font-size: 10pt; line-height: 1.4; }
h1 { page-break-before: always; font-size: 20pt; border-bottom: 2px solid #335; }
h2 { font-size: 14pt; margin-top: 1.4em; page-break-after: avoid; }
h3 { font-size: 11.5pt; page-break-after: avoid; }
header#title-block-header { page-break-after: always; text-align: center; margin-top: 30%; }
header#title-block-header h1 { page-break-before: avoid; border: none; font-size: 26pt; }
nav#TOC { page-break-after: always; }
nav#TOC ul { list-style: none; padding-left: 1.2em; }
pre, code { font-family: "DejaVu Sans Mono", monospace; font-size: 8.5pt; }
pre { white-space: pre-wrap; overflow-wrap: anywhere; background: #f4f4f4;
      padding: 6px 8px; border-left: 3px solid #bbb; page-break-inside: avoid; }
table { border-collapse: collapse; margin: 0.6em 0; page-break-inside: avoid; }
th, td { border: 1px solid #bbb; padding: 3px 6px; font-size: 9pt; vertical-align: top; }
th { background: #e8e8ee; }
img { max-width: 100%; max-height: 11cm; display: block; margin: 0.5em auto; }
figure { margin: 0.8em 0; page-break-inside: avoid; }
figcaption { text-align: center; font-size: 9pt; color: #444; }
blockquote { color: #333; border-left: 3px solid #88a; margin-left: 0; padding-left: 10px; }
a { color: #224; }
"""


def combined_markdown(link_base: str | None) -> str:
    out = []
    for file, ident, prefix in PARTS:
        text = (DOCS / file).read_text(encoding="utf-8")
        lines = text.splitlines()
        assert lines[0].startswith("# "), f"{file} must start with its title"
        lines[0] = f"# {prefix}{lines[0][2:]} {{#{ident}}}"
        text = "\n".join(lines)

        def link(m):
            target = m.group(1)
            if target in INSIDE:
                return f"]({INSIDE[target]})"
            if link_base and not re.match(r"[a-z]+:|#|images/", target):
                return f"]({link_base.rstrip('/')}/{target})"
            return m.group(0)

        out.append(re.sub(r"\]\(([^)\s]+)\)", link, text))
    return "\n\n".join(out) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("out")
    ap.add_argument("--link-base")
    ap.add_argument("--version", default="")
    args = ap.parse_args()

    import pypandoc
    from weasyprint import CSS as WCSS, HTML

    subtitle = "The Hillside tutorial"
    if args.version:
        subtitle += f", MCP server {args.version}"
    html = pypandoc.convert_text(
        combined_markdown(args.link_base), "html5", format="commonmark_x+implicit_figures",
        extra_args=["--standalone", "--toc", "--toc-depth=2",
                    "--metadata", "title=XTrkCAD layouts with the MCP server",
                    "--metadata", f"subtitle={subtitle}",
                    "--metadata", f"date={datetime.date.today().isoformat()}"])
    HTML(string=html, base_url=str(DOCS)).write_pdf(args.out, stylesheets=[WCSS(string=CSS)])
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
