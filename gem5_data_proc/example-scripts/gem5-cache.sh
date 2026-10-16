# set -x

ulimit -n 4096

export PYTHONPATH=`pwd`


mkdir -p results

example_stats_dirs=(
/nfs-nvme/home/share/zyy/gem5-results/mutipref-replay-merge-tlb-pref
)
jsons=(
./simpoint_cpt/resources/spec06_rv64gcb_o2_20m.json
)

len=${#jsons[@]}

for (( i=0; i<$len; i++ ))
do
    example_stats_dir=${example_stats_dirs[i]}
    tag=$(basename $example_stats_dir)
    python3 batch.py -s $example_stats_dir -o results/$tag.csv \
        --cache

    echo $tag
    python3 simpoint_cpt/compute_weighted.py \
        -r results/$tag.csv \
        -j ${jsons[i]} \
        -o results/$tag-weighted.csv
done
