b=" ";
a="N=\$1;
ans=();
for$b((i=2;i<=\$N;i++));
do${b}
    while$b((\$N%\$i==0));
    do${b}
        ans+=(\$i);N=\$((\$N/\$i));
    done;
done;
echo$b\${ans[@]}";
eval $a