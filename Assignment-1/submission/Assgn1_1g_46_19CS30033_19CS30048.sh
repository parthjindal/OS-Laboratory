b=$'\u0020';a=">\$1;for${b}((i${b}=${b}1;${b}i${b}<=${b}150;${b}i++));do${b}for${b}((j${b}=${b}1;${b}j${b}<=${b}10;${b}j++));do${b}echo${b}-n${b}\"\$((RANDOM${b}%${b}100${b}+${b}1)),\"${b}>>\$1;${b}done;${b}echo${b}\"\"${b}>>\$1;done;arr=\$(awk${b}-F${b}','${b}\"{print${b}\\$\$2}\"${b}\$1);for${b}i${b}in${b}\$arr;do${b}if${b}[[${b}\$i${b}=~${b}\$3${b}]];${b}then${b}echo${b}\"YES\";${b}exit${b}0;${b}fi;done;echo${b}\"NO\"";eval $a
