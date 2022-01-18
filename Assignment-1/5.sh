# Part-1: Set envirnment variables
REQ_HEADERS="traceparent,User-Agent"
VERBOSE=0
YELLOW='\033[1;33m'
NC='\033[0m'
if [ $# -gt 0 ]; then
	if [ $1 = "-v" ]; then
		VERBOSE=1
	fi
fi
function LOG() {
	if [[ $VERBOSE -eq 1 ]]; then
		echo -e "${YELLOW}[debug] $@${NC}"
	fi
}

# Part-2: Fetch the webpage and save HTML
curl -o example.html https://www.example.com/
if [ $? -ne 0 ]; then
	LOG "Failed to fetch the webpage"
	exit 1
fi

# Part-3: Print IP and response headers
curl -i http://ip.jsontest.com/
if [ $? -ne 0 ]; then
	LOG "Failed to fetch the IP"
	exit 1
fi

# Part-4: Print required headers from response
# Part-5: Download check validity of JSONs
arrIN=(${REQ_HEADERS//,/ })
LOG "REQ_HEADERS: [${arrIN[@]}]"
str=$(printf ".\"%s\", " "${arrIN[@]}")
str=${str::-2}
curl http://headers.jsontest.com/ | jq "$str"

# Part-5: Download check validity of JSONs
touch valid.txt invalid.txt
FILES=$(find ./JSON -name "*.json")
for i in $FILES; do
	is_json=$(curl -d "json=$(cat ${i})" -X POST http://validate.jsontest.com/ | jq '.validate')
	if [ "$is_json" == "true" ]; then
		LOG "JSON file: $i is valid"
		echo $(basename -- $i) >>valid.txt
	else
		LOG "JSON file: $i is invalid"
		echo $(basename -- $i) >>invalid.txt
	fi
done
sort -o valid.txt valid.txt
sort -o invalid.txt invalid.txt