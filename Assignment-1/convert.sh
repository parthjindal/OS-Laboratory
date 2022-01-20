echo -n "b=\$'\u0020';a=\"" > $2
sed -ze 's/\n/;/g' -e 's/\$/\\$/g' -e 's/"/\\"/g' -e 's/[[:space:]]\+/\${b}/g' $1 >> $2
echo "\";eval \$a" >> $2