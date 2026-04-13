#!/usr/bin/env python3
"""Orchestrator: spawn one DUT worker process per device in the config."""
import json, sys, os, signal, time, logging, argparse
import multiprocessing as mp
from python_producers.dut_worker import run_worker

logging.basicConfig(level=logging.INFO,
    format="%(asctime)s [%(levelname)s] orchestrator %(message)s")
logger = logging.getLogger(__name__)

def _worker_main(dut_config, zmq_endpoint, poll_interval_ms):
    run_worker(dut_config, zmq_endpoint, poll_interval_ms)

def main():
    parser = argparse.ArgumentParser(description="ezsnmp_data_logger orchestrator")
    parser.add_argument("--config", required=True, help="Path to JSON config file")
    args = parser.parse_args()

    with open(args.config) as f:
        cfg = json.load(f)

    zmq_endpoint  = cfg.get("zmq_endpoint", "ipc:///tmp/ezsnmp_loggerd.sock")
    poll_interval = cfg.get("poll_interval_ms", 1000)
    duts          = cfg.get("duts", [])

    # Validate metric name uniqueness per DUT
    for dut in duts:
        names = [m["name"] for m in dut.get("metrics", [])]
        seen = set()
        for n in names:
            if n in seen:
                raise ValueError(f"Duplicate metric name '{n}' in DUT '{dut['name']}'")
            seen.add(n)

    # Use spawn for macOS compatibility
    ctx = mp.get_context(cfg.get("process_start_method", "spawn"))

    procs = []
    for dut in duts:
        p = ctx.Process(
            target=_worker_main,
            args=(dut, zmq_endpoint, poll_interval),
            name=f"worker-{dut['name']}",
            daemon=True,
        )
        p.start()
        logger.info("spawned worker for DUT '%s' (pid=%d)", dut["name"], p.pid)
        procs.append(p)

    def _shutdown(signum, frame):
        logger.info("shutting down workers...")
        for p in procs:
            if p.is_alive():
                p.terminate()
        for p in procs:
            p.join(timeout=5)
        sys.exit(0)

    signal.signal(signal.SIGINT,  _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    logger.info("all %d workers started, waiting...", len(procs))
    while True:
        for p in procs:
            if not p.is_alive():
                logger.warning("worker '%s' died (exit=%s)", p.name, p.exitcode)
        time.sleep(5)

if __name__ == "__main__":
    main()
