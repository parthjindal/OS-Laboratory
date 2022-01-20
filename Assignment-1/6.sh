[[ $# -ne 2 || $2<1 || $2>4 ]] && echo "USAGE: $0 <file_name> <col_no(1-4)>" && exit
[[ ! -f $1 ]] && echo "Cannot find mentioned file" && exit
cat "$1" | awk -v value=$2 '{print $value}'| sort -f | uniq -ic |  awk '{print tolower($2),$1}' | sort -rn -k 2 >> 1f_output_$2_column.freq