"""Command-line entry points for xtrkcad-mcp tools."""

import argparse
import sys


def report_main() -> None:
    """Entry point for the xtrkcad-report command."""
    parser = argparse.ArgumentParser(
        prog="xtrkcad-report",
        description="Generate a layout summary report from an XTrkCAD .xtc file.",
    )
    parser.add_argument("path", help="Path to the .xtc or .xtce layout file")
    parser.add_argument(
        "-o", "--output",
        default=None,
        help="Output file path (default: <layout-name>_report.txt next to the input file)",
    )
    parser.add_argument(
        "-s", "--stations",
        default=None,
        metavar="YAML",
        help="Path to stations.yaml — enables annotation completeness checks",
    )
    parser.add_argument(
        "-f", "--format",
        default="txt",
        choices=["txt", "md", "html", "json"],
        help="Output format (default: txt)",
    )
    args = parser.parse_args()

    from pathlib import Path
    from xtrkcad_mcp.server import write_layout_report

    input_path = Path(args.path).expanduser()
    if not input_path.exists():
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if args.output:
        output_path = args.output
    else:
        suffix = f".{args.format}" if args.format != "txt" else ".txt"
        output_path = str(input_path.with_suffix("") ) + "_report" + suffix

    result = write_layout_report(
        path=str(input_path),
        output_path=output_path,
        stations_yaml=args.stations,
        format=args.format,
    )
    print(result)
