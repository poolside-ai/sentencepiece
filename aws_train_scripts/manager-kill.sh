
while read p; do
    args=($p)
    echo ${args[0]}
    nohup ssh -o "StrictHostKeyChecking=no" -i "ssh.pem" ubuntu@${args[0]} "sudo docker kill sp" 2>&1 &
done <chunks.txt

