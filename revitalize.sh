start_time=$(date +%s)

bash clean.sh
bash build.sh

end_time=$(date +%s)
duration=$((end_time - start_time))
echo "Duration: $duration seconds"