// Tencent is pleased to support the open source community by making TNN available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "tnn/device/x86/acc/convolution/x86_conv_3d_layer_common.h"
#include "tnn/device/x86/acc/compute/x86_compute.h"
#include "tnn/device/x86/x86_context.h"
#include "tnn/utils/data_type_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>

namespace TNN_NS {
/*
X86ConvLayerCommonas as the last solution, always return true
handle the case group != 1, dilate != 1, any pads and strides
*/
bool X86Conv3DLayerCommon::isPrefered(ConvLayerParam *param, const std::vector<Blob *> &inputs,
                                    const std::vector<Blob *> &outputs) {
    return true;
}

X86Conv3DLayerCommon::~X86Conv3DLayerCommon() {}

Status X86Conv3DLayerCommon::Reshape(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    return TNN_OK;
}

Status X86Conv3DLayerCommon::allocateBufferWeight(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    ConvLayerParam *param = dynamic_cast<ConvLayerParam *>(param_);
    CHECK_PARAM_NULL(param);
    ConvLayerResource *conv_res = dynamic_cast<ConvLayerResource *>(resource_);
    CHECK_PARAM_NULL(conv_res);

    auto input       = inputs[0];
    auto output      = outputs[0];
    //{1, 1, 5, 5, 5}
    auto dims_input  = input->GetBlobDesc().dims;
    // {1, 1, 3, 3, 3}
    auto dims_output = output->GetBlobDesc().dims; 

    if (!buffer_weight_.GetBytesSize()) {
        int k_c = conv_gemm_conf_.K_c_;
        int m_c = conv_gemm_conf_.M_c_;
        int n_block = conv_gemm_conf_.n_block_;

        // int K = dims_input[1] * param->kernels[0] * param->kernels[1] / param->group;
        // int M = dims_output[1] / param->group;
        int K = (dims_input[1] * param->kernels[0] * param->kernels[1]*param->kernels[2]) / param->group;
        int M = dims_output[1] / param->group;

        // std::cout << "K="<<K << std::endl;
        // std::cout << "M="<<M << std::endl;

        size_t weight_pack_per_group = ROUND_UP(K, k_c) * ROUND_UP(M, n_block);

        const float *src = conv_res->filter_handle.force_to<float *>();

        if (conv_res->filter_handle.GetDataType() == DATA_TYPE_FLOAT) {
            RawBuffer temp_buffer(weight_pack_per_group * param->group * sizeof(float));
            float *dst = temp_buffer.force_to<float *>();

            for (int g = 0; g < param->group; g++) {
                auto src_g = src + K * M * g;
                auto dst_g = dst + weight_pack_per_group * g;
                conv_pack_col_b_n(M, K, src_g, K, dst_g, conv_gemm_conf_);
            }

            temp_buffer.SetDataType(DATA_TYPE_FLOAT);
            buffer_weight_ = temp_buffer;
        } else {
            LOGE("Error: DataType %d not support\n", conv_res->filter_handle.GetDataType());
            return Status(TNNERR_MODEL_ERR, "conv_res DataType is not supported");
        }
    }
    return TNN_OK;
}

Status X86Conv3DLayerCommon::allocateBufferBias(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    ConvLayerParam *conv_param = dynamic_cast<ConvLayerParam *>(param_);
    CHECK_PARAM_NULL(conv_param);
    ConvLayerResource *conv_res = dynamic_cast<ConvLayerResource *>(resource_);
    CHECK_PARAM_NULL(conv_res);

    if (!buffer_bias_.GetBytesSize()) {
        auto dims_output = outputs[0]->GetBlobDesc().dims;
        int total_byte_size = ROUND_UP(dims_output[1], 8) * DataTypeUtils::GetBytesSize(conv_res->bias_handle.GetDataType());
        RawBuffer temp_buffer(total_byte_size);
        if (conv_param->bias) {
            const int bias_handle_size    = conv_res->bias_handle.GetBytesSize();
            const float *bias_handle_data = conv_res->bias_handle.force_to<float *>();
            memcpy(temp_buffer.force_to<float *>(), conv_res->bias_handle.force_to<float *>(), bias_handle_size);
        }
        buffer_bias_ = temp_buffer;
    }
    return TNN_OK;
}

Status X86Conv3DLayerCommon::Init(Context *context, LayerParam *param, LayerResource *resource,
                                const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    auto status = X86LayerAcc::Init(context, param, resource, inputs, outputs);
    if (status != TNN_OK) {
        return status;
    }
    conv_gemm_conf_ = conv_gemm_config<float, float, float>();

    RETURN_ON_NEQ(allocateBufferWeight(inputs, outputs), TNN_OK);
    RETURN_ON_NEQ(allocateBufferBias(inputs, outputs), TNN_OK);

    return TNN_OK;
}

Status X86Conv3DLayerCommon::DoForward(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    Blob *input_blob    = inputs[0];
    Blob *output_blob   = outputs[0];
    auto input_dims     = inputs[0]->GetBlobDesc().dims;
    auto output_dims    = outputs[0]->GetBlobDesc().dims;
    void *input_ptr     = input_blob->GetHandle().base;
    void *output_ptr    = output_blob->GetHandle().base;
    auto param = dynamic_cast<ConvLayerParam *>(param_);
    auto resource = dynamic_cast<ConvLayerResource *>(resource_);
  
    int conv_in_offset_ = input_dims[1] * input_dims[2] * input_dims[3]*input_dims[4]; //c*d*w*h 输入数据量
    int conv_out_spatial_dim_ = output_dims[2] * output_dims[3]*output_dims[4]; // d*w*h 单个通道输出的数据量
    int output_offset_ = output_dims[1] * conv_out_spatial_dim_ / param->group; //c_out*d*h*w /g 卷积层输出尺寸
    //c_in * k_w * k_h * k_d * out_h * out_w * out_d / g  输入特征图转化为矩阵的尺寸
    size_t col_offset_ =  param->kernels[0] * param->kernels[1] * param->kernels[2] *output_dims[2] * output_dims[3] * output_dims[4] * (input_dims[1] / param->group);


    //===============================================================
    int max_num_threads = OMP_MAX_THREADS_NUM_;
    conv_ajust_m_blk_size(max_num_threads, conv_out_spatial_dim_, conv_gemm_conf_.M_c_);

    int m_c = conv_gemm_conf_.M_c_;  //64
    int k_c = conv_gemm_conf_.K_c_;  //256
    int n_block = conv_gemm_conf_.n_block_; //6
    size_t src_trans_size = m_c * k_c;  //64*256 

    size_t im2col_size = ROUND_UP(col_offset_ * param->group * sizeof(float), 32); //作im2col所需要的内存
    size_t workspace_size = (im2col_size + ROUND_UP(src_trans_size * max_num_threads * sizeof(float), 32));
    float *workspace = reinterpret_cast<float *>(context_->GetSharedWorkSpace(workspace_size));

    float *im2col_workspace = workspace;
    float *src_trans_workspace = workspace + im2col_size / sizeof(float);
    //===============================================================

    // 3d kernel matrix A ： M*K = c_out * (c_in*k_h*k_w*k_d) /g^2 
    // 3d input  matrix B ： K*N = (c_in*k_h*k_w*k_d) * out_w * out_h*out_d
    int K = (input_dims[1] * param->kernels[0] * param->kernels[1]*param->kernels[2]) / param->group; //c_in * k_h*k_w*k_d /g
    int M = output_dims[1] / param->group; //c_out/g
    int N = conv_out_spatial_dim_; //out_d * out_w * out_h
    size_t weight_offset_per_group = ROUND_UP(K, k_c) * ROUND_UP(M, n_block);

    if (outputs[0]->GetBlobDesc().data_type == DATA_TYPE_FLOAT) {
        auto input_data = static_cast<float*>(input_ptr);
        auto output_data = static_cast<float*>(output_ptr);
        auto weights_data = buffer_weight_.force_to<float*>();
        float *bias_data  = buffer_bias_.force_to<float*>();

        for (size_t b = 0; b < outputs[0]->GetBlobDesc().dims[0]; b++) {
            //input:NCDHW
            //param:[w h d]
            // X86_VID2COL(float* src, int channel, int depth,int height, int width,int kernelh, int kernelw, int kerneld, int padl, int padr,
            //       int padt, int padb,int padf,int padback ,int strideh, int stridew, int strided,int dilationh, int dilationw, int dilationd,float* dst)
            X86_VID2COL(input_data + b * conv_in_offset_, input_dims[1],
                        input_dims[2], input_dims[3],input_dims[4],
                        param->kernels[1], param->kernels[0],param->kernels[2],
                        param->pads[0], param->pads[1],
                        param->pads[2], param->pads[3],
                        param->pads[4], param->pads[5],
                        param->strides[1], param->strides[0],param->strides[2],
                        param->dialations[1], param->dialations[0],param->dialations[2],
                        im2col_workspace);
            //——————-------------------------------------------------------
            // 3d kernel matrix A ： M*K = c_out * (c_in*k_h*k_w*k_d) /g^2 
            // 3d input  matrix B ： K*N = (c_in*k_h*k_w*k_d) * out_w * out_h*out_d
            for (int g = 0; g < param->group; g++) {
                conv_sgemm_nn_col_major_prepack_b(
                    N, M, K,
                    im2col_workspace + col_offset_ * g, N,
                    weights_data + weight_offset_per_group * g, K,
                    output_data + (b * param->group + g) * output_offset_, N,
                    bias_data + g * param->output_channel / param->group,
                    param->activation_type, src_trans_workspace, conv_gemm_conf_);
            }
  
        }
    } else {
        return Status(TNNERR_DEVICE_ACC_DATA_FORMAT_NOT_SUPPORT, "Error: x86 device not support this data type");
    }

    return TNN_OK;
}
}  // namespace TNN_NS
