"""
Decode a Bristlemouth network_metrics.log to CSV for inspection.

ADIN component "adin_port_stats_<port>" fields (cumulative since boot):
  sqi/mse/lq/rxe/sye/fc/len/algn  (see the shared decoder for meaning)
  mse==0 means no link partner even though lq/sqi read "good".
"""

import argparse
import sys
from pathlib import Path
import pandas as pd

# Shared, canonical decoder lives with the schema (vendored via bm_core).

sys.path.append(
    str(
        Path(__file__).resolve().parents[3] / "src/lib/bm_core/bm_common_messages/tools"
    )
)
from decode_metrics_log import decode_metrics_log

LINK_QUALITY = {0: "poor", 1: "marginal", 2: "good"}


def load_metrics_by_component(log_path: str) -> dict[str, pd.DataFrame]:
    """Decode a network_metrics.log into one DataFrame per component family."""
    groups: dict[str, list[dict]] = {}
    for r in decode_metrics_log(log_path):
        comp = r["component"]
        # Fold a trailing "_<n>" into a `port` column
        family, _, tail = comp.rpartition("_")
        if family and tail.isdigit():
            port = int(tail)
        else:
            family, port = comp, None

        row = {
            "tick_ms": r["tick_ms"],
            "timestamp_utc": r["timestamp_utc"],
            "node_id": r["node_id"],
            "version": r["version"],
            "uptime_ms": r["uptime_ms"],
        }
        if port is not None:
            row["port"] = port
        fields = r["fields"]
        row.update(fields)
        if "lq" in fields:
            row["lq_label"] = (
                "no-link"
                if fields.get("mse") == 0
                else LINK_QUALITY.get(fields["lq"], "?")
            )
        groups.setdefault(family, []).append(row)

    tables = {}
    for name, rows in groups.items():
        df = pd.DataFrame(rows).dropna(axis=1, how="all")  # drop empty columns
        sort_cols = [c for c in ("node_id", "port", "tick_ms") if c in df.columns]
        tables[name] = df.sort_values(sort_cols).reset_index(drop=True)
    return tables


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Decode a Bristlemouth network_metrics.log to per-component CSVs"
    )
    parser.add_argument("log_file", help="path to network_metrics.log")
    args = parser.parse_args()

    base = args.log_file.rsplit(".", 1)[0]
    tables = load_metrics_by_component(args.log_file)
    if not tables:
        print("No decodable metrics found.")
        return
    for name, df in tables.items():
        out = f"{base}_{name}.csv"
        df.to_csv(out, index=False)
        print(f"Wrote {out} ({len(df)} rows)")


if __name__ == "__main__":
    main()
