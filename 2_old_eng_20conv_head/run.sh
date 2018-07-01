export CUDA_VISIBLE_DEVICES=2
#export PYTHONHOME=/home/vis/anaconda
export PYTHONHOME=./anaconda
BATCH_SIZE=32
nohup \
./paddle_trainer_warpctc \
--config=./batch_32/conf/sample_trainer.conf \
--use_gpu=1 \
--dot_period=$[64/BATCH_SIZE] \
--log_period=$[10000/BATCH_SIZE] \
--test_period=$[100000/BATCH_SIZE] \
--local=1 \
--save_dir=./pre_train/result \
--start_pass=1 \
--num_passes=100 \
--trainer_count=1 \
--gpu_id=0 \
--test_all_data_in_one_period=1 \
--saving_period_by_batches=$[100000/BATCH_SIZE] > ./train.log &
