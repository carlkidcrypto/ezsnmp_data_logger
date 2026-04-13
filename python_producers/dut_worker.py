#!/usr/bin/env python3
"""DUT worker process - one per device under test."""
import os, sys, time, signal, logging, msgpack, zmq
from datetime import datetime, timezone

logger = logging.getLogger(__name__)

TYPE_NULL = 0
TYPE_I64  = 1
TYPE_F64  = 2
TYPE_STR  = 3

def _classify(value_str: str):
    """Return (type_tag, i64, f64, str_val) from a string SNMP value."""
    try:
        iv = int(value_str)
        return TYPE_I64, iv, 0.0, ""
    except (ValueError, TypeError):
        pass
    try:
        fv = float(value_str)
        return TYPE_F64, 0, fv, ""
    except (ValueError, TypeError):
        pass
    return TYPE_STR, 0, 0.0, str(value_str) if value_str is not None else ""

def run_worker(dut_config: dict, zmq_endpoint: str, poll_interval_ms: int):
    """Main worker loop. Runs until SIGTERM or KeyboardInterrupt."""
    dut_name = dut_config["name"]
    dut_ip   = dut_config["dut_ip"]
    metrics  = dut_config.get("metrics", [])
    snmp_cfg = dut_config.get("snmp", {})

    oid_list  = [m["oid"]  for m in metrics]
    name_list = [m["name"] for m in metrics]

    # Try to import ezsnmp; fall back to a stub for testing
    try:
        import ezsnmp
        session = ezsnmp.Session(
            hostname=dut_ip,
            version=snmp_cfg.get("version", 2),
            community=snmp_cfg.get("community", "public"),
            remote_port=snmp_cfg.get("port_number", 161),
            timeout=snmp_cfg.get("timeout", 1),
            retries=snmp_cfg.get("retries", 2),
        )
        def poll():
            return session.get(oid_list)
    except ImportError:
        logger.warning("ezsnmp not available; using stub poller for %s", dut_name)
        def poll():
            import random
            class R:
                def __init__(self, n, v): self.oid = n; self.value = str(v)
            return [R(o, random.randint(0, 1000)) for o in oid_list]

    ctx = zmq.Context()
    sock = ctx.socket(zmq.PUSH)
    sock.setsockopt(zmq.SNDHWM, 10000)
    sock.connect(zmq_endpoint)

    shutdown = False
    def _sig(signum, frame):
        nonlocal shutdown; shutdown = True
    signal.signal(signal.SIGTERM, _sig)

    interval_s = poll_interval_ms / 1000.0
    logging.basicConfig(level=logging.INFO,
        format=f"%(asctime)s [%(levelname)s] worker/{dut_name} %(message)s")
    logger.info("started, endpoint=%s interval_ms=%d", zmq_endpoint, poll_interval_ms)

    while not shutdown:
        tick_start = time.monotonic()
        ts_ns = time.time_ns()
        try:
            results = poll()
        except Exception as exc:
            logger.warning("poll error: %s", exc)
            results = []

        samples = []
        for idx, res in enumerate(results):
            if idx >= len(name_list):
                break
            name = name_list[idx]
            oid  = getattr(res, 'oid', oid_list[idx] if idx < len(oid_list) else "")
            val  = getattr(res, 'value', None)
            tag, i64, f64, sv = _classify(str(val) if val is not None else "")
            samples.append([name, oid, tag, i64, f64, sv, 0])

        batch = {
            "ts_unix_ns": ts_ns,
            "dut_name":   dut_name,
            "dut_ip":     dut_ip,
            "samples":    samples,
        }
        try:
            sock.send(msgpack.packb(batch, use_bin_type=True), zmq.NOBLOCK)
        except zmq.Again:
            logger.warning("ZMQ send would block (HWM reached), dropping batch")

        elapsed = time.monotonic() - tick_start
        sleep_s = max(0.0, interval_s - elapsed)
        if sleep_s > 0:
            time.sleep(sleep_s)

    sock.close()
    ctx.term()
    logger.info("shutdown complete")

if __name__ == "__main__":
    import json
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <dut_json> <zmq_endpoint> <poll_interval_ms>")
        sys.exit(1)
    dut_cfg  = json.loads(sys.argv[1])
    endpoint = sys.argv[2]
    interval = int(sys.argv[3])
    run_worker(dut_cfg, endpoint, interval)
