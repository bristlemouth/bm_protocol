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


def load_metrics(log_path: str) -> pd.DataFrame:
    rows = []
    for r in decode_metrics_log(log_path):
        row = {
            "tick_ms": r["tick_ms"],
            "timestamp_utc": r["timestamp_utc"],
            "node_id": r["node_id"],
            "version": r["version"],
            "uptime_ms": r["uptime_ms"],
            "component": r["component"],
        }
        fields = r["fields"]
        row.update(fields)
        # Component-specific niceties, added only where they apply:
        if r["component"].startswith("adin_port_stats_"):
            row["port"] = int(r["component"].rsplit("_", 1)[1])
        if "lq" in fields:
            # mse==0 means no link partner even though lq/sqi read "good"
            row["lq_label"] = (
                "no-link"
                if fields.get("mse") == 0
                else LINK_QUALITY.get(fields["lq"], "?")
            )
        rows.append(row)
    return pd.DataFrame(rows)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Decode a Bristlemouth network_metrics.log"
    )
    parser.add_argument("log_file", help="path to network_metrics.log")
    args = parser.parse_args()

    df = load_metrics(args.log_file)
    csv_path = args.log_file.rsplit(".", 1)[0] + ".csv"
    df.to_csv(csv_path, index=False)
    print(f"Wrote {csv_path} ({len(df)} rows)")


if __name__ == "__main__":
    main()
