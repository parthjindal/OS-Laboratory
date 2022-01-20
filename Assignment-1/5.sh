REQ_HEADERS="traceparent,User-Agent"
VERBOSE=0
YELLOW='\033[1;33m'
NC='\033[0m'
if [ $# -gt 0 ]
then if [ $1 = "-v" ] 
then VERBOSE=1
	fi
fi
function LOG() { if [[ $VERBOSE -eq 1 ]]
	then echo -e "${YELLOW}[debug] $@${NC}"
	fi
}
curl -o example.html https://www.example.com/
if [ $? -ne 0 ]
then LOG "Failed to fetch the webpage"
	exit 1
fi
curl -i http://ip.jsontest.com/
if [ $? -ne 0 ]
then LOG "Failed to fetch the IP"
	exit 1
fi
arrIN=(${REQ_HEADERS//,/ })
LOG "REQ_HEADERS: [${arrIN[@]}]"
str=$(printf ".\"%s\", " "${arrIN[@]}")
curl http://headers.jsontest.com/ | jq "${str::-2}"
> valid.txt
> invalid.txt
FILES=$(find ./JSONData -name "*.json")
for i in $FILES
do is_json=$(curl -d "json=$(cat ${i})" -X POST http://validate.jsontest.com/ | jq '.validate')
	if [ "$is_json" == "true" ]
	then LOG "JSON file: $i is valid"
		echo $(basename -- $i) >>valid.txt
	else LOG "JSON file: $i is invalid"
		echo $(basename -- $i) >>invalid.txt
	fi
done
sort -o valid.txt valid.txt
sort -o invalid.txt invalid.txt