# Scripts for Starting BPE Model Training

## Setup

### The master machine

https://us-east-1.console.aws.amazon.com/ec2/home?region=us-east-1#InstanceDetails:instanceId=i-0983ded603f0d5205

1. Download the docker image:

```bash
aws s3 cp s3://poolside.yiyang/images/sp_dockerimage.tgz sp_dockerimage.tgz
```

2. Load the docker image:

```bash
docker load < sp_dockerimage.tgz
```

3. Prepare the SSH key:

Place the key to access the slaves here, with the name of `ssh.pem`.

### The slave machines (used in the first phase of training)

Go here:

https://us-east-1.console.aws.amazon.com/ec2/home?region=us-east-1#Instances:sort=tag:Name

You should see 8 machines with name `bpe-train2`.

The number of machines should be equal to the number of chunks to process. Feel free to add more machines with the same settings.

## Distributed Training of the First Phase

### Step 1: Prepare `chunks.txt`

Start the slave machines. And run `python3 prepare_chunks.py`.

### Step 2: Gather Sentences

Run `manager.sh` like this `bash manager.sh`.

Logs will be appended to `0.log` to `7.log`.
If everything goes smoothly, You should see `sp_cache_0.bin.zst` to `sp_cache_7.bin.zst` under this directory.

Then you can shutdown the slave machines.

### Step 3: Merge

Start a docker container:

```bash
docker run -d --rm -v $(pwd):/host --name sp sentencepiece sleep infinity
```

And attach into it:

```bash
docker exec -it sp bash
```

Go to the working directory:

```bash
cd /host
```

Start the merge:

```bash
/sentencepiece/build/src/spm_bpe_cache_merge --output sp_cache_merged.bin sp_cache_*.bin
```

`sp_cache_merged.bin` should exist (`/host` in the container and `/home/ubuntu/aws_train_scripts` in the host) if everything works.

### Step 4: Train

Still inside the container:

This might take a while, it is best you wrap it with nohup and run it in the background even if the connection is lost.

```bash
rm -f log.full && /sentencepiece/build/src/spm_train --vocab_size 100000 --split_digits --byte_fallback --verbatim_control_char=1 --max_sentence_length 262143 --model_type bpe --model_prefix poolside0.sp --new_line_delim 0 --normalization_rule_name nfkc_code --normalization_report_file normalization.txt --num_threads 96 --code_block_end=2 --code_meta_block_begin=3 --code_meta_block_end=4 --cache_sentence_frequencies_file=sp_cache_merged.bin 2>&1 | tee log.full
```

`poolside0.sp.model` and `poolside0.sp.vocab` should be generated.

Remember to shutdown the bpe-train-master AWS EC2 instance.
