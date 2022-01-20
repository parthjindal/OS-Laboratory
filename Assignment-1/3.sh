INP_DIR="./data"
arr=($(find ${INP_DIR} -type f -name "*.*" | perl -ne 'print $1 if m/\.([^.\/]+)$/' | sort -u))
printf '%s\n' "${arr[@]}" | xargs -I {} mkdir -p ./Output/{}
for i in "${arr[@]}"
do find ${INP_DIR} -type f -name "*.$i" -exec mv -t ./Output/$i {} +
done
mkdir -p ./Output/Nil
find ${INP_DIR} -name "*" -type f -exec mv -t ./Output/Nil {} +
rm -rf ${INP_DIR}/*
mv ./Output/* ${INP_DIR}/
rm -rf ./Output