N=$1
ans=()
for ((i = 2; i <= $N; i++))
do while (($N % $i == 0))
do ans+=($i)
   N=$(($N / $i))
   done
done
echo ${ans[@]}
