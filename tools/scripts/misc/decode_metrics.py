import argparse
import base64
import re
import pandas as pd
import cbor2

LINK_QUALITY = {0: "poor", 1: "marginal", 2: "good"}
LINE_RE = re.compile(r"^(?P<ts>\S+)\s*\|\s*(?P<node>\S+)/metrics\s+(?P<b64>[A-Za-z0-9+/=]+)")

# Per-port fields flattened by the bm_core network component as "<name>_<port>".
PORT_FIELDS = ["sqi", "mse", "lq", "rxe", "sye", "fc", "len", "algn"]

"""
Each reply is a base64-encoded CBOR envelope:
  { "mv": <envelope version>, "node": <id>, "up": <uptime_ms>,
    "data": { "network_port_stats": { "num_ports": N, "sqi_1":.., "mse_1":.., ... },
              <other components...> } }
Field meanings (network_port_stats, per ADIN2111 port, cumulative since boot):
  sqi  - Signal Quality Indicator 0-7 (7 best)
  mse  - raw MSE_VAL register (lower = cleaner signal; no units)
  lq   - link quality enum: 0 poor / 1 marginal / 2 good
  rxe  - frame-check RX (bad-CRC) error count
  sye  - symbol error count
  fc   - false-carrier count (noise on an idle line)
  len  - frame length-error count
  algn - frame alignment-error count
Note: an idle port with no link partner reports sqi=7/mse=0 - not a real "good" link.
Counters are monotonic; diff consecutive samples per (node, port) for a rate.
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
            net = env.get("data", {}).get("network_port_stats")
            if not net:
                continue  # only network stats for now; other components ignored

            tick = m.group("ts").rstrip("t")
            tick_ms = int(tick) if tick.isdigit() else None
            node = env.get("node_id")
            node_id = f"{node:016x}" if isinstance(node, int) else m.group("node")

            for port in range(1, net.get("num_ports", 0) + 1):
                row = {
                    "tick_ms": tick_ms,
                    "node_id": node_id,
                    "version": env.get("version"),
                    "uptime_ms": env.get("uptime_ms"),
                    "port": port,
                }
                for fld in PORT_FIELDS:
                    row[fld] = net.get(f"{fld}_{port}")
                row["lq_label"] = LINK_QUALITY.get(row.get("lq"), "?")
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
