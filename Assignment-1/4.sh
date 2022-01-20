mkdir -p files_mod
for FILE in temp/*
do  f="$(basename -- $FILE)"
    awk 'BEGIN{i=1} /.*/{printf "%d % s\n",i,$0; i++}' temp/$f | tr ' ' ',' > files_mod/$f
done