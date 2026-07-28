import argparse
import base64
import re
import pandas as pd
import cbor2

LINK_QUALITY = {0: "poor", 1: "marginal", 2: "good"}
LINE_RE = re.compile(r"^(?P<ts>\S+)\s*\|\s*(?P<node>\S+)/metrics\s+(?P<b64>[A-Za-z0-9+/=]+)")

"""
Each reply is a base64-encoded CBOR envelope:
  { "version": <v>, "node_id": <id>, "uptime_ms": <ms>,
    "data": { "<component>": { "<field>": <value>, ... }, ... } }

The decoder is generic: every component becomes a row and its fields become
columns, so new component types are captured with no changes here. Component-
specific niceties are layered on top only where they apply (see load_metrics).

Today the only producer is the ADIN2111 driver, which reports one component per
port, "adin_port_stats_<port>", with these fields (cumulative since boot):
  sqi  - Signal Quality Indicator 0-7 (7 best)
  mse  - raw MSE_VAL register (lower = cleaner signal; no units)
  lq   - link quality enum: 0 poor / 1 marginal / 2 good
  rxe  - frame-check RX (bad-CRC) error count
  sye  - symbol error count
  fc   - false-carrier count (noise on an idle line)
  len  - frame length-error count
  algn - frame alignment-error count
Note: an idle port with no link partner reports sqi=7/mse=0 - not a real "good" link.
Counters are monotonic; diff consecutive samples per (node, component) for a rate.
The leading log field is a millisecond tick from the bridge (not wall-clock time).
"""


def load_metrics(log_path: str) -> pd.DataFrame:
    rows = []
    with open(log_path, "r") as f:
        for line in f:
            m = LINE_RE.match(line)
            if not m:
                continue
            try:
                env = cbor2.loads(base64.b64decode(m.group("b64")))
            except Exception:
                continue  # skip malformed lines

            tick = m.group("ts").rstrip("t")
            tick_ms = int(tick) if tick.isdigit() else None
            node = env.get("node_id")
            node_id = f"{node:016x}" if isinstance(node, int) else m.group("node")

            # Generic: every component becomes a row and its fields become
            # columns, so new component types need no changes here.
            for component, fields in env.get("data", {}).items():
                if not isinstance(fields, dict):
                    continue
                row = {
                    "tick_ms": tick_ms,
                    "node_id": node_id,
                    "version": env.get("version"),
                    "uptime_ms": env.get("uptime_ms"),
                    "component": component,
                }
                row.update(fields)
                # Component-specific niceties, added only where they apply:
                if component.startswith("adin_port_stats_"):
                    row["port"] = int(component.rsplit("_", 1)[1])
                if "lq" in fields:
                    row["lq_label"] = LINK_QUALITY.get(fields["lq"], "?")
                rows.append(row)
    return pd.DataFrame(rows)


def main() -> None:
    parser = argparse.ArgumentParser(description="Decode a Bristlemouth network_metrics.log")
    parser.add_argument("log_file", help="path to network_metrics.log")
    args = parser.parse_args()

    df = load_metrics(args.log_file)
    csv_path = args.log_file.rsplit(".", 1)[0] + ".csv"
    df.to_csv(csv_path, index=False)
    print(f"Wrote {csv_path} ({len(df)} rows)")

if __name__ == "__main__":
    main()
