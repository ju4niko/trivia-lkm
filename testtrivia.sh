
retardo=0.5
GP=/sys/class/gpio
echo 49 > $GP/export
echo 48 > $GP/export
echo 20 > $GP/export

echo 44 > $GP/export
echo 45 > $GP/export
echo 47 > $GP/export

echo out > $GP/gpio49/direction
echo out > $GP/gpio48/direction
echo out > $GP/gpio20/direction

echo out > $GP/gpio44/direction
echo out > $GP/gpio45/direction
echo out > $GP/gpio47/direction

echo 1 > $GP/gpio49/value
sleep $retardo
echo 0 > $GP/gpio49/value
sleep $retardo

echo 1 > $GP/gpio48/value
sleep $retardo
echo 0 > $GP/gpio48/value
sleep $retardo

echo 1 > $GP/gpio20/value
sleep $retardo
echo 0 > $GP/gpio20/value
sleep $retardo



echo 1 > $GP/gpio44/value
sleep $retardo
echo 0 > $GP/gpio44/value
sleep $retardo

echo 1 > $GP/gpio45/value
sleep $retardo
echo 0 > $GP/gpio45/value
sleep $retardo

echo 1 > $GP/gpio47/value
sleep $retardo
echo 0 > $GP/gpio47/value


echo 49 > $GP/unexport
echo 48 > $GP/unexport
echo 20 > $GP/unexport

echo 44 > $GP/unexport
echo 45 > $GP/unexport
echo 47 > $GP/unexport

