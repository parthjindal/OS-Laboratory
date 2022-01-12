mapfile -d $'\0' array < <(find ./Input -name '*.*' -type f -print0 | sed -z 's/.*\.//g')
declare -A b
for i in "${array[@]}"; do b["$i"]=1; done

for i in "${!b[@]}"
do
    echo $i
    mkdir -p ./Output/"$i"
    find ./Input -name "*.$i" -type f -exec mv -t ./Output/"$i" {} +
done

mkdir -p ./Output/Nil
find ./Input -name "*" -type f -exec mv -t ./Output/Nil {} +