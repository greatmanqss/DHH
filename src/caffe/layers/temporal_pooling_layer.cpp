#include <cfloat>
#include <vector>
#include <fstream>
#include "caffe/layers/temporal_pooling_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe { 

template <typename Dtype>
void TemporalPoolingLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CHECK(this->layer_param().temporal_pooling_param().clips_num() > 0) <<
      "Temporal Pooling Layer takes at least one video clips.";
  op_ = this->layer_param_.temporal_pooling_param().operation(); 
  video_num_ = this->layer_param().temporal_pooling_param().clips_num();
}

template <typename Dtype>
void TemporalPoolingLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {

  top[0]->Reshape(video_num_, 1, 1, bottom[0]->channels()*bottom[0]->height()*bottom[0]->width());//qss
  top[1]->Reshape(video_num_,1,1,1);
  start_end_idx_.Reshape(video_num_,2,1,1);
  // If max operation, we will initialize the vector index part.
  if (this->layer_param_.temporal_pooling_param().operation() ==
      TemporalPoolingParameter_TemPoolOp_MAX && top.size() == 2) {
       max_idx_.Reshape(top[0]->shape());
  }
  if(top.size()>2){
	  top[2]->Reshape(video_num_,bottom[0]->channels(),bottom[0]->height(),bottom[0]->width());
	  top[3]->Reshape(video_num_,1,1,1);
  }
}

template <typename Dtype>
void TemporalPoolingLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    Dtype first_label = bottom[1]->cpu_data()[1];//qss
    Dtype* seidx_data = start_end_idx_.mutable_cpu_data();
    Dtype* label_data = top[1]->mutable_cpu_data();
    label_data[0] = bottom[1]->cpu_data()[0];//qss
    seidx_data[0] = 0;
    int clip_count = 1;
	//fstream output;
	//output.open("/home/data/qiaoshishi/test2.txt",ios::out|ios::app);
	//output<<first_label<<"\t"<<Dtype (int(first_label)%1000)<<"\t"<<Dtype (int(first_label)/1000)<<std::endl;
    for(int i = 1; i < bottom[1]->num(); i++)
  {
	  seidx_data[clip_count*2-1] = i;
      if(bottom[1]->cpu_data()[i*2+1] != first_label)
      {
		  seidx_data[clip_count*2-1] = (top.size()>2)? (i-clip_count):(i-1);
		  if(top.size()>2){
			  top[3]->mutable_cpu_data()[clip_count-1] = bottom[1]->cpu_data()[i*2];
		  }
          if (clip_count < video_num_)
          {     
                seidx_data[clip_count*2] = seidx_data[clip_count*2-1]+1;
				if(top.size()>2){
					i = i + 1;
				}
                first_label = bottom[1]->cpu_data()[i*2+1];
                label_data[clip_count] = bottom[1]->cpu_data()[i*2];
				//output<<bottom[1]->cpu_data()[i*2+1]<<"\t"<<bottom[1]->cpu_data()[i*2]<<"\t"<<label_data[clip_count]<<std::endl;
                clip_count ++;
				
          }
          else
          {
                break;
          }
      }
  }
  //output.close();
  
  int* mask = NULL;
  const Dtype* bottom_data_a = NULL;
  const Dtype* bottom_data_b = NULL;
  // dim of frame-level features
  const int count = bottom[0]->channels()*bottom[0]->height()*bottom[0]->width();
  Dtype* top_data = top[0]->mutable_cpu_data();
  switch (op_) {
  case TemporalPoolingParameter_TemPoolOp_AVG:
    caffe_set(count*video_num_, Dtype(0), top_data);
    // avg pooling for each clip
    for(int clip_id = 0; clip_id < video_num_; clip_id++)
    {
        int start = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2]+clip_id):start_end_idx_.cpu_data()[clip_id*2];
        int end = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2+1]+clip_id):start_end_idx_.cpu_data()[clip_id*2+1];
        Dtype coeff = 1.0 / (end - start + 1);
        int offset_top = top[0]->offset(clip_id);
        for (int frame_id = start; frame_id <= end; frame_id++)
        {
            int offset_bottom = bottom[0]->offset(frame_id);
            caffe_axpy(count, coeff, bottom[0]->cpu_data()+offset_bottom, top_data+offset_top);
        }
		if(top.size()>2){
			int offset_bottom_still = bottom[0]->offset(end+1);
		    int offset_top_still = top[2]->offset(clip_id);
		    caffe_copy(count, bottom[0]->cpu_data()+offset_bottom_still, top[2]->mutable_cpu_data()+offset_top_still);
		}
		
    }
    break;
  case TemporalPoolingParameter_TemPoolOp_MAX:
    // Initialize
    mask = max_idx_.mutable_cpu_data();
    caffe_set(count*video_num_, -1, mask);
    caffe_set(count*video_num_, Dtype(-FLT_MAX), top_data);
    for(int clip_id = 0; clip_id < video_num_; clip_id++)
    {
        int start = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2]+clip_id):start_end_idx_.cpu_data()[clip_id*2];
        int end = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2+1]+clip_id):start_end_idx_.cpu_data()[clip_id*2+1];
        int offset_top = top[0]->offset(clip_id);

        int offset_data_a = bottom[0]->offset(start);
        int offset_data_b = bottom[0]->offset(start+1);
        bottom_data_a = bottom[0]->cpu_data()+offset_data_a;
        bottom_data_b = bottom[0]->cpu_data()+offset_data_b;
        for (int idx = 0; idx < count; ++idx) {
          if (bottom_data_a[idx] > bottom_data_b[idx]) {
            top_data[idx+offset_top] = bottom_data_a[idx];  // maxval
            mask[idx+offset_top] = start;  // maxid
          } else {
            top_data[idx+offset_top] = bottom_data_b[idx];  // maxval
            mask[idx+offset_top] = start + 1;  // maxid
          }
        }
        for (int frame_id = start+2; frame_id <= end; frame_id++)
        {
            int offset_bottom = bottom[0]->offset(frame_id);
            bottom_data_b = bottom[0]->cpu_data()+offset_bottom;
            for (int idx = 0; idx < count; ++idx) {
              if (bottom_data_b[idx] > top_data[idx+offset_top]) {
                top_data[idx+offset_top] = bottom_data_b[idx];  // maxval 
                mask[idx+offset_top] = frame_id;  // maxid
              }
            }
        }
		if(top.size()>2){
			int offset_bottom_still = bottom[0]->offset(end+1);
		    int offset_top_still = top[2]->offset(clip_id);
		    caffe_copy(count, bottom[0]->cpu_data()+offset_bottom_still, top[2]->mutable_cpu_data()+offset_top_still);
		}
    }
    break;
  default:
    LOG(FATAL) << "Unknown temporal pooling operation.";
  }
}

template <typename Dtype>
void TemporalPoolingLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  const int* mask = NULL;
  const int count = bottom[0]->channels()*bottom[0]->height()*bottom[0]->width();
  const Dtype* top_diff = top[0]->cpu_diff();
  for (int i = 0; i < bottom.size(); ++i) {
    if (propagate_down[i]) {
      Dtype* bottom_diff = bottom[i]->mutable_cpu_diff();
      caffe_set(bottom[i]->count(), Dtype(0), bottom_diff);
      switch (op_) {
      case TemporalPoolingParameter_TemPoolOp_AVG:
          for(int clip_id = 0; clip_id < video_num_; clip_id++)
          {
              int start = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2]+clip_id):start_end_idx_.cpu_data()[clip_id*2];
              int end = (top.size()>2)? (start_end_idx_.cpu_data()[clip_id*2+1]+clip_id):start_end_idx_.cpu_data()[clip_id*2+1];
              Dtype coeff = 1.0 / (end - start + 1);
              int offset_top = top[0]->offset(clip_id);
              for (int frame_id = start; frame_id <= end; frame_id++)
              {
                  int offset_bottom = bottom[i]->offset(frame_id);
                  caffe_cpu_scale(count, coeff, top_diff+offset_top, bottom_diff+offset_bottom);
              }
			  if(top.size()>2){
				  
			      int offset_bottom_still = bottom[i]->offset(end+1); 
		          int offset_top_still = top[2]->offset(clip_id);
		          caffe_copy(count, top[2]->cpu_diff() + offset_top_still, bottom_diff+offset_bottom_still);
		      }
          }
        break;
      case TemporalPoolingParameter_TemPoolOp_MAX:
        mask = max_idx_.cpu_data();
        for(int clip_id = 0; clip_id < video_num_; clip_id++)
        {			
            int offset_top = top[0]->offset(clip_id);
            for (int index = 0; index < count; index++)
            {
                int frame_id = mask[index+offset_top];
                int offset_bottom = bottom[i]->offset(frame_id);
                bottom_diff[offset_bottom + index] = top_diff[offset_top + index];
            }
			if(top.size()>2){
				
				//int start = start_end_idx_.cpu_data()[clip_id*2]+clip_id;
                int end = start_end_idx_.cpu_data()[clip_id*2+1]+clip_id;
			    int offset_bottom_still = bottom[i]->offset(end+1);
		        int offset_top_still = top[2]->offset(clip_id);
		        caffe_copy(count, top[2]->cpu_diff() + offset_top_still, bottom_diff+offset_bottom_still);
		    }
        }
        break;
      default:
        LOG(FATAL) << "Unknown elementwise operation.";
      }
    }
  }
}

#ifdef CPU_ONLY
STUB_GPU(TemporalPoolingLayer);
#endif

INSTANTIATE_CLASS(TemporalPoolingLayer);
REGISTER_LAYER_CLASS(TemporalPooling);

}  // namespace caffe
