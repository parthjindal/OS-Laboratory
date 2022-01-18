read -p "Enter File Name: " file
read -p "Enter Column Number: " col
read -p "Enter regex: " regex
>$file
for ((i = 1; i <= 150; i++)); do
    for ((j = 1; j <= 10; j++)); do
        echo -n "$((RANDOM % 100 + 1))," >>$file
    done
    echo "" >>$file
done
awk -F ',' "{print \$$col}" $file | grep -E "$regex"