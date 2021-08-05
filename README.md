# TNN项目实战

3D Conv的x86和openvino实现


 
## 3D Conv的openvino实现
具体做法：
在<path to TNN>/network/openvino/layer_builder中添加了一个conv3d_layer_builder.cc文件

测试：
input:    1x3x16x224x224 
	
3d conv:  32x3x3x3x3
	
output:   1x32x16x112x112
	
TNN Benchmark time cost: min = 68.923   ms  |  max = 68.923   ms  |  avg = 68.923   ms	
  
  
## 3D Conv的x86实现
具体做法：
    在source/tnn/device/x86/acc下添加：
   	  source/tnn/device/x86/acc/x86_conv_3d_layer_acc.h    （完成）
   	  source/tnn/device/x86/acc/x86_conv_3d_layer_acc.cc   （完成）
	
   上面文件主要调用：X86Conv3DLayerAccFactory，所以在source/tnn/device/x86/acc/convolution下添加：
   	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_acc_factory.h  （完成）
   	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_acc_factory.cc	 （完成）
	
   上面文件，仅实现FP常规卷积计算：X86Conv3DLayerCommon，所以在source/tnn/device/x86/acc/convolution下添加：
	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_common.h    （完成）
	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_common.cc    (存在问题)
	
     在source/tnn/device/x86/acc/compute/x86_compute.cc添加函数X86_VID2COL的实现
     在source/tnn/device/x86/acc/compute/x86_compute.h添加X86_VID2COL的声明
   
   主要问题： x86 3D Conv算出来的数值不对。    
    
