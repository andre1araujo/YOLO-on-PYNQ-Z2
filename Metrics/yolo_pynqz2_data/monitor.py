import re
import subprocess
import time
import sys
import os

def modify_runYolo(new_batch_start):
    pattern = r"\s*for \(int batch_start = \d+;"  # Pattern to match the line

    with open("programs/yolo_data.cpp", "r") as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if re.match(pattern, line):
            modified_line = re.sub(r"batch_start = \d+", "batch_start = {}".format(new_batch_start), line).rstrip()
            lines[i] = modified_line
            break

    with open("runYolo.cpp", "w") as f:
        f.writelines(lines)
    print("Modified runYolo.cpp: batch_start changed to", new_batch_start)

def compile_and_run_yolo():
    subprocess.call(["make"])  # Compile runYolo.cpp
    time.sleep(1)
    subprocess.call(["./yolo"])  # Run ./yolo

def main():
    yolo_proc = None

    while True:
        if yolo_proc is None or yolo_proc.poll() is not None:
            if yolo_proc is not None and yolo_proc.poll() is not None:
                yolo_proc.stdout.close()
            yolo_proc = subprocess.Popen(["./yolo"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
            new_batch_start = None  # Initialize to None
            prev_line = ""  # Initialize to an empty string
            modify_yolo = False  # Flag to indicate if runYolo.cpp needs to be modified

        line = yolo_proc.stdout.readline().strip()
        if line:
            print(line)  # Print console output
            sys.stdout.flush()  # Flush the output buffer
            if "[DNNDK] DPU timeout while execute DPU Task" in line:
                # Use the previous line to extract the new batch_start value
                match = re.search(r"Processing batch: (\d+)", prev_line)
                if match:
                    new_batch_start = int(match.group(1))
                    print("New batch_start:", new_batch_start)
                    modify_yolo = True  # Set flag to modify runYolo.cpp

            if modify_yolo:
                # Delete the yolo executable
                os.remove("yolo")
                time.sleep(4)
                print("File removed successfully!")

                modify_runYolo(new_batch_start)
                modify_yolo = False  # Reset the flag
                compile_and_run_yolo()
                os.remove("objects/yolo_data.o")  # Clean up compiled object file

            # Update the previous line
            prev_line = line
            time.sleep(0.1)  # Sleep for a short time before reading the next line

        time.sleep(1)  # Sleep for a short time before reading the next line

if __name__ == "__main__":
    main()
