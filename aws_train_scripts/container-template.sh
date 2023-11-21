set -e
set -x

INDEX=$1
FILE=$2

export AWS_ACCESS_KEY_ID="<AWS_ACCESS_KEY_ID>"
export AWS_SECRET_ACCESS_KEY="<AWS_SECRET_ACCESS_KEY>"
export AWS_SESSION_TOKEN="<AWS_SESSION_TOKEN>"
cd /host
aws s3 cp $FILE chunk.txt.zst --quiet
zstd --decompress chunk.txt.zst

cd /sentencepiece

./build/src/spm_train --vocab_size 100000 --split_digits --byte_fallback --verbatim_control_char=1 --max_sentence_length 262143 --model_type bpe --model_prefix poolside$INDEX.sp --input /host/chunk.txt --new_line_delim 0 --normalization_rule_name nfkc_code --normalization_report_file /host/normalization$INDEX.txt --num_threads 96 --code_block_end=2 --code_meta_block_begin=3 --code_meta_block_end=4 --cache_sentence_frequencies_file=/host/sp_cache_$INDEX.bin 2>&1 | tee /host/log.$INDEX

cd /host
zstd sp_cache_$INDEX.bin

rm chunk.txt
