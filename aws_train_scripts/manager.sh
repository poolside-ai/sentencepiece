set -e
# set -x

# Check if AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, and AWS_SESSION_TOKEN exist
if [[ -z "${AWS_ACCESS_KEY_ID}" || -z "${AWS_SECRET_ACCESS_KEY}" || -z "${AWS_SESSION_TOKEN}" ]]; then
    echo "One or more AWS environment variables are missing."
    exit 1
else
    echo "All AWS environment variables are set."
    # Continue with your script logic here
fi

sed \
    -e "s|<AWS_ACCESS_KEY_ID>|$AWS_ACCESS_KEY_ID|g" \
    -e "s|<AWS_SECRET_ACCESS_KEY>|$AWS_SECRET_ACCESS_KEY|g" \
    -e "s|<AWS_SESSION_TOKEN>|$AWS_SESSION_TOKEN|g" \
    container-template.sh > container.sh

# Check for existing *.log files
if ls *.log 1> /dev/null 2>&1; then
    echo "Existing log files found. Do you want to delete them? (Y/n)"
    read -r -n 1 answer
    echo
    if [[ "$answer" == "" || "$answer" == "y" ]]; then
        rm -f *.log
        echo "Log files deleted."
    else
        echo "Log files not deleted."
    fi
fi

while read p; do
    args=($p)
    echo ${args[0]}
    nohup bash start_job.sh $p 2>&1 >${args[0]}.log &
done <chunks.txt

