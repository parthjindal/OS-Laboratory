DIR="./1.b.files"
OUT_DIR="./1.b.out"
if [ ! -d $OUT_DIR ] 
then mkdir $OUT_DIR
fi
ls $DIR | xargs -I {} -P 4 sort -n $DIR/{} -o $OUT_DIR/{}
sort -n -m $OUT_DIR/* | uniq -c | awk '{print $2,$1}' > $OUT_DIR/1.b.out.txt
