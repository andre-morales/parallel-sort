declare -i i=9000
set -e
while true
do
	./generate 10000 3 1mb.dat $i
	./ep 1mb.dat result.out 0
	./verify 1mb.dat q
	i+=1
done

