if [ $# != 1 ];then
echo "USAGE: $0 ./MYINPUT.TXT"
exit 1;
fi
taskid=`date '+%s'`
echo "taskid is " $taskid
mkdir $taskid
mkdir $taskid/imgs
find ../output/ -name "*.jpg" | xargs -i cp {} $taskid/imgs/
mkdir $taskid/labels
find ../output/ -name "*.label" | xargs -i cp {} $taskid/labels/
mkdir $taskid/txt
cp $1 $taskid/txt/text.txt
tar -cvf $taskid.tar $taskid
scp $taskid.tar work@cp01-sys-ens001.cp01.baidu.com:/home/work/liusheng/idlevaluate/htdocs/imageenhancement/ocrtool
echo "TrackUrl is: http://idlqa.baidu.com:8080/imageenhancement/?r=ocrdemo&taskid=$taskid"
