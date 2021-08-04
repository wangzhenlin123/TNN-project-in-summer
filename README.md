# TNN项目实战

3D Conv的x86和openvino实现


## openvino的3D Conv实现
具体做法：
在<path to TNN>/network/openvino/layer_builder中添加了一个conv3d_layer_builder.cc文件

  
  
## x86的3D Conv实现
具体做法：
    在source/tnn/device/x86/acc下添加：
   	  source/tnn/device/x86/acc/x86_conv_3d_layer_acc.h    （完成）
   	  source/tnn/device/x86/acc/x86_conv_3d_layer_acc.cc    （完成）
   上面文件主要调用：X86Conv3DLayerAccFactory，所以在source/tnn/device/x86/acc/convolution下添加：
   	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_acc_factory.h  （完成）
   	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_acc_factory.cc	 （完成）
 上面文件，仅实现FP常规卷积计算：X86Conv3DLayerCommon，所以在source/tnn/device/x86/acc/convolution下添加：
	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_common.h    （完成）
	  source/tnn/device/x86/acc/convolution/x86_conv_3d_layer_common.cc    (未完成)
  
  
