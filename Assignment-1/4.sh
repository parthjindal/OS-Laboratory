mkdir -p files_mod
for FILE in data1d/*; 
do 
    f="$(basename -- $FILE)"
    awk 'BEGIN{i=1} /.*/{printf "%d % s\n",i,$0; i++}' data1d/$f | sed 's/[[:blank:]][[:blank:]]*/,/g' > files_mod/$f
done