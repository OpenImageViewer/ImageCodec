"""Full-mip baseline/candidate comparisons with setup and hashing outside timing."""
import argparse
import json
import os
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("baseline", type=Path)
parser.add_argument("candidate", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--formats", nargs="+", default=["bgra", "rgba", "bgrx", "5551", "4444", "bc4", "bc5"])
parser.add_argument("--sizes", nargs="+", type=int, default=[64, 2048, 8192])
parser.add_argument("--patterns", nargs="+", choices=["random", "constant"], default=["random"])
parser.add_argument("--file", action="store_true")
parser.add_argument("--live", action="store_true")
parser.add_argument("--parallel", action="store_true")
parser.add_argument("--independent", action="store_true", help="Give each worker independent encoded storage")
parser.add_argument("--pair", action="store_true", help="Only baseline and combined candidate")
parser.add_argument("--workers", nargs="+", type=int, default=[1, 2, 8, 24])
parser.add_argument("--cpus", nargs="+", type=int, help="Explicit logical CPU order for parallel workers")
parser.add_argument("--cpu", type=int, help="Opt-in Windows single-thread affinity")
parser.add_argument("--iterations", type=int)
parser.add_argument("--consume", choices=["both", "yes", "no"], default="both")
args = parser.parse_args()
if args.parallel and (not args.cpus or max(args.workers) > len(args.cpus)):
    parser.error("Parallel runs require enough explicit --cpus for every worker")
args.output.mkdir(parents=True, exist_ok=True)
rows, summary = [], []
def save(name, data):
    (args.output / name).write_bytes((json.dumps(data, indent=2) + "\n").replace("\n", "\r\n").encode())
for pattern in args.patterns:
 for width in args.sizes:
  for fmt in args.formats:
   for workers in (args.workers if args.parallel else [1]):
    for consume in ([False] if args.parallel or args.consume == "no" else [True] if args.consume == "yes" else [False, True]):
        iterations = args.iterations or (384 if args.parallel else {64: 40000, 1024: 128, 2048: 128, 8192: 8}.get(width, 32))
        if not args.iterations and width == 64 and fmt in ["bgra", "rgba"]: iterations = 200000
        if args.file: iterations = min(iterations, 2000)
        variants = [("baseline", args.baseline, "raw"), ("direct", args.candidate, "direct"),
                    ("pooled", args.candidate, "raw")]
        if args.pair:
            variants = [("baseline", args.baseline, "raw"), ("candidate", args.candidate, "raw")]
        mode = "file" if args.file else "live" if args.live else "parallel" if args.parallel else None
        if mode: variants = [("baseline", args.baseline, mode), ("candidate", args.candidate, mode)]
        environment = os.environ.copy()
        environment["DDS_BENCH_PATTERN"] = pattern
        if args.cpu is not None: environment["DDS_BENCH_CPU"] = str(args.cpu)
        if args.parallel: environment["DDS_BENCH_CPUS"] = ",".join(map(str, args.cpus[:workers]))
        if args.independent: environment["DDS_BENCH_INDEPENDENT"] = "1"
        else: environment.pop("DDS_BENCH_INDEPENDENT", None)
        group = []
        def run(trial):
            for variant, binary, api in (variants if trial % 2 else list(reversed(variants))):
                command = [str(binary), fmt, str(width), str(iterations), api, "consume" if consume else "decode"]
                if args.file: command.append(str((args.output / "fixture.dds").resolve()))
                result = json.loads(subprocess.check_output(command, text=True, env=environment))
                result.update(variant=variant, trial=trial, cpu=args.cpu, cpus=args.cpus[:workers] if args.parallel else None)
                group.append(result); rows.append(result); save("timings.json", rows)
        def stats():
            result = {}
            for variant, _, _ in variants:
                samples = [x["ms"] for x in group if x["variant"] == variant]
                median = statistics.median(samples)
                result[variant] = dict(median=median, min=min(samples), max=max(samples),
                    cv=100 * statistics.stdev(samples) / statistics.mean(samples), trials=len(samples), images_per_second=1000 / median)
            return result
        for trial in range(1, 4): run(trial)
        if max(x["cv"] for x in stats().values()) > 5:
            for trial in range(4, 8): run(trial)
        assert len({x["digest"] for x in group}) == 1, "Pixel parity failed"
        assert len({x.get("upload_digest", "0") for x in group}) == 1, "Upload parity failed"
        result = dict(format=fmt, width=width, consume=consume, pattern=pattern, workers=workers, independent_inputs=args.independent,
                      mips=group[0]["mips"], output_bytes=group[0].get("output_bytes"), stats=stats())
        if args.parallel:
            one = next((x for x in summary if x["format"] == fmt and x["width"] == width and x["pattern"] == pattern and x["workers"] == 1), result)
            for variant in result["stats"]:
                speedup = one["stats"][variant]["median"] / result["stats"][variant]["median"]
                result["stats"][variant].update(speedup=speedup, efficiency=speedup/workers)
        summary.append(result); save("summary.json", summary); print(json.dumps(result), flush=True)
