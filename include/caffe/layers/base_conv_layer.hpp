#ifndef CAFFE_BASE_CONVOLUTION_LAYER_HPP_
#define CAFFE_BASE_CONVOLUTION_LAYER_HPP_

#include <vector>

#include "caffe/blob.hpp"
#include "caffe/layer.hpp"
//#include "caffe/proto/caffe.pb.h"
#include "caffe/util/im2col.hpp"

extern "C"
{
#include "caffe/util/swim2col.h"
#include "caffe/util/sw_batchsize_im2col.h"
}
namespace caffe {

/**
 * @brief Abstract base class that factors out the BLAS code common to
 *        ConvolutionLayer and DeconvolutionLayer.
 */
template <typename Dtype>
class BaseConvolutionLayer : public Layer<Dtype> {
 public:
  explicit BaseConvolutionLayer(const LayerParameter& param)
      : Layer<Dtype>(param) {}
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);

  virtual inline int MinBottomBlobs() const { return 1; }
  virtual inline int MinTopBlobs() const { return 1; }
  virtual inline bool EqualNumBottomTopBlobs() const { return true; }
  inline const Blob<int>& kernel_shape() const { return kernel_shape_; }

 protected:
  // Helper functions that abstract away the column buffer and gemm arguments.
  // The last argument in forward_cpu_gemm is so that we can skip the im2col if
  // we just called weight_cpu_gemm with the same input.
  void forward_cpu_gemm(const Dtype* input, const Dtype* weights,
      Dtype* output, bool skip_im2col = false);
  void forward_cpu_bias(Dtype* output, const Dtype* bias);
  void backward_cpu_gemm(const Dtype* input, const Dtype* weights,
      Dtype* output);
  void weight_cpu_gemm(const Dtype* input, const Dtype* output, Dtype*
      weights);
  void backward_cpu_bias(Dtype* bias, const Dtype* input);

#ifdef USE_CONV
  void forward_batchsize_cpu_gemm(const int num,const int offset,const Dtype* input,
    const Dtype* weights, Dtype* output, bool skip_im2col=false) ;
  void forward_batchsize_cpu_bias(const int num,Dtype* output,
    const Dtype* bias) ;
  void backward_batchsize_cpu_gemm(const int num,const int offset,const Dtype* input, const Dtype* weights,
      Dtype* output);
  void batchsize_weight_cpu_gemm(const int num,const int offset,const Dtype* input, const Dtype* output, Dtype*
      weights);
  void backward_batchsize_cpu_bias(const int num,Dtype* bias, const Dtype* input);
#endif
#ifndef CPU_ONLY
  void forward_gpu_gemm(const Dtype* col_input, const Dtype* weights,
      Dtype* output, bool skip_im2col = false);
  void forward_gpu_bias(Dtype* output, const Dtype* bias);
  void backward_gpu_gemm(const Dtype* input, const Dtype* weights,
      Dtype* col_output);
  void weight_gpu_gemm(const Dtype* col_input, const Dtype* output, Dtype*
      weights);
  void backward_gpu_bias(Dtype* bias, const Dtype* input);
#endif

  /// @brief The spatial dimensions of the input.
  inline int input_shape(int i) {
    return (*bottom_shape_)[channel_axis_ + i];
  }
  // reverse_dimensions should return true iff we are implementing deconv, so
  // that conv helpers know which dimensions are which.
  virtual bool reverse_dimensions() = 0;
  // Compute height_out_ and width_out_ from other parameters.
  virtual void compute_output_shape() = 0;

  /// @brief The spatial dimensions of a filter kernel.
  Blob<int> kernel_shape_;
  /// @brief The spatial dimensions of the stride.
  Blob<int> stride_;
  /// @brief The spatial dimensions of the padding.
  Blob<int> pad_;
  /// @brief The spatial dimensions of the dilation.
  Blob<int> dilation_;
  /// @brief The spatial dimensions of the convolution input.
  Blob<int> conv_input_shape_;
  /// @brief The spatial dimensions of the col_buffer.
  vector<int> col_buffer_shape_;
  /// @brief The spatial dimensions of the output.
  vector<int> output_shape_;
  const vector<int>* bottom_shape_;

  int num_spatial_axes_;
  int bottom_dim_;
  int top_dim_;

  int channel_axis_;
  int num_;
  int channels_;
  int group_;
  int out_spatial_dim_;
  int weight_offset_;
  int num_output_;
  bool bias_term_;
  bool is_1x1_;
  bool force_nd_im2col_;
  double im2col_time,col2im_time;
  double forward_cpu_gemm_time,forward_cpu_bias_time;
  double backward_cpu_gemm_time,backward_cpu_bias_time,weight_cpu_gemm_time;

#ifdef USE_4CG
  Dtype* tmp_bias_diff[NThread-1];
  Dtype* tmp_weight_diff[NThread-1];
  Blob<Dtype> col_buffers_[NThread];
#endif
 private:
  // wrap im2col/col2im so we don't have to remember the (long) argument lists
#ifdef USE_CONV
  inline void conv_im2col_batchsize_cpu(const int num,const int group,const Dtype* data, Dtype* col_buff) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
#ifdef USE_SWIM2COL
      if(sizeof(Dtype) == sizeof(float))
      {
        swim2col_batchsize_f((const float*)data, num,group,conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (float*)col_buff);
      }
      else
      {
        swim2col_batchsize_d((const double*)data,num,group,conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (double*)col_buff);

      }
#else
      im2col_cpu(data,num* conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], col_buff);
#endif
    } else {
      LOG(INFO)<<"forward im2col_nd_cpu not implementing!!!.";
      /*im2ddcol_nd_cpu(data, num_spatial_axes_, conv_input_shape_.cpu_data(),
          col_buffer_shape_.data(), kernel_shape_.cpu_data(),
          pad_.cpu_data(), stride_.cpu_data(), dilation_.cpu_data(), col_buff);
      */
    }
  }
  inline void conv_col2im_batchsize_cpu(const int num,const int group,const Dtype* col_buff, Dtype* data) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
#ifdef USE_SWIM2COL
      if(sizeof(Dtype) == sizeof(float))
      {
         swcol2im_batchsize_f((const float*)col_buff,num,group, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1],(float*) data);
        
      }
      else
      {  
        swcol2im_batchsize_d((double*)col_buff,num,group, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (double*)data);

      }
//LOG(INFO)<<"backward col2im channels_:"<<conv_in_channels_<<" height: "<<conv_input_shape_.cpu_data()[1]
//  <<" width: "<<conv_input_shape_.cpu_data()[2]<<" kernel_h:"<<kernel_shape_.cpu_data()[0]
//  <<" kernel_w: "<<kernel_shape_.cpu_data()[1] << " pad_h: "<<pad_.cpu_data()[0]<<" pad_w:"<<pad_.cpu_data()[1]
//  <<" stride_h: "<<stride_.cpu_data()[0]<<" stride_w: "<<stride_.cpu_data()[1]
//  <<" dilation_h: "<<dilation_.cpu_data()[0]<<" dilation_w: "<<dilation_.cpu_data()[1];
#else
      col2im_cpu(col_buff, num*conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], data);
#endif
    } else {
      col2im_nd_cpu(col_buff, num_spatial_axes_, conv_input_shape_.cpu_data(),
          col_buffer_shape_.data(), kernel_shape_.cpu_data(),
          pad_.cpu_data(), stride_.cpu_data(), dilation_.cpu_data(), data);
    }
  }
#endif

  inline void conv_im2col_cpu(const Dtype* data, Dtype* col_buff) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
#ifdef USE_SWIM2COL
//LOG(INFO)<<"forward im2col channels_:"<<conv_in_channels_<<" height: "<<conv_input_shape_.cpu_data()[1]
//  <<" width: "<<conv_input_shape_.cpu_data()[2]<<" kernel_h:"<<kernel_shape_.cpu_data()[0]
//  <<" kernel_w: "<<kernel_shape_.cpu_data()[1] << " pad_h: "<<pad_.cpu_data()[0]<<" pad_w:"<<pad_.cpu_data()[1]
//  <<" stride_h: "<<stride_.cpu_data()[0]<<" stride_w: "<<stride_.cpu_data()[1]
//  <<" dilation_h: "<<dilation_.cpu_data()[0]<<" dilation_w: "<<dilation_.cpu_data()[1];

      if(sizeof(Dtype) == sizeof(float))
      {
        swim2col_f((const float*)data, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (float*)col_buff);
      }
      else
      {
        swim2col_d((const double*)data, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (double*)col_buff);

      }
      //LOG(INFO)<<"forward im2col OK.";
#else
      im2col_cpu(data, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], col_buff);
#endif
    } else {
      im2col_nd_cpu(data, num_spatial_axes_, conv_input_shape_.cpu_data(),
          col_buffer_shape_.data(), kernel_shape_.cpu_data(),
          pad_.cpu_data(), stride_.cpu_data(), dilation_.cpu_data(), col_buff);
    }
  }
  inline void conv_col2im_cpu(const Dtype* col_buff, Dtype* data) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
#ifdef USE_SWIM2COL
      if(sizeof(Dtype) == sizeof(float))
      {
         swcol2im_f((const float*)col_buff, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1],(float*) data);
        
      }
      else
      {  
        swcol2im_d((double*)col_buff, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], (double*)data);

      }
//LOG(INFO)<<"backward col2im channels_:"<<conv_in_channels_<<" height: "<<conv_input_shape_.cpu_data()[1]
//  <<" width: "<<conv_input_shape_.cpu_data()[2]<<" kernel_h:"<<kernel_shape_.cpu_data()[0]
//  <<" kernel_w: "<<kernel_shape_.cpu_data()[1] << " pad_h: "<<pad_.cpu_data()[0]<<" pad_w:"<<pad_.cpu_data()[1]
//  <<" stride_h: "<<stride_.cpu_data()[0]<<" stride_w: "<<stride_.cpu_data()[1]
//  <<" dilation_h: "<<dilation_.cpu_data()[0]<<" dilation_w: "<<dilation_.cpu_data()[1];
#else
      col2im_cpu(col_buff, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], data);
#endif
    } else {
      col2im_nd_cpu(col_buff, num_spatial_axes_, conv_input_shape_.cpu_data(),
          col_buffer_shape_.data(), kernel_shape_.cpu_data(),
          pad_.cpu_data(), stride_.cpu_data(), dilation_.cpu_data(), data);
    }
  }
#ifndef CPU_ONLY
  inline void conv_im2col_gpu(const Dtype* data, Dtype* col_buff) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
      im2col_gpu(data, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], col_buff);
    } else {
      im2col_nd_gpu(data, num_spatial_axes_, num_kernels_im2col_,
          conv_input_shape_.gpu_data(), col_buffer_.gpu_shape(),
          kernel_shape_.gpu_data(), pad_.gpu_data(),
          stride_.gpu_data(), dilation_.gpu_data(), col_buff);
    }
  }
  inline void conv_col2im_gpu(const Dtype* col_buff, Dtype* data) {
    if (!force_nd_im2col_ && num_spatial_axes_ == 2) {
      col2im_gpu(col_buff, conv_in_channels_,
          conv_input_shape_.cpu_data()[1], conv_input_shape_.cpu_data()[2],
          kernel_shape_.cpu_data()[0], kernel_shape_.cpu_data()[1],
          pad_.cpu_data()[0], pad_.cpu_data()[1],
          stride_.cpu_data()[0], stride_.cpu_data()[1],
          dilation_.cpu_data()[0], dilation_.cpu_data()[1], data);
    } else {
      col2im_nd_gpu(col_buff, num_spatial_axes_, num_kernels_col2im_,
          conv_input_shape_.gpu_data(), col_buffer_.gpu_shape(),
          kernel_shape_.gpu_data(), pad_.gpu_data(), stride_.gpu_data(),
          dilation_.gpu_data(), data);
    }
  }
#endif

  int num_kernels_im2col_;
  int num_kernels_col2im_;
  int conv_out_channels_;
  int conv_in_channels_;
  int conv_out_spatial_dim_;
  int kernel_dim_;
  int col_offset_;
  int output_offset_;
  Blob<Dtype> col_buffer_;
  Blob<Dtype> bias_multiplier_;
};

}  // namespace caffe

#endif  // CAFFE_BASE_CONVOLUTION_LAYER_HPP_
