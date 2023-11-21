set -e
set -x

cd /home/ubuntu

if ! command -v docker &> /dev/null; then
    sudo apt update
    sudo apt -y install docker.io
fi

if ! sudo docker images | grep -q "sentencepiece"; then
    sudo docker load < sp_dockerimage.tgz
fi

mkdir -p /home/ubuntu/data

if ! findmnt /dev/nvme1n1 >/dev/null; then
    sudo mkfs -t xfs /dev/nvme1n1
    sudo mount /dev/nvme1n1 /home/ubuntu/data
fi

sudo chown -R ubuntu:ubuntu /home/ubuntu/data

cd /home/ubuntu/data

cp ../container.sh ../host.sh .

INDEX=$1
FILE=$2

sudo docker run --rm -v /home/ubuntu/data:/host --name sp sentencepiece bash /host/container.sh "$INDEX" "$FILE"
