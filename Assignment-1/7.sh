>$1
for ((i = 1; i <= 150; i++))
do for ((j = 1; j <= 10; j++))
do echo -n "$((RANDOM % 100 + 1))," >>$1
    done
    echo "" >>$1
done
arr=$(awk -F ',' "{print \$$2}" $1)
for i in $arr
do  echo $i
    if [[ $i =~ $3 ]]
    then echo "YES"
         exit 0
    fi
done
echo "NO"