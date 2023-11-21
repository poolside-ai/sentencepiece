import subprocess
import yaml
import json
import os

with open("job_config.yml", "r", encoding="utf-8") as f:
    job_config = yaml.safe_load(f)

slaves = []

for slave in job_config["slaves"]:
    os.system(f"aws ec2 describe-instances --filters Name=tag:Name,Values={slave} > ec2.json")
    with open("ec2.json", "r", encoding="utf-8") as f:
        json_output = json.load(f)
    for reservation in json_output["Reservations"]:
        for instance in reservation["Instances"]:
            if instance["State"]["Name"] == "running":
                slaves.append(instance["PrivateIpAddress"])

if len(slaves) == 0:
    print("No slaves are running")
    exit(1)

user_input = input(f"Start the job with the following {len(slaves)} slaves? (Y/n)\n" + str(slaves) + "\n").lower()
if user_input not in ["", "y", "n"]:
    print("Illegal input. Please enter 'y' or 'n'.")
    exit(1)

if user_input != "n":
    pass
else:
    print("Job cancelled")
    exit(1)

files = []

for file in job_config["files"]:
    # strip s3://
    if file[:5] == "s3://":
        file = file[5:]
    bucket, prefix = file.split("/", 1)
    os.system(f"aws s3api list-objects --bucket {bucket} --prefix {prefix} > s3.json")

    with open("s3.json", "r", encoding="utf-8") as f:
        json_output = json.load(f)
    for i in range(len(json_output["Contents"])):
        files.append(f"s3://{bucket}/{json_output['Contents'][i]['Key']}")

if len(files) == 0:
    print("No files to process")
    exit(1)

user_input = input(str(files) + "\n" + f"Start the job with the following {len(files)} files? (Y/n)\n").lower()
if user_input not in ["", "y", "n"]:
    print("Illegal input. Please enter 'y' or 'n'.")
    exit(1)

if user_input != "n":
    pass
else:
    print("Job cancelled")
    exit(1)
# Assign files to slaves as evenly as possible
file_assignment = [[] for _ in range(len(slaves))]
for i in range(len(files)):
    file_assignment[i % len(slaves)].append(files[i])

file_i = 0
# Generate chunks.txt
with open("chunks.txt", "w", encoding="utf-8") as f:
    for i in range(len(slaves)):
        f.write(slaves[i])
        for file in file_assignment[i]:
            f.write(" " + str(file_i) + ":" + file)
            file_i += 1
        f.write("\n")
