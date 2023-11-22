#!/bin/bash

# Variables to count success and failure
success_count=0
failure_count=0
total_runs=1000  # Adjust this to the number of times you want to run the command

# Clean and build the program
make clean
make 

# Run the command multiple times and count success and failure rates
for ((i=0; i<total_runs; i++))
do
    echo "Run $i"
    sudo ./ioctl_block
    if [ $? -eq 0 ]; then
        ((success_count++))
    else
        ((failure_count++))
    fi
done

# Calculate success and failure rates
success_rate=$(echo "scale=2; $success_count / $total_runs * 100" | bc)
failure_rate=$(echo "scale=2; $failure_count / $total_runs * 100" | bc)

# Print success and failure rates
echo "Success rate: $success_rate%"
echo "Failure rate: $failure_rate%"