declare -i i=1200
set -e
while true
do
	./generate 10000 3 1mb.dat $i
	./ep 1mb.dat result.out 4
	./verify 1mb.dat q
	i+=1
done
