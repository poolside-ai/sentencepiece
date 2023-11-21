set -e
set -x

REMOTE=$1

# r6i.32xlarge
# c5d.9xlarge
rsync -e "ssh -o StrictHostKeyChecking=no -i ssh.pem" -auP sp_dockerimage.tgz host.sh container.sh ubuntu@$REMOTE:/home/ubuntu

# Loop through arguments starting from $2
for arg in "${@:2}"; do
    # Extract index and value using awk
    INDEX=${arg%%:*}
    FILE=${arg#*:}

    ssh -o "StrictHostKeyChecking=no" -i "ssh.pem" ubuntu@$REMOTE bash host.sh "$INDEX" "$FILE"

    rsync -e "ssh -o StrictHostKeyChecking=no -i ssh.pem" -auP ubuntu@$REMOTE:/home/ubuntu/data/normalization$INDEX.txt .
    rsync -e "ssh -o StrictHostKeyChecking=no -i ssh.pem" -auP ubuntu@$REMOTE:/home/ubuntu/data/sp_cache_$INDEX.bin.zst .
    rsync -e "ssh -o StrictHostKeyChecking=no -i ssh.pem" -auP ubuntu@$REMOTE:/home/ubuntu/data/log.$INDEX .

    ssh -o "StrictHostKeyChecking=no" -i "ssh.pem" ubuntu@$REMOTE rm \
        /home/ubuntu/data/normalization$INDEX.txt \
        /home/ubuntu/data/sp_cache_$INDEX.bin.zst \
        /home/ubuntu/data/log.$INDEX
done

