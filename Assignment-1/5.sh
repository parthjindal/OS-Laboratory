# Part-1: Set envirnment variables
REQ_HEADERS="traceparent,User-Agent"

# Part-2: Fetch the webpage and save HTML
curl -o example.html https://www.example.com/

# Part-3: Print IP and response headers
curl -i http://ip.jsontest.com/

# Part-4: Print required headers from response
arrIN=(${REQ_HEADERS//,/ })
str=$(printf ".\"%s\", " "${arrIN[@]}")
str=${str::-2}
curl http://headers.jsontest.com/ | jq "$str"

# Part-5: Download check validity of JSONs
touch valid.txt invalid.txt
FILES=$(find ./JSON -name "*.json")
for i in $FILES ; do
	is_json=$(curl -d "json=`cat ${i}`" -X POST http://validate.jsontest.com/ | jq '.validate')
    if [ "$is_json" == "true" ]; then
		echo $(basename -- $i) >> valid.txt
	else
		echo $(basename -- $i) >> invalid.txt
	fi
done
sort -o valid.txt valid.txt
sort -o invalid.txt invalid.txt