#!/bin/bash

# Specify what numbers of threads will be tested
PARAM_THREADS="1,2,3,4,5,6,7,8,10,12"

# Define the files and warmup counts
files=("1"       "2"       "4"       "8"      "16"     "32"    "64"    "128"   "256" "512" "1024")
setts=("50:200" "50:200" "50:200" "50:100" "50:100" "25:100" "20:50" "10:20" "5:10" "4:5" "3:4")

inputType=$1
outputType=$2

main() {
	# Create ramfs if needed
	if [[ $inputType == "ram" ]] || [[ $outputType == "ram" ]]; then
		create_ramfs
	fi
	
	# Run Hyperfine for each file with specified warmup runs
	echo :: Running Hyperfine...
	for i in "${!files[@]}"; do
		input_size="${files[$i]}"
		IFS=":" read -r warm runs <<< "${setts[$i]}";
		
		psInput="test/${input_size}mb.dat"
		psOutput="result.out"
		
		if [[ $inputType == "ram" ]]; then
			cp "test/${input_size}mb.dat" "/mnt/temp/a/file.dat"
			psInput="/mnt/temp/a/file.dat"	
		fi
		
		if [[ $outputType == "ram" ]]; then
			psOutput="/mnt/temp/a/result.out"
		fi
		
		hyperfine \
		--warmup "$warm" \
		--runs "$runs" \
		--shell=none \
		--style basic \
		--parameter-list size ${input_size} \
		--parameter-list threads ${PARAM_THREADS} \
		--export-csv "benchmarks/benchmark_${input_size}mb.csv" \
		"./ep ${psInput} ${psOutput} {threads}"
	done

	# Merge the CSV files
	csvstack benchmarks/*.csv > raw_benchmark.csv	
}

create_ramfs() {
	# Echo creating temporary RAM fs
	echo :: Creating temporary ramfs.
	sudo mkdir /mnt/temp/a
	sudo mount ramfs -t ramfs /mnt/temp
	sudo mkdir /mnt/temp/a
	sudo chmod 777 /mnt/temp/a
}

main