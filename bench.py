import subprocess
import json

# Define the threshold for consistency
# Rerun the benchmark if the stddev > 6ms
STDDEV_THRESHOLD = 0.006
STDDEV_THRESHOLD = .5
MAX_RETRIES = 10

# Run hyperfine on the given command and return the parsed results.
def run_hyperfine(size, threads):
	try:
		cmd = [
			"hyperfine",
			"--export-json", "result.json",
			"--export-csv", f"benchmarks/{size}mb.{threads}.csv",
			"--warmup", "5",
			"--runs", "8",
			"--parameter-list", "threads", str(threads),
			"--prepare", "sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches; sleep 1",
			"sudo nice --19 ./ep /mnt/temp/a/file.dat /mnt/temp/a/result.out {threads}"
			#f"sudo nice --19 ./ep test/{size}mb.dat result.out {threads}",
			
		]
		result = subprocess.run(cmd, check=True, capture_output=True, text=True)

		# Load results from the JSON file
		with open("result.json", "r") as f:
			data = json.load(f)

		return data
	except subprocess.CalledProcessError as e:
		print(f"Error running hyperfine: {e.stderr}")
		return None

# Check if the benchmark result is consistent
def is_consistent(result):
	run = result.get("results")[0]

	# Check standard deviation and any presence of outliers
	stddev = run["stddev"]
	mean = run["mean"]
	if stddev > STDDEV_THRESHOLD:
		print(f"Inconsistent result: Mean {mean:.3f}s - Stddev {stddev:.3f}s exceeds threshold.")
		return False

	return True

def main():
	threadsParam = [1,2,4,6,8,10,12]
	
	for threads in threadsParam:
		print(f"\n-- {threads} Threads --")
		retries = 0
		while retries < MAX_RETRIES:
			print(f":: Try {retries + 1}")
			result = run_hyperfine(8, threads)
						
			if is_consistent(result):
				out = result.get("results")[0]
				mean = out["mean"] * 1000
				sdev = out["stddev"] * 1000
				print(f"Mean: {mean:.2f}ms, ± {sdev:.2f}ms")
				break
			
			retries += 1
		else:
			print("Maximum retries reached. Benchmark could not stabilize.")

if __name__ == "__main__":
	main()
